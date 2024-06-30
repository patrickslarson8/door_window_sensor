#pragma once
#include <inttypes.h>

#define CYCLES_TO_WAIT 80

#define LP_NUM_ADDRESSES 1
#define LP_ADDRESS_1 0x69
#define LP_ADDRESS_2 0x69

#define MP_NUM_ADDRESSES 2
#define MP_ADDRESS_1 0x68
#define MP_ADDRESS_2 0x69

#define TIMER_ADDRESS_0 0x18
#define TIMER_ADDRESS_1 0x19
#define TIMER_ADDRESS_2 0x1A
#define LP_WATCHDOG_THRESH 25640  // approx 1s

/******************************************/ 
/* Accelerometer Registers Configuration **/
/******************************************/ 
typedef struct {
    uint8_t reg;
    uint8_t value;
} bmi_160_register;

#define WRITE_ACCEL_SETTINGS_0_REG    0x40
#define WRITE_ACCEL_SETTINGS_0_VALUE  0x2B

#define WRITE_ACCEL_SETTINGS_1_REG    0x41
#define WRITE_ACCEL_SETTINGS_1_VALUE  0x03

#define WRITE_INTERRUPT_ENG_0_REG     0x50
#define WRITE_INTERRUPT_ENG_0_VALUE   0x07

#define WRITE_INTERRUPT_ENG_1_REG     0x51
#define WRITE_INTERRUPT_ENG_1_VALUE   0x08

#define WRITE_INTERRUPT_ENG_2_REG     0x52
#define WRITE_INTERRUPT_ENG_2_VALUE   0x00

#define WRITE_INTERRUPT_ELEC_CONFIG_REG   0x53
#define WRITE_INTERRUPT_ELEC_CONFIG_VALUE 0xAA

#define WRITE_INTERRUPT_LATCH_TIME_REG    0x54
#define WRITE_INTERRUPT_LATCH_TIME_VALUE  0x0A

#define WRITE_INTERRUPT_MAP_0_REG     0x55
#define WRITE_INTERRUPT_MAP_0_VALUE   0x04

#define WRITE_INTERRUPT_MAP_1_REG     0x56
#define WRITE_INTERRUPT_MAP_1_VALUE   0x00

#define WRITE_INTERRUPT_MAP_2_REG     0x57
#define WRITE_INTERRUPT_MAP_2_VALUE   0x01

#define WRITE_DATA_SRC_0_REG          0x58
#define WRITE_DATA_SRC_0_VALUE        0x80

#define WRITE_DATA_SRC_1_REG          0x59
#define WRITE_DATA_SRC_1_VALUE        0x80

#define WRITE_LOW_G_TIME_REG          0x5A
#define WRITE_LOW_G_TIME_VALUE        0x27

#define WRITE_LOW_G_THRESHOLD_REG     0x5B
#define WRITE_LOW_G_THRESHOLD_VALUE   0x60

#define WRITE_LOW_G_HYSTERISIS_REG    0x5C
#define WRITE_LOW_G_HYSTERISIS_VALUE  0x00

#define WRITE_LOW_G_DELAY_REG         0x5D
#define WRITE_LOW_G_DELAY_VALUE       0x00

#define WRITE_MOTION_SAMPLES_REG      0x5F
#define WRITE_MOTION_SAMPLES_VALUE    0x00

#define WRITE_MOTION_THRESH_REG       0x60
#define WRITE_MOTION_THRESH_VALUE     0x20

#define WRITE_MOTION_SETTINGS_0_REG   0x61
#define WRITE_MOTION_SETTINGS_0_VALUE 0x14

#define WRITE_MOTION_SETTINGS_1_REG   0x62
#define WRITE_MOTION_SETTINGS_1_VALUE 0x10

// Store in an array for easier writing
#define BMI_CONFIG_SIZE 20
bmi_160_register default_bmi_config[BMI_CONFIG_SIZE] = {
    { WRITE_ACCEL_SETTINGS_0_REG,    WRITE_ACCEL_SETTINGS_0_VALUE },
    { WRITE_ACCEL_SETTINGS_1_REG,    WRITE_ACCEL_SETTINGS_1_VALUE },
    { WRITE_INTERRUPT_ENG_0_REG,     WRITE_INTERRUPT_ENG_0_VALUE },
    { WRITE_INTERRUPT_ENG_1_REG,     WRITE_INTERRUPT_ENG_1_VALUE },
    { WRITE_INTERRUPT_ENG_2_REG,     WRITE_INTERRUPT_ENG_2_VALUE },
    { WRITE_INTERRUPT_ELEC_CONFIG_REG,   WRITE_INTERRUPT_ELEC_CONFIG_VALUE },
    { WRITE_INTERRUPT_LATCH_TIME_REG,    WRITE_INTERRUPT_LATCH_TIME_VALUE },
    { WRITE_INTERRUPT_MAP_0_REG,     WRITE_INTERRUPT_MAP_0_VALUE },
    { WRITE_INTERRUPT_MAP_1_REG,     WRITE_INTERRUPT_MAP_1_VALUE },
    { WRITE_INTERRUPT_MAP_2_REG,     WRITE_INTERRUPT_MAP_2_VALUE },
    { WRITE_DATA_SRC_0_REG,          WRITE_DATA_SRC_0_VALUE },
    { WRITE_DATA_SRC_1_REG,          WRITE_DATA_SRC_1_VALUE },
    { WRITE_LOW_G_TIME_REG,          WRITE_LOW_G_TIME_VALUE },
    { WRITE_LOW_G_THRESHOLD_REG,     WRITE_LOW_G_THRESHOLD_VALUE },
    { WRITE_LOW_G_HYSTERISIS_REG,    WRITE_LOW_G_HYSTERISIS_VALUE },
    { WRITE_LOW_G_DELAY_REG,         WRITE_LOW_G_DELAY_VALUE },
    { WRITE_MOTION_SAMPLES_REG,      WRITE_MOTION_SAMPLES_VALUE },
    { WRITE_MOTION_THRESH_REG,       WRITE_MOTION_THRESH_VALUE },
    { WRITE_MOTION_SETTINGS_0_REG,   WRITE_MOTION_SETTINGS_0_VALUE },
    { WRITE_MOTION_SETTINGS_1_REG,   WRITE_MOTION_SETTINGS_1_VALUE }
};

uint8_t lp_addresses[LP_NUM_ADDRESSES] = {
    LP_ADDRESS_1
    // ,LP_ADDRESS_2
};

/* Commands */
bmi_160_register write_cmd_start_accel = {0x7E, 0x11};
bmi_160_register write_cmd_reset_iterrupts = {0x7E, 0xB1};
bmi_160_register write_cmd_reboot = {0x7E, 0xB6};

/* Values to read*/
bmi_160_register read_error_dropped_cmd_mask = {0x02, 0x40};  // 0bx1xxxxxx
bmi_160_register read_error_code_mask = {0x02, 0x0E};         // 0bxxx1111x
bmi_160_register read_fatal_error_code_mask = {0x02, 0x01};   // 0bxxxxxxx1
bmi_160_register read_pmu_accel_normal_mask = {0x03, 0x10};   // 0bxxx1xxxx
bmi_160_register read_anymotion_tripped_mask = {0x1C, 0x04};  // 0bxxxxx1xx
bmi_160_register read_low_g_tripped_mask = {0x1D, 0x08};      // 0bxxxx1xxx
bmi_160_register read_temperature_0_mask = {0x20, 0xFF};
bmi_160_register read_temperature_1_mask = {0x21, 0xFF};