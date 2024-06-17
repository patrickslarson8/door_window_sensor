#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ulp_lp_core_gpio.h"
#include "ulp_lp_core_i2c.h"
#include "ulp_lp_core_uart.h"
#include "ulp_lp_core_utils.h"
#include "../bmi160_defs.h"

#define LP_UART_PORT_NUM    LP_UART_NUM_0
// #define LP_I2C_NUM_0 0

#define LP_I2C_TRANS_TIMEOUT_CYCLES 5000
#define LP_I2C_TRANS_WAIT_FOREVER   -1
#define REGISTER_WRITE_SIZE 2
#define REGISTER_READ_SIZE 1
#define INTERRUPT_CHECK_PERIOD_uS 100000

#define GPIO_DRIVE LP_IO_NUM_4
#define GPIO_DRIVEN LP_IO_NUM_5

/* These will all be set by the MP before the lp core starts */
volatile bmi_160_register shared_bmi_config[20];     // config will be loaded into all bmi on startup
volatile uint8_t shared_bmi_addresses[2];
volatile uint8_t shared_num_bmi;
volatile bmi_160_register shared_register_masks[20]; // will be checked and discrepancies reported
volatile uint8_t shared_num_masks;
volatile uint8_t shared_timer_addresses[3];
volatile uint8_t shared_num_timers;
volatile uint32_t shared_deadman_threshold;

// set by LP core to send messages back
volatile int shared_out_status = -2;                     // -1 for normal, -2 for watchdog checkin, pos num for register that was tripped
volatile uint32_t shared_out_address = 0;

esp_err_t write_register(const uint16_t address, const bmi_160_register *reg){
    uint8_t buffer[2];
    buffer[0] = reg->reg;
    buffer[1] = reg->value;
    return lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, address, &buffer, REGISTER_WRITE_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
}

esp_err_t read_register(const uint16_t address, uint8_t reg, uint8_t *buffer){
    esp_err_t err;
    err = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, address, &reg, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    if (err != ESP_OK) { return err; }
    err = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, address, &buffer, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    return err;
}

esp_err_t compare_register(const uint16_t address, const bmi_160_register *reg){
    uint8_t buffer;
    esp_err_t err;
    uint8_t address_reg = reg->reg;
    uint8_t value = reg->value;
    err = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, address, &address_reg, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    if (err != ESP_OK) { return err; }
    err = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, address, &buffer, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    if (buffer != reg->value) { return ESP_ERR_INVALID_RESPONSE;}
    return err;
}

esp_err_t check_register_mask(const uint16_t address, const bmi_160_register *reg, bool *return_value){
    uint8_t buffer;
    esp_err_t err;
    uint8_t reg_num = reg->reg;
    uint8_t mask = reg->value;
    err = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, address, &reg_num, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    err = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, address, &buffer, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    uint8_t compared = buffer & mask;
    *return_value = (compared != 0);
    return err;
}

esp_err_t configure_bmi(const uint16_t address){
    esp_err_t error;
    for (int i = 0; i < BMI_CONFIG_SIZE; i++){
        error = write_register(address, &shared_bmi_config[i]);
        if (error != ESP_OK) return error;
    }
    return error;
}

esp_err_t validate_bmi_configuration(const uint16_t address){
    esp_err_t error = ESP_OK;
    for (int i = 0; i < BMI_CONFIG_SIZE; i++){
        error = compare_register(address, &shared_bmi_config[i]);
        if (error != ESP_OK) return error;
    }
    return error;
}

esp_err_t time_for_checkin(uint32_t* time, uint32_t watchdog_threshold, bool* result){
    esp_err_t err;
    uint32_t previous_time = *time;
    *time = 0;
    uint8_t buffer;
    uint8_t bitshift = 0;
    for (int i = 0; i < shared_num_timers; i++){
        uint8_t reg = shared_timer_addresses[i];
        err = read_register(shared_bmi_addresses[0], reg, &buffer);
        if (err != ESP_OK) return err;
        *time = *time | (uint32_t)buffer << (8*i); // combines the three 8 bit registers into a single 32 bit
    }
    if (*time < previous_time) *result = true; // timer wrapped around, so checkin
    if ((*time - previous_time) > watchdog_threshold) *result = true;
    return err;
}

esp_err_t init_bmi(){
    esp_err_t err = ESP_ERR_NOT_FINISHED;
    lp_core_printf("sharednumbmi:%d\r\n", (int)shared_num_bmi);
    for (int i = 0; i < shared_num_bmi; i++){
        uint16_t address = shared_bmi_addresses[i];
        lp_core_printf("address:%d\r\n", (int)address);
        err = configure_bmi(address);
        if (err != ESP_OK){return err;}
        err = validate_bmi_configuration(address);
        if (err != ESP_OK){return err;}
        err = write_register(address, &write_cmd_start_accel);
        if (err != ESP_OK){return err;}
    }
    return err;
}

void init_gpio(){
    ulp_lp_core_gpio_init(GPIO_DRIVE);
    ulp_lp_core_gpio_output_enable(GPIO_DRIVE);
    ulp_lp_core_gpio_set_output_mode(GPIO_DRIVE, RTCIO_LL_OUTPUT_NORMAL);
    ulp_lp_core_gpio_set_level(GPIO_DRIVE, 0);

    ulp_lp_core_gpio_init(GPIO_DRIVEN);
    ulp_lp_core_gpio_pulldown_enable(GPIO_DRIVEN);
    ulp_lp_core_gpio_input_enable(GPIO_DRIVEN);
}

int main (void)
{
    lp_core_printf("dogvalue:%d, dogaddress:%d\n\r",
     (int) shared_deadman_threshold,
     (int) &shared_deadman_threshold);
    lp_core_printf("address0:%d, num:%d, mask0:%d, num:%d, timer0:%d, num:%d, dog:%d\n\r",
        (int)shared_bmi_addresses[0],
        (int)shared_num_bmi,
        (int)shared_register_masks[0].reg,
        (int)shared_num_masks,
        (int)shared_timer_addresses[0],
        (int)shared_num_timers,
        (int)shared_deadman_threshold);
    esp_err_t err = ESP_OK;
    bool mask_tripped = false;
    bool checkin_due = false;
    int msg_out = -1;
    uint8_t trouble_address = 0;
    uint32_t time_ticks = 0;
    // init_gpio();
    err = init_bmi();

    while(1){
        if (err != ESP_OK) break; // quit and let MP restart LP core
        if (msg_out != -1){
            lp_core_printf("msg:%d\n\r", (int)msg_out);
            shared_out_status = msg_out;
            shared_out_address = trouble_address;
            ulp_lp_core_gpio_set_level(GPIO_DRIVE, 1);
            while (!ulp_lp_core_gpio_get_level(GPIO_DRIVEN)); // wait for main processor to acknowledge
            ulp_lp_core_gpio_set_level(GPIO_DRIVE, 0);
            while (ulp_lp_core_gpio_get_level(GPIO_DRIVEN));  // wait for main processor to read
            msg_out = -1;
        }
        for (int i = 0; i < shared_num_bmi; i++){
            for (int j = 0; j < shared_num_masks; j++){
                // If a mask register is tripped, report it back to the main processor to handle
                err = check_register_mask(shared_bmi_addresses[i], &shared_register_masks[j], &mask_tripped);
                if (err != ESP_OK) break;
                if (mask_tripped){
                    trouble_address = i;
                    msg_out = j;
                    lp_core_printf("mask tripped on %d num %d\n\r", (int)trouble_address, (int)msg_out);
                    continue;
                }
            }
            if (err != ESP_OK) break;
        }
        if (err != ESP_OK) break;
        err = time_for_checkin(&time_ticks, shared_deadman_threshold, &checkin_due);
        if (checkin_due){
            lp_core_printf("watchdog\n\r");
            msg_out = -2;
        } 
    };

    return 0;
}
