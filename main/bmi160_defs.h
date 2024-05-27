#pragma once
#include <inttypes.h>

/******************************************/ 
/* Accelerometer Registers Configuration **/
/******************************************/ 
typedef struct {
    const uint8_t reg;
    const uint8_t value;
} bmi_160_register;

/* Values to write */
const bmi_160_register write_accel_settings_0 = {0x40, 0x2B};      // 800 hz
const bmi_160_register write_accel_settings_1 = {0x41, 0x03};      // 2g range
const bmi_160_register write_interrupt_eng_0 = {0x50, 0x07};       // anymotion
const bmi_160_register write_interrupt_eng_1 = {0x51, 0x08};       // low_g
const bmi_160_register write_interrupt_eng_2 = {0x52, 0x00};       // interrupts we don't want
const bmi_160_register write_interrupt_elec_config = {0x53, 0xAA}; // push/pull
const bmi_160_register write_interrupt_latch_time = {0x54, 0x0A};  // 160 ms
const bmi_160_register write_interrupt_map_0 = {0x55, 0x04};       // int 1 -> anymotion
const bmi_160_register write_interrupt_map_1 = {0x56, 0x00};       // functions don't want mapped
const bmi_160_register write_interrupt_map_2 = {0x57, 0x01};       // int 2 -> low_g
const bmi_160_register write_data_src_0 = {0x58, 0x80};            // low_g prefiltered data
const bmi_160_register write_data_src_1 = {0x59, 0x80};            // anymotion prefiltered data
const bmi_160_register write_low_g_time = {0x5A, 0x27};            // 100 ms
const bmi_160_register write_low_g_threshold = {0x5B, 0x60};       // 0.7g
const bmi_160_register write_low_g_hysterisis = {0x5C, 0x00};      // no hysteresis
const bmi_160_register write_low_g_delay = {0x5D, 0x00};           // no delay
const bmi_160_register write_motion_samples = {0x5F, 0x00};        // 1 sample
const bmi_160_register write_motion_thresh = {0x60, 0x20};         // 0.12 g
const bmi_160_register write_motion_settings_0 = {0x61, 0x14};     // don't care
const bmi_160_register write_motion_settings_1 = {0x62, 0x10};     // anymotion settings

/* Store in an array for easier writing */
#define BMI_CONFIG_SIZE 20
bmi_160_register bmi_config [BMI_CONFIG_SIZE] = {
write_accel_settings_0,
write_accel_settings_1,
write_interrupt_eng_0,
write_interrupt_eng_1,
write_interrupt_eng_2,
write_interrupt_elec_config,
write_interrupt_latch_time,
write_interrupt_map_0,
write_interrupt_map_1,
write_interrupt_map_2,
write_data_src_0,
write_data_src_1,
write_low_g_time,
write_low_g_threshold,
write_low_g_hysterisis,
write_low_g_delay,
write_motion_samples,
write_motion_thresh,
write_motion_settings_0,
write_motion_settings_1
};

/* Commands */
const bmi_160_register write_cmd_start_accel = {0x7E, 0x11};
const bmi_160_register write_cmd_reset_iterrupts = {0x7E, 0xB1};
const bmi_160_register write_cmd_reboot = {0x7E, 0xB6};

/* Values to read*/
const bmi_160_register read_error_dropped_cmd_mask = {0x02, 0x40};  // 0bx1xxxxxx
const bmi_160_register read_error_code_mask = {0x02, 0x0E};         // 0bxxx1111x
const bmi_160_register read_fatal_error_code_mask = {0x02, 0x01};   // 0bxxxxxxx1
const bmi_160_register read_pmu_accel_normal_mask = {0x03, 0x10};   // 0bxxx1xxxx
const bmi_160_register read_anymotion_tripped_mask = {0x1C, 0x04};  // 0bxxxxx1xx
const bmi_160_register read_low_g_tripped_mask = {0x1D, 0x08};      // 0bxxxx1xxx
const bmi_160_register read_temperature_0_mask = {0x20, 0xFF};
const bmi_160_register read_temperature_1_mask = {0x21, 0xFF};