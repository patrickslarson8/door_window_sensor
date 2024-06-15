#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ulp_lp_core_gpio.h"
#include "ulp_lp_core_i2c.h"
#include "ulp_lp_core_utils.h"
#include "../bmi160_defs.h"

// #define LP_I2C_NUM_0 0

#define BMI160_ADDRESS_1 0x68
#define BMI160_ADDRESS_2 0x69

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
volatile bmi_160_register shared_timers[3];
volatile uint8_t shared_num_timers;
volatile uint32_t shared_deadman_threshold;

volatile int shared_out = -2;                     // -1 for normal, -2 for watchdog checkin, pos num for register that was tripped


esp_err_t write_register(const uint16_t address, const bmi_160_register *reg){
    uint8_t buffer[2];
    buffer[0] = reg->reg;
    buffer[1] = reg->value;
    return lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, address, &buffer, REGISTER_WRITE_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
}

esp_err_t read_register(const uint16_t address, const bmi_160_register *reg, uint8_t *buffer){
    esp_err_t err;
    uint8_t address_reg = reg->reg;
    err = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, address, &address_reg, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
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

uint32_t get_time_elapsed(uint32_t previous_time){
    uint32_t current_time = 0;
    uint8_t buffer;
    uint8_t bitshift = 0;
    for (int i = 0; i < shared_num_timers; i++){
        bmi_160_register reg = shared_timers[i];
        read_register(shared_bmi_addresses[0], &reg, &buffer);
        current_time = current_time | (uint32_t)buffer << (8*i); // combines the three 8 bit registers into a single 32 bit
    }
    return current_time - previous_time;
}

uint8_t init_bmi(){
    esp_err_t err = ESP_OK;
    for (int i = 0; i < shared_num_bmi; i++){
        uint16_t address = shared_bmi_addresses[i];
        err = configure_bmi(address);
        if (err != ESP_OK){return 1;}
        err = validate_bmi_configuration(address);
        if (err != ESP_OK){return 1;}
        err = write_register(address, &write_cmd_start_accel);
        if (err != ESP_OK){return 1;}
        else return 0;
    }
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
    esp_err_t err = ESP_OK;
    bool mask_tripped = false;
    int msg_out = 0;
    uint32_t time_ticks = 0;
    init_gpio();
    while(init_bmi()); // keep trying to program bmi until it works

    while(1){
        if (msg_out != -1){
            shared_out = msg_out;
            ulp_lp_core_gpio_set_level(GPIO_DRIVE, 1);
            while (!ulp_lp_core_gpio_get_level(GPIO_DRIVEN)); // wait for main processor to acknowledge
            ulp_lp_core_gpio_set_level(GPIO_DRIVE, 0);
            while (ulp_lp_core_gpio_get_level(GPIO_DRIVEN)); // wait for main processor to read
            msg_out = -1;
        }
        for (int i = 0; i < shared_num_bmi; i++){
            for (int i = 0; i < shared_num_masks; i++){
                // If a mask register is tripped, report if back to the main processor to handle
                check_register_mask(shared_bmi_addresses[i], &read_anymotion_tripped_mask, &mask_tripped);
                if (mask_tripped){
                    msg_out = i;
                    continue;
                }
            }
        }
        time_ticks = get_time_elapsed(time_ticks);
        if (time_ticks > shared_deadman_threshold){
            msg_out = -2;
        }
    };

    return 0;
}
