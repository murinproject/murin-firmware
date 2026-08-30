/*
 * SPDX-FileCopyrightText: 2024
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include "battery.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "battery";

static i2c_master_bus_handle_t s_battery_i2c_bus;
static i2c_master_dev_handle_t s_battery_wattmeter_dev;
static bool s_battery_initialized;
static bool s_battery_available;

static void battery_cleanup(void)
{
    if (s_battery_wattmeter_dev != NULL) {
        i2c_master_bus_rm_device(s_battery_wattmeter_dev);
        s_battery_wattmeter_dev = NULL;
    }
    if (s_battery_i2c_bus != NULL) {
        i2c_del_master_bus(s_battery_i2c_bus);
        s_battery_i2c_bus = NULL;
    }

    s_battery_initialized = false;
    s_battery_available = false;
}

/**
 * @brief Read a big-endian 16-bit register from the INA219
 *
 * @param reg_addr Register address
 * @param data Pointer to store 16-bit value
 * @return esp_err_t ESP_OK on success
 */
static esp_err_t i2c_read_register_16(uint8_t reg_addr, uint16_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_battery_available || s_battery_wattmeter_dev == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t reg_data[2] = {0};
    esp_err_t ret = i2c_master_transmit_receive(
        s_battery_wattmeter_dev,
        &reg_addr,
        sizeof(reg_addr),
        reg_data,
        sizeof(reg_data),
        BATTERY_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02X: %s", reg_addr, esp_err_to_name(ret));
        return ret;
    }

    *data = ((uint16_t)reg_data[0] << 8) | reg_data[1];
    return ESP_OK;
}

/**
 * @brief Write a big-endian 16-bit register to the INA219
 */
static esp_err_t i2c_write_register_16(uint8_t reg_addr, uint16_t value)
{
    if (!s_battery_available || s_battery_wattmeter_dev == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t reg_data[3] = {
        reg_addr,
        (uint8_t)(value >> 8),
        (uint8_t)value,
    };
    return i2c_master_transmit(
        s_battery_wattmeter_dev, reg_data, sizeof(reg_data), BATTERY_I2C_TIMEOUT_MS);
}

/**
 * @brief Convert the INA219 bus-voltage register to volts
 *
 * @param raw_register Raw INA219 register value
 * @return float Voltage in volts
 */
static float convert_voltage(uint16_t raw_register)
{
    return (float)(raw_register >> 3) * 0.004f;
}

/**
 * @brief Convert the calibrated INA219 current register to amps
 *
 * @param raw_register Raw INA219 register value
 * @return float Current in amps
 */
static float convert_current(int16_t raw_register)
{
    return (float)raw_register * 0.001f;
}

/**
 * @brief Convert the INA219 power register to watts
 *
 * @param raw_register Raw INA219 register value
 * @return float Power in watts
 */
static float convert_power(uint16_t raw_register)
{
    return (float)raw_register * 0.020f;
}

void battery_init(void)
{
    if (s_battery_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Initializing I2C for battery monitoring (SDA=%d, SCL=%d)",
             BATTERY_I2C_MASTER_SDA_IO, BATTERY_I2C_MASTER_SCL_IO);

    i2c_master_bus_config_t bus_config = {
        .i2c_port = BATTERY_I2C_MASTER_NUM,
        .sda_io_num = BATTERY_I2C_MASTER_SDA_IO,
        .scl_io_num = BATTERY_I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &s_battery_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        battery_cleanup();
        s_battery_initialized = true;
        return;
    }

    ret = i2c_master_probe(s_battery_i2c_bus, BATTERY_WATTMETER_I2C_ADDR, BATTERY_I2C_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Wattmeter not found at I2C address 0x%02X, battery telemetry disabled: %s",
                 BATTERY_WATTMETER_I2C_ADDR, esp_err_to_name(ret));
        battery_cleanup();
        s_battery_initialized = true;
        return;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BATTERY_WATTMETER_I2C_ADDR,
        .scl_speed_hz = BATTERY_I2C_MASTER_FREQ_HZ,
        .scl_wait_us = 0,
    };

    ret = i2c_master_bus_add_device(s_battery_i2c_bus, &dev_config, &s_battery_wattmeter_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add wattmeter device to I2C bus: %s", esp_err_to_name(ret));
        battery_cleanup();
        s_battery_initialized = true;
        return;
    }

    s_battery_available = true;
    ret = i2c_write_register_16(BATTERY_REG_CONFIG, BATTERY_CONFIG);
    if (ret == ESP_OK) {
        ret = i2c_write_register_16(BATTERY_REG_CALIBRATION, BATTERY_CALIBRATION);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "INA219 configuration failed, battery telemetry disabled: %s",
                 esp_err_to_name(ret));
        battery_cleanup();
        s_battery_initialized = true;
        return;
    }

    s_battery_initialized = true;
    ESP_LOGI(TAG, "Battery monitoring initialized successfully");
}

esp_err_t battery_read_data(battery_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    uint16_t raw_value = 0;
    int16_t raw_current = 0;

    memset(data, 0, sizeof(battery_data_t));
    data->timestamp = (uint32_t)(esp_timer_get_time() / 1000); // microseconds to milliseconds

    if (!s_battery_available) {
        data->valid = false;
        return ESP_ERR_NOT_FOUND;
    }

    ret = i2c_read_register_16(BATTERY_REG_BUS_VOLTAGE, &raw_value);
    if (ret == ESP_OK) {
        /* INA219 bus-voltage data is left-aligned in bits 15:3.
         * Keep the register value for diagnostics, but expose volts to
         * callers through data->voltage. */
        data->voltage_raw = raw_value;
        data->voltage = convert_voltage(raw_value);
    } else {
        ESP_LOGW(TAG, "Failed to read voltage: %s", esp_err_to_name(ret));
        data->valid = false;
        return ret;
    }

    ret = i2c_read_register_16(BATTERY_REG_CURRENT, &raw_value);
    if (ret == ESP_OK) {
        raw_current = (int16_t)raw_value;
        data->current = convert_current(raw_current);
    } else {
        ESP_LOGW(TAG, "Failed to read current: %s", esp_err_to_name(ret));
        data->valid = false;
        return ret;
    }

    ret = i2c_read_register_16(BATTERY_REG_POWER, &raw_value);
    if (ret == ESP_OK) {
        data->power = convert_power(raw_value);
    } else {
        ESP_LOGW(TAG, "Failed to read power: %s", esp_err_to_name(ret));
        data->valid = false;
        return ret;
    }

    data->energy = 0.0f; // INA219 has no energy accumulator.
    data->current_raw = raw_current;

    data->valid = true;
    return ESP_OK;
}

esp_err_t battery_read_voltage(float *voltage)
{
    if (voltage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw_value = 0;
    esp_err_t ret = i2c_read_register_16(BATTERY_REG_BUS_VOLTAGE, &raw_value);
    if (ret == ESP_OK) {
        *voltage = convert_voltage(raw_value);
    }
    return ret;
}

esp_err_t battery_read_current(float *current)
{
    if (current == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw_value = 0;
    esp_err_t ret = i2c_read_register_16(BATTERY_REG_CURRENT, &raw_value);
    if (ret == ESP_OK) {
        *current = convert_current((int16_t)raw_value);
    }
    return ret;
}

esp_err_t battery_read_power(float *power)
{
    if (power == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t raw_value = 0;
    esp_err_t ret = i2c_read_register_16(BATTERY_REG_POWER, &raw_value);
    if (ret == ESP_OK) {
        *power = convert_power(raw_value);
    }
    return ret;
}

esp_err_t battery_read_energy(float *energy)
{
    if (energy == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *energy = 0.0f;
    return ESP_ERR_NOT_SUPPORTED;
}

void battery_print_status(void)
{
    battery_data_t data;
    esp_err_t ret = battery_read_data(&data);

    if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Battery wattmeter unavailable");
        return;
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read battery data: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "=== Battery Status ===");
    ESP_LOGI(TAG, "Voltage: %.3f V", data.voltage);
    ESP_LOGI(TAG, "Current: %.3f A", data.current);
    ESP_LOGI(TAG, "Power: %.3f W", data.power);
    ESP_LOGI(TAG, "Energy: unavailable (INA219 has no energy register)");
    ESP_LOGI(TAG, "Raw Bus Voltage Register: 0x%04X", data.voltage_raw);
    ESP_LOGI(TAG, "Raw Current Register: %d", data.current_raw);
    ESP_LOGI(TAG, "Data Valid: %s", data.valid ? "YES" : "NO");
    ESP_LOGI(TAG, "Timestamp: %" PRIu32 " ms", data.timestamp);
}

esp_err_t battery_reset_energy(void)
{
    ESP_LOGW(TAG, "Energy reset not supported: INA219 has no energy accumulator");
    return ESP_ERR_NOT_SUPPORTED;
}
