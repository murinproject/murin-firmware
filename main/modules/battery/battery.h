/*
 * SPDX-FileCopyrightText: 2024
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "esp_err.h"
#include "driver/i2c_master.h"

/* I2C Configuration */
#define BATTERY_I2C_MASTER_NUM         I2C_NUM_0
#define BATTERY_I2C_MASTER_SDA_IO      I2C_SDA
#define BATTERY_I2C_MASTER_SCL_IO      I2C_SCL
#define BATTERY_I2C_MASTER_FREQ_HZ     100000
#define BATTERY_I2C_TIMEOUT_MS         1000

/* DFRobot INA219 I2C address (A0 = 1, A1 = 1) */
#define BATTERY_WATTMETER_I2C_ADDR     0x45

/* INA219 registers and DFRobot configuration */
#define BATTERY_REG_CONFIG             0x00
#define BATTERY_REG_SHUNT_VOLTAGE      0x01
#define BATTERY_REG_BUS_VOLTAGE        0x02
#define BATTERY_REG_POWER              0x03
#define BATTERY_REG_CURRENT             0x04
#define BATTERY_REG_CALIBRATION        0x05
#define BATTERY_CONFIG                 0x3FFF  // 32 V, PGA /8, 12-bit x8, continuous
#define BATTERY_CALIBRATION            4096

/**
 * @brief Battery measurement data structure
 */
typedef struct {
    float voltage;          // Voltage in volts
    float current;          // Current in amps
    float power;            // Power in watts
    float energy;           // Energy in watt-hours
    uint16_t voltage_raw;   // Raw INA219 bus-voltage register value
    int16_t current_raw;    // Raw INA219 current register value
    uint32_t timestamp;     // Timestamp of last measurement
    bool valid;             // Data validity flag
} battery_data_t;

/**
 * @brief Initialize battery monitoring system
 * Sets up I2C communication and INA219 calibration using DFRobot's defaults
 *
 * Initialization errors are logged internally; battery telemetry is marked
 * unavailable when initialization cannot complete.
 */
void battery_init(void);

/**
 * @brief Read all battery measurements
 *
 * @param data Pointer to battery_data_t structure to store measurements
 * @return esp_err_t ESP_OK on success
 */
esp_err_t battery_read_data(battery_data_t *data);

/**
 * @brief Read INA219 bus voltage
 *
 * @param voltage Pointer to store voltage value in volts
 * @return esp_err_t ESP_OK on success
 */
esp_err_t battery_read_voltage(float *voltage);

/**
 * @brief Read INA219 current
 *
 * @param current Pointer to store current value in amps
 * @return esp_err_t ESP_OK on success
 */
esp_err_t battery_read_current(float *current);

/**
 * @brief Read INA219 power
 *
 * @param power Pointer to store power value in watts
 * @return esp_err_t ESP_OK on success
 */
esp_err_t battery_read_power(float *power);

/**
 * @brief Read accumulated energy (not supported by INA219)
 *
 * @param energy Pointer to store energy value in watt-hours
 * @return esp_err_t ESP_OK on success
 */
esp_err_t battery_read_energy(float *energy);

/**
 * @brief Print battery status for debugging
 */
void battery_print_status(void);

/**
 * @brief Reset energy counter (not supported by INA219)
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t battery_reset_energy(void);

#endif // BATTERY_H
