/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "ulp_lp_core_print.h"
#include "ulp_lp_core_uart.h"
#include "ulp_lp_core_i2c.h"
#include "ulp_lp_core_utils.h"
#include "../bmi160_defs.h"

#define LP_UART_PORT_NUM    LP_UART_NUM_0
// #define LP_I2C_NUM_0 0

#define BMI160_ADDRESS_1 0x68
#define BMI160_ADDRESS_2 0x69

#define LP_I2C_TRANS_TIMEOUT_CYCLES 5000
#define LP_I2C_TRANS_WAIT_FOREVER   -1
#define REGISTER_WRITE_SIZE 2
#define REGISTER_READ_SIZE 1
#define INTERRUPT_CHECK_PERIOD_uS 100000


esp_err_t write_register(const uint16_t address, const bmi_160_register *reg){
    uint8_t buffer[2];
    buffer[0] = reg->reg;
    buffer[1] = reg->value;
    return lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, address, &buffer, REGISTER_WRITE_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
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
    uint8_t address_reg = reg->reg;
    uint8_t mask = reg->value;
    err = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, address, &address_reg, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    err = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, address, &buffer, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    uint8_t compared = buffer & mask;
    *return_value = (compared != 0);
    return err;
}

esp_err_t configure_bmi(const uint16_t address){
    esp_err_t error;
    for (int i = 0; i < BMI_CONFIG_SIZE; i++){
        error = write_register(address, &bmi_config[i]);
        if (error != ESP_OK) return error;
    }
    return error;
}

esp_err_t validate_bmi_configuration(const uint16_t address){
    esp_err_t error = ESP_OK;
    for (int i = 0; i < BMI_CONFIG_SIZE; i++){
        error = compare_register(address, &bmi_config[i]);
        if (error != ESP_OK) return error;
    }
    return error;
}

esp_err_t init_bmi(const uint16_t address){
    esp_err_t err = ESP_OK;
    err = configure_bmi(address);
    if (err != ESP_OK){return err;}
    err = validate_bmi_configuration(address);
    if (err != ESP_OK){return err;}
    err = write_register(address, &write_cmd_start_accel);
    if (err != ESP_OK){return err;}
}

int main (void)
{
    bool anymotion_1 = false;
    bool low_g_1 = false;
    bool was_disconnected_1 = false;

    bool anymotion_2 = false;
    bool low_g_2 = false;
    bool was_disconnected_2 = true;
    esp_err_t err = ESP_OK;

    while(1){
        // if (was_disconnected_1){
        //     err = init_bmi(BMI160_ADDRESS_1);
        //     if (err != ESP_OK) {continue;}
        //     was_disconnected_1 = false;
        // }
        if (was_disconnected_2){
            err = init_bmi(BMI160_ADDRESS_2);
            if (err != ESP_OK) {continue;}
            was_disconnected_2 = false;
        }

        // err = check_register_mask(BMI160_ADDRESS_1, &read_anymotion_tripped_mask, &anymotion_1);
        // if (err != ESP_OK) {was_disconnected_1 = true; continue;}
        // if (anymotion_1){
        //     lp_core_printf("Anymotion tripped\r\n");
        //     anymotion_1 = false;
        // }

        err = check_register_mask(BMI160_ADDRESS_2, &read_anymotion_tripped_mask, &anymotion_2);
        if (err != ESP_OK) {was_disconnected_2 = true; continue;}
        if (anymotion_2){
            lp_core_printf("Anymotion tripped\r\n");
            anymotion_2 = false;
        }

        // err = check_register_mask(BMI160_ADDRESS_1, &read_low_g_tripped_mask, &low_g_1);
        // if (err != ESP_OK) {was_disconnected_1 = true; continue;}
        // if (low_g_1){
        //     lp_core_printf("low g tripped\r\n");
        //     low_g_1 = false;
        // }

        err = check_register_mask(BMI160_ADDRESS_2, &read_low_g_tripped_mask, &low_g_2);
        if (err != ESP_OK) {was_disconnected_2 = true; continue;}
        if (low_g_2){
            lp_core_printf("low g tripped\r\n");
            low_g_2 = false;
        }

        ulp_lp_core_delay_us(INTERRUPT_CHECK_PERIOD_uS);
    };

    return 0;
}
