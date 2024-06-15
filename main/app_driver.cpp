/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>
#include <esp_matter.h>
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

#include <app_priv.h>

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;
extern uint16_t door_endpoint_id;

/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_power(led_indicator_handle_t handle, esp_matter_attr_val_t *val)
{
#if CONFIG_BSP_LEDS_NUM > 0
    esp_err_t err = ESP_OK;
    if (val->val.b) {
        err = led_indicator_start(handle, BSP_LED_ON);
    } else {
        err = led_indicator_start(handle, BSP_LED_OFF);
    }
    return err;
#else
    ESP_LOGI(TAG, "LED set power: %d", val->val.b);
    return ESP_OK;
#endif
}

static esp_err_t app_driver_light_set_brightness(led_indicator_handle_t handle, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_BRIGHTNESS, STANDARD_BRIGHTNESS);
#if CONFIG_BSP_LEDS_NUM > 0
    return led_indicator_set_brightness(handle, value);
#else
    ESP_LOGI(TAG, "LED set brightness: %d", value);
    return ESP_OK;
#endif
}

static esp_err_t app_driver_light_set_hue(led_indicator_handle_t handle, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_HUE, STANDARD_HUE);
#if CONFIG_BSP_LEDS_NUM > 0
    led_indicator_ihsv_t hsv;
    hsv.value = led_indicator_get_hsv(handle);
    hsv.h = value;
    return led_indicator_set_hsv(handle, hsv.value);
#else
    ESP_LOGI(TAG, "LED set hue: %d", value);
    return ESP_OK;
#endif
}

static esp_err_t app_driver_light_set_saturation(led_indicator_handle_t handle, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_SATURATION, STANDARD_SATURATION);
#if CONFIG_BSP_LEDS_NUM > 0
    led_indicator_ihsv_t hsv;
    hsv.value = led_indicator_get_hsv(handle);
    hsv.s = value;
    return led_indicator_set_hsv(handle, hsv.value);
#else
    ESP_LOGI(TAG, "LED set saturation: %d", value);
    return ESP_OK;
#endif
}

static esp_err_t app_driver_light_set_temperature(led_indicator_handle_t handle, esp_matter_attr_val_t *val)
{
    uint32_t value = REMAP_TO_RANGE_INVERSE(val->val.u16, STANDARD_TEMPERATURE_FACTOR);
#if CONFIG_BSP_LEDS_NUM > 0
    return led_indicator_set_color_temperature(handle, value);
#else
    ESP_LOGI(TAG, "LED set temperature: %ld", value);
    return ESP_OK;
#endif
}

static void app_driver_button_toggle_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Toggle button pressed");
    uint16_t endpoint_id = light_endpoint_id;
    uint32_t cluster_id = OnOff::Id;
    uint32_t attribute_id = OnOff::Attributes::OnOff::Id;

    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = cluster::get(endpoint, cluster_id);
    attribute_t *attribute = attribute::get(cluster, attribute_id);

    esp_matter_attr_val_t val = esp_matter_invalid(NULL);
    attribute::get_val(attribute, &val);
    val.val.b = !val.val.b;
    attribute::update(endpoint_id, cluster_id, attribute_id, &val);
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;
    if (endpoint_id == light_endpoint_id) {
        led_indicator_handle_t handle = (led_indicator_handle_t)driver_handle;
        if (cluster_id == OnOff::Id) {
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                err = app_driver_light_set_power(handle, val);
            }
        } else if (cluster_id == LevelControl::Id) {
            if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
                err = app_driver_light_set_brightness(handle, val);
            }
        } else if (cluster_id == ColorControl::Id) {
            if (attribute_id == ColorControl::Attributes::CurrentHue::Id) {
                err = app_driver_light_set_hue(handle, val);
            } else if (attribute_id == ColorControl::Attributes::CurrentSaturation::Id) {
                err = app_driver_light_set_saturation(handle, val);
            } else if (attribute_id == ColorControl::Attributes::ColorTemperatureMireds::Id) {
                err = app_driver_light_set_temperature(handle, val);
            }
        }
    }
    return err;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    esp_err_t err = ESP_OK;
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    led_indicator_handle_t handle = (led_indicator_handle_t)priv_data;
    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = NULL;
    attribute_t *attribute = NULL;
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

    /* Setting brightness */
    cluster = cluster::get(endpoint, LevelControl::Id);
    attribute = attribute::get(cluster, LevelControl::Attributes::CurrentLevel::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_brightness(handle, &val);

    /* Setting color */
    cluster = cluster::get(endpoint, ColorControl::Id);
    attribute = attribute::get(cluster, ColorControl::Attributes::ColorMode::Id);
    attribute::get_val(attribute, &val);
    if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kCurrentHueAndCurrentSaturation) {
        /* Setting hue */
        attribute = attribute::get(cluster, ColorControl::Attributes::CurrentHue::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_hue(handle, &val);
        /* Setting saturation */
        attribute = attribute::get(cluster, ColorControl::Attributes::CurrentSaturation::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_saturation(handle, &val);
    } else if (val.val.u8 == (uint8_t)ColorControl::ColorMode::kColorTemperature) {
        /* Setting temperature */
        attribute = attribute::get(cluster, ColorControl::Attributes::ColorTemperatureMireds::Id);
        attribute::get_val(attribute, &val);
        err |= app_driver_light_set_temperature(handle, &val);
    } else {
        ESP_LOGE(TAG, "Color mode not supported");
    }

    /* Setting power */
    cluster = cluster::get(endpoint, OnOff::Id);
    attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_power(handle, &val);

    return err;
}

app_driver_handle_t app_driver_light_init()
{
#if CONFIG_BSP_LEDS_NUM > 0
    /* Initialize led */
    led_indicator_handle_t leds[CONFIG_BSP_LEDS_NUM];
    ESP_ERROR_CHECK(bsp_led_indicator_create(leds, NULL, CONFIG_BSP_LEDS_NUM));
    led_indicator_set_hsv(leds[0], SET_HSV(DEFAULT_HUE, DEFAULT_SATURATION, DEFAULT_BRIGHTNESS));
    
    return (app_driver_handle_t)leds[0];
#else
    return NULL;
#endif
}

app_driver_handle_t app_driver_button_init()
{
    /* Initialize button */
    button_handle_t btns[BSP_BUTTON_NUM];
    ESP_ERROR_CHECK(bsp_iot_button_create(btns, NULL, BSP_BUTTON_NUM));
    ESP_ERROR_CHECK(iot_button_register_cb(btns[0], BUTTON_PRESS_DOWN, app_driver_button_toggle_cb, NULL));
    
    return (app_driver_handle_t)btns[0];
}

void app_driver_lp_gpio_init(){
    rtc_gpio_init((gpio_num_t)LP_GPIO_DRIVE);
    rtc_gpio_set_direction((gpio_num_t)LP_GPIO_DRIVE, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_pulldown_dis((gpio_num_t)LP_GPIO_DRIVE);
    rtc_gpio_pullup_dis((gpio_num_t)LP_GPIO_DRIVE);

    rtc_gpio_init((gpio_num_t)LP_GPIO_DRIVEN);
    rtc_gpio_set_direction((gpio_num_t)LP_GPIO_DRIVEN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_en((gpio_num_t)LP_GPIO_DRIVEN);
    rtc_gpio_pullup_dis((gpio_num_t)LP_GPIO_DRIVEN);
}

void app_driver_mp_gpio_init(){
    gpio_config_t drive_cfg = {};
    drive_cfg.intr_type = GPIO_INTR_DISABLE;
    drive_cfg.mode = GPIO_MODE_OUTPUT;
    drive_cfg.pin_bit_mask = MP_GPIO_DRIVE_MASK;
    drive_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    drive_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&drive_cfg);

    gpio_config_t driven_cfg = {};
    driven_cfg.intr_type = GPIO_INTR_POSEDGE;
    driven_cfg.mode = GPIO_MODE_INPUT;
    driven_cfg.pin_bit_mask = MP_GPIO_DRIVEN_MASK;
    driven_cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
    driven_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&driven_cfg);
}



static void app_driver_door_opened_cb(void *arg, void *data)
{   
    ESP_LOGI(TAG, "Door opened");
    uint16_t endpoint_id = door_endpoint_id;
    uint32_t cluster_id = BooleanState::Id;
    uint32_t attribute_id = BooleanState::Attributes::StateValue::Id;
    
    // node_t *node = node::get();
    // endpoint_t *endpoint = endpoint::get(node, door_endpoint_id);
    // cluster_t *cluster = cluster::get(endpoint, cluster_id);
    // attribute_t *attribute = attribute::get(cluster, attribute_id);

    esp_matter_attr_val_t new_state = esp_matter_bool(false);
    attribute::update(endpoint_id, cluster_id, attribute_id, &new_state);
}

static void app_driver_door_closed_cb(void *arg, void *data)
{   
    ESP_LOGI(TAG, "Door closed");
    uint16_t endpoint_id = door_endpoint_id;
    uint32_t cluster_id = BooleanState::Id;
    uint32_t attribute_id = BooleanState::Attributes::StateValue::Id;
    
    // node_t *node = node::get();
    // endpoint_t *endpoint = endpoint::get(node, door_endpoint_id);
    // cluster_t *cluster = cluster::get(endpoint, cluster_id);
    // attribute_t *attribute = attribute::get(cluster, attribute_id);

    esp_matter_attr_val_t new_state = esp_matter_bool(true);
    attribute::update(endpoint_id, cluster_id, attribute_id, &new_state);
}

bool app_driver_get_button_state(app_driver_handle_t handle){
    return iot_button_get_key_level((button_handle_t) handle);
}
void app_driver_set_door_opened(){
    app_driver_door_opened_cb(NULL, NULL);
}
void app_driver_set_door_closed(){
    app_driver_door_closed_cb(NULL, NULL);
}

app_driver_handle_t app_driver_door_init(){
    button_handle_t handle = iot_button_create(&door_btn_cfg);
    iot_button_register_cb(handle, BUTTON_PRESS_UP, app_driver_door_opened_cb, NULL);
    iot_button_register_cb(handle, BUTTON_PRESS_DOWN, app_driver_door_closed_cb, NULL);
    return (app_driver_handle_t) handle;
}



