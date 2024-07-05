/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <common_macros.h>
#include <app_priv.h>
#include <app_reset.h>
#include <bmi160_defs.h>

#include <stdio.h>
#include "lp_core_main.h"
#include "ulp_lp_core.h"
#include "lp_core_i2c.h"
#include "lp_core_uart.h"

#include <platform/ESP32/OpenthreadLauncher.h>

#include <app/server/CommissioningWindowManager.h>

#include <app/server/Server.h>


extern const uint8_t lp_core_main_bin_start[] asm("_binary_lp_core_main_bin_start");
extern const uint8_t lp_core_main_bin_end[]   asm("_binary_lp_core_main_bin_end");

static const char *TAG = "app_main";
uint16_t light_endpoint_id = 0;
uint16_t door_endpoint_id = 1;
uint16_t window_endpoint_id = 2;

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

constexpr auto k_timeout_seconds = 300;

int lp_status;
int lp_address_given;
esp_err_t lp_esp_err;

#if CONFIG_ENABLE_ENCRYPTED_OTA
extern const char decryption_key_start[] asm("_binary_esp_image_encryption_key_pem_start");
extern const char decryption_key_end[] asm("_binary_esp_image_encryption_key_pem_end");

static const char *s_decryption_key = decryption_key_start;
static const uint16_t s_decryption_key_len = decryption_key_end - decryption_key_start;
#endif // CONFIG_ENABLE_ENCRYPTED_OTA

static void lp_uart_init(void)
{
    lp_core_uart_cfg_t cfg;
    cfg.uart_pin_cfg.tx_io_num = GPIO_NUM_5; 
    cfg.uart_pin_cfg.rx_io_num = GPIO_NUM_4;
    cfg.uart_pin_cfg.rts_io_num = GPIO_NUM_2;
    cfg.uart_pin_cfg.cts_io_num = GPIO_NUM_3;
    cfg.uart_proto_cfg.baud_rate = 115200;
    cfg.uart_proto_cfg.data_bits = UART_DATA_8_BITS;
    cfg.uart_proto_cfg.parity = UART_PARITY_DISABLE;
    cfg.uart_proto_cfg.stop_bits = UART_STOP_BITS_1;
    cfg.uart_proto_cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.uart_proto_cfg.rx_flow_ctrl_thresh = 0;
    cfg.lp_uart_source_clk = LP_UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(lp_core_uart_init(&cfg));

    printf("LP UART initialized successfully\n");
}

static void lp_i2c_init(void)
{
    esp_err_t ret = ESP_OK;

    /* Initialize LP I2C with default configuration */
    lp_core_i2c_cfg_t i2c_cfg;
    i2c_cfg.i2c_pin_cfg.sda_io_num = GPIO_NUM_6;
    i2c_cfg.i2c_pin_cfg.scl_io_num = GPIO_NUM_7;
    i2c_cfg.i2c_pin_cfg.sda_pullup_en = true;
    i2c_cfg.i2c_pin_cfg.scl_pullup_en = true;
    i2c_cfg.i2c_timing_cfg.clk_speed_hz = 100000;
    i2c_cfg.i2c_src_clk = LP_I2C_SCLK_LP_FAST;

    ret = lp_core_i2c_master_init(LP_I2C_NUM_0, (const lp_core_i2c_cfg_t*)&i2c_cfg);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG,"LP I2C init failed\n");
        abort();
    }
    ESP_LOGI(TAG, "LP I2C initialized successfully\n");
}

static void lp_core_init(void)
{
    /* Set LP core wakeup source as the HP CPU */
    ulp_lp_core_cfg_t cfg = {
        .wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_LP_TIMER,
        .lp_timer_sleep_duration_us = 1000000,
    };

    /* Load LP core firmware */
    ESP_ERROR_CHECK(ulp_lp_core_load_binary(lp_core_main_bin_start, (lp_core_main_bin_end - lp_core_main_bin_start)));

    // set gpio high
    gpio_set_level(MP_GPIO_DRIVE, 1);

    /* Run LP core */
    ESP_ERROR_CHECK(ulp_lp_core_run(&cfg));

    // load variables
    bmi_160_register *shared_bmi_config = (bmi_160_register *)&ulp_shared_bmi_config;
    for (int i = 0; i < BMI_CONFIG_SIZE; i++){
        shared_bmi_config[i] = default_bmi_config[i];
    }
    uint8_t *shared_bmi_addresses = (uint8_t *)&ulp_shared_bmi_addresses;
        for (int i = 0; i < LP_NUM_ADDRESSES; i++){
        shared_bmi_addresses[i] = lp_addresses[i];
    }
    uint8_t *shared_num_bmi = (uint8_t *)&ulp_shared_num_bmi;
    *shared_num_bmi = LP_NUM_ADDRESSES;
    bmi_160_register *shared_register_masks = (bmi_160_register*)&ulp_shared_register_masks;
    shared_register_masks[0] = read_anymotion_tripped_mask;
    shared_register_masks[1] = read_low_g_tripped_mask;
    uint8_t *shared_num_masks = (uint8_t *)&ulp_shared_num_masks;
    *shared_num_masks = 2;

    uint8_t *shared_timers = (uint8_t*)&ulp_shared_timer_addresses;
    uint8_t addr_0 = TIMER_ADDRESS_0;
    uint8_t addr_1 = TIMER_ADDRESS_1;
    uint8_t addr_2 = TIMER_ADDRESS_2;
    shared_timers[0] = addr_0;
    shared_timers[1] = addr_1;
    shared_timers[2] = addr_2;

    uint8_t *shared_num_timers = (uint8_t *)&ulp_shared_num_timers;
    *shared_num_timers = 3;

    ulp_shared_deadman_threshold = LP_WATCHDOG_THRESH;
    volatile uint32_t *shared_watchdog_threshold = &ulp_shared_deadman_threshold;
    *shared_watchdog_threshold = LP_WATCHDOG_THRESH;

    printf("address0:%d, num:%d, mask0:%d, num:%d, timer0:%d, num:%d, dog:%d\n\r",
        (int)shared_bmi_addresses[0],
        (int)*shared_num_bmi,
        (int)shared_register_masks[0].reg,
        (int)*shared_num_masks,
        (int)shared_timers[0],
        (int)*shared_num_timers,
        (int)*shared_watchdog_threshold);

    // set gpio low
    gpio_set_level(MP_GPIO_DRIVE, 0);

    printf("LP core loaded with firmware and running successfully\n");
}

static void lp_msg_callback(void *arg, void *data){
    ESP_LOGI(TAG, "Message from LP");
    //drive pin high
    gpio_set_level(MP_GPIO_DRIVE, 1);
    // vTaskDelay(100000);
    // copy memory
    lp_status = (int)ulp_shared_out_status;
    lp_address_given = (int)ulp_shared_out_address;
    lp_esp_err = (esp_err_t)ulp_shared_esp_err;
    // drive pin low
    gpio_set_level(MP_GPIO_DRIVE, 0);
    // perform actions
    ESP_LOGI(TAG, "LP core says %d at %d", (int)lp_status, (int)lp_address_given);
    if (lp_status == -3){
        ESP_ERROR_CHECK_WITHOUT_ABORT(lp_esp_err);
        // esp_err_to_name(lp_esp_err));
    }
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        {
            ESP_LOGI(TAG, "Fabric removed successfully");
            if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0)
            {
                chip::CommissioningWindowManager & commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
                constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
                if (!commissionMgr.IsCommissioningWindowOpen())
                {
                    /* After removing last fabric, this example does not remove the Wi-Fi credentials
                     * and still has IP connectivity so, only advertising on DNS-SD.
                     */
                    CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(kTimeoutSeconds,
                                                    chip::CommissioningWindowAdvertisement::kDnssdOnly);
                    if (err != CHIP_NO_ERROR)
                    {
                        ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
                    }
                }
            }
        break;
        }

    case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved:
        ESP_LOGI(TAG, "Fabric will be removed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricUpdated:
        ESP_LOGI(TAG, "Fabric is updated");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
        ESP_LOGI(TAG, "Fabric is committed");
        break;

    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
        ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
        break;

    default:
        break;
    }
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing an LED or light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    esp_err_t err = ESP_OK;

    if (type == PRE_UPDATE) {
        /* Driver update */
        app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
        err = app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
    }

    return err;
}

extern "C" void app_main()
{
    esp_err_t err = ESP_OK;

    /* Initialize the ESP NVS layer */
    nvs_flash_init();

    /* Initialize LP_I2C from the main processor */
    lp_i2c_init();
    lp_uart_init();

    // gpio network
    app_driver_mp_gpio_init(lp_msg_callback);

    /* Load LP Core binary and start the coprocessor */
    // load_bmi_defaults();
    lp_core_init();

    /* Initialize driver */
    app_driver_handle_t light_handle = app_driver_light_init();
    app_driver_handle_t button_handle = app_driver_button_init();
    app_reset_button_register(button_handle);
    app_driver_handle_t door_button_handle = app_driver_door_init();

    /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    extended_color_light::config_t light_config;
    light_config.on_off.on_off = DEFAULT_POWER;
    light_config.on_off.lighting.start_up_on_off = nullptr;
    light_config.level_control.current_level = DEFAULT_BRIGHTNESS;
    light_config.level_control.lighting.start_up_current_level = DEFAULT_BRIGHTNESS;
    light_config.color_control.color_mode = (uint8_t)ColorControl::ColorMode::kColorTemperature;
    light_config.color_control.enhanced_color_mode = (uint8_t)ColorControl::ColorMode::kColorTemperature;
    light_config.color_control.color_temperature.startup_color_temperature_mireds = nullptr;
    endpoint_t *endpoint = extended_color_light::create(node, &light_config, ENDPOINT_FLAG_NONE, light_handle);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create extended color light endpoint"));
    light_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Light created with endpoint_id %d", light_endpoint_id);
    cluster_t *level_control_cluster = cluster::get(endpoint, LevelControl::Id);
    attribute_t *current_level_attribute = attribute::get(level_control_cluster, LevelControl::Attributes::CurrentLevel::Id);
    attribute::set_deferred_persistence(current_level_attribute);
    cluster_t *color_control_cluster = cluster::get(endpoint, ColorControl::Id);
    attribute_t *current_x_attribute = attribute::get(color_control_cluster, ColorControl::Attributes::CurrentX::Id);
    attribute::set_deferred_persistence(current_x_attribute);
    attribute_t *current_y_attribute = attribute::get(color_control_cluster, ColorControl::Attributes::CurrentY::Id);
    attribute::set_deferred_persistence(current_y_attribute);
    attribute_t *color_temp_attribute = attribute::get(color_control_cluster, ColorControl::Attributes::ColorTemperatureMireds::Id);
    attribute::set_deferred_persistence(color_temp_attribute);

    // Start my clusters
    contact_sensor::config_t door_config;
    endpoint_t *door_endpoint = contact_sensor::create(node, &door_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create door sensor endpoint"));
    door_endpoint_id = endpoint::get_id(door_endpoint);
    ESP_LOGI(TAG, "Door sensor created with endpoint_id %d", door_endpoint_id);

    contact_sensor::config_t window_config;
    endpoint_t *window_endpoint = contact_sensor::create(node, &window_config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(endpoint != nullptr, ESP_LOGE(TAG, "Failed to create window sensor endpoint"));
    window_endpoint_id = endpoint::get_id(window_endpoint);
    ESP_LOGI(TAG, "Window sensor created with endpoint_id %d", window_endpoint_id);



    /* Set OpenThread platform config */
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&config);


    /* Matter start */
    err = esp_matter::start(app_event_cb);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to start Matter, err:%d", err));

    /* Starting driver with default values */
    app_driver_light_set_defaults(light_endpoint_id);
    if (app_driver_get_button_state(door_button_handle)){
        app_driver_set_door_closed();
        ESP_LOGI(TAG, "Set door initially closed");
    } else {
        app_driver_set_door_opened();
        ESP_LOGI(TAG, "Set door initially open");
    }

#if CONFIG_ENABLE_ENCRYPTED_OTA
    err = esp_matter_ota_requestor_encrypted_init(s_decryption_key, s_decryption_key_len);
    ABORT_APP_ON_FAILURE(err == ESP_OK, ESP_LOGE(TAG, "Failed to initialized the encrypted OTA, err: %d", err));
#endif // CONFIG_ENABLE_ENCRYPTED_OTA

#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::wifi_register_commands();
#if CONFIG_OPENTHREAD_CLI
    esp_matter::console::otcli_register_commands();
#endif
    esp_matter::console::init();
#endif

}