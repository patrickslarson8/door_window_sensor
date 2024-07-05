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

#define GPIO_DRIVE LP_IO_NUM_3
#define GPIO_DRIVEN LP_IO_NUM_2

/* These will all be set by the MP before the lp core starts */
volatile bmi_160_register shared_bmi_config[20];     // config will be loaded into all bmi on startup
volatile uint8_t shared_bmi_addresses[2];
volatile uint8_t shared_num_bmi;
volatile bmi_160_register shared_register_masks[20]; // will be checked and discrepancies reported
volatile uint8_t shared_num_masks;
volatile uint8_t shared_timer_addresses[3];
volatile uint8_t shared_num_timers;
volatile uint32_t shared_deadman_threshold;
volatile bmi_160_register shared_cmd_start;
volatile bmi_160_register shared_cmd_reboot;


// set by LP core to send messages back
volatile int shared_out_status = -2;                 // -1 for normal, -2 for watchdog checkin, -3 for esp err, or pos num for register that was tripped
volatile esp_err_t shared_esp_err = 0;
volatile uint32_t shared_out_address = 0;

union time_bytes {
    struct {
        uint8_t byte1;
        uint8_t byte2;
        uint8_t byte3;
        uint8_t byte4; // Additional byte to complete 32-bit alignment
    } bytes;
    uint32_t combined;
};

esp_err_t write_register(const uint16_t address, const bmi_160_register *reg){
    esp_err_t err;
    uint8_t buffer[2];
    buffer[0] = reg->reg;
    buffer[1] = reg->value;
    err = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, address, &buffer, REGISTER_WRITE_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    lp_core_printf("writing reg: %X, val: %x, err: %x\r\n", reg->reg, reg->value, err);
    return err;
}

esp_err_t read_register(const uint16_t address, uint8_t reg, uint8_t *buffer){
    esp_err_t err;
    err = lp_core_i2c_master_write_to_device(LP_I2C_NUM_0, address, &reg, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    if (err != ESP_OK) { return err; }
    err = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, address, buffer, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    // lp_core_printf("read: %x value: %x\r\n", reg, *buffer);
    return err;
}

esp_err_t compare_register(const uint16_t address, const bmi_160_register *reg){
    uint8_t buffer;
    esp_err_t err;
    uint8_t address_reg = reg->reg;
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
    if (err != ESP_OK) { return err; }
    err = lp_core_i2c_master_read_from_device(LP_I2C_NUM_0, address, &buffer, REGISTER_READ_SIZE, LP_I2C_TRANS_TIMEOUT_CYCLES);
    uint8_t compared = buffer & mask;
    *return_value = (compared != 0);
    return err;
}

esp_err_t configure_bmi(const uint16_t address){
    esp_err_t error;
    for (int i = 0; i < BMI_CONFIG_SIZE; i++){
        // ulp_lp_core_delay_cycles(CYCLES_TO_WAIT);
        error = write_register(address, &shared_bmi_config[i]);
        if (error != ESP_OK) return error;
    }
    return error;
}

esp_err_t validate_bmi_configuration(const uint16_t address){
    lp_core_printf("validating...");
    esp_err_t error = ESP_OK;
    for (int i = 0; i < BMI_CONFIG_SIZE; i++){
        error = compare_register(address, &shared_bmi_config[i]);
        if (error != ESP_OK) break;
    }
    lp_core_printf("err:%x\r\n", error);
    return error;
}

esp_err_t time_for_checkin(uint32_t* prev_time, uint32_t watchdog_threshold, bool* result){
    esp_err_t err;
    uint32_t time = 0;
    uint8_t buffer;

    err = read_register(shared_bmi_addresses[0], shared_timer_addresses[0], &buffer);
    if (err != ESP_OK) return err;
    uint8_t byte1 = buffer;

    err = read_register(shared_bmi_addresses[0], shared_timer_addresses[1], &buffer);
    if (err != ESP_OK) return err;
    uint8_t byte2 = buffer;

    err = read_register(shared_bmi_addresses[0], shared_timer_addresses[2], &buffer);
    if (err != ESP_OK) return err;
    uint8_t byte3 = buffer;

    time = (uint32_t)byte1 | ((uint32_t)byte2 << 8) | ((uint32_t)byte3 << 16);

    lp_core_printf("prev:%d time:%d\r\n", *prev_time, time);
    if ((time < *prev_time) || ((time - *prev_time) > watchdog_threshold)){
        *result = true;
        *prev_time = time;
    }
    return err;
}

esp_err_t init_bmi(){
    esp_err_t err = ESP_ERR_NOT_FINISHED;
    lp_core_printf("init_bmi...");
    for (int i = 0; i < shared_num_bmi; i++){
        uint16_t address = shared_bmi_addresses[i];
        err = write_register(address, &write_cmd_reboot);
        if (err != ESP_OK) break;
        ulp_lp_core_delay_cycles(100000);
        err = configure_bmi(address);
        if (err != ESP_OK) break;
        err = validate_bmi_configuration(address);
        if (err != ESP_OK){return err;}
        err = write_register(address, &write_cmd_start_accel);
    }
    ulp_lp_core_delay_cycles(100000);
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
    uint32_t time = 0;
    lp_core_printf("\033[H\033[2J");
    init_gpio();
    ulp_lp_core_delay_cycles(20000000); // 1 sec
    ulp_lp_core_gpio_get_level(GPIO_DRIVEN);
    lp_core_printf("Waiting for hp core\r\n");
    while(ulp_lp_core_gpio_get_level(GPIO_DRIVEN)){
        ulp_lp_core_delay_cycles(10000);
    }
    lp_core_printf("address0:%x, num:%d, mask0:%x, num:%d, timer0:%d, num:%d, dog:%d\n\r",
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
    // uint32_t time_ticks = 0;
    err = init_bmi();

    while(1){
        if (err != ESP_OK){
            lp_core_printf("err code:%x\r\n",err);
            msg_out = -3;
        }    
        if (msg_out != -1){
            lp_core_printf("msg:%d\n\r", (int)msg_out);
            shared_out_status = msg_out;
            shared_out_address = trouble_address;
            shared_esp_err = err;
            ulp_lp_core_gpio_set_level(GPIO_DRIVE, 1);
            while (ulp_lp_core_gpio_get_level(GPIO_DRIVEN) == 0); // wait for main processor to acknowledge
            ulp_lp_core_gpio_set_level(GPIO_DRIVE, 0);
            while (ulp_lp_core_gpio_get_level(GPIO_DRIVEN) == 1);  // wait for main processor to read
            msg_out = -1;
        }
        for (int i = 0; i < shared_num_bmi; i++){
            for (int j = 0; j < shared_num_masks; j++){
                // If a mask register is tripped, report it back to the main processor to handle
                err = check_register_mask(shared_bmi_addresses[i], &shared_register_masks[j], &mask_tripped);
                if (err != ESP_OK) continue;
                if (mask_tripped){
                    trouble_address = i;
                    msg_out = j;
                    lp_core_printf("mask tripped on %d num %d\n\r", (int)trouble_address, (int)msg_out);
                    continue;
                }
            }
            if (err != ESP_OK) continue;
        }
        if (err != ESP_OK) continue;
        err = time_for_checkin(&time, shared_deadman_threshold, &checkin_due);
        ulp_lp_core_delay_cycles(100000);
        if (checkin_due){
            lp_core_printf("watchdog\n\r");
            checkin_due = false;
            msg_out = -2;
        }
    };

    return 0;
}
