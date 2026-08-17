/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "config.h"
#include "motor_control.h"
#include "battery.h"
#include "board_led.h"
#include "rp3_receiver.h"
// #include "uart_bridge.h"
#include "usb_bridge.h"
#include "ros2_msgs.h"
#include "shell_uart.h"
#include "diag.h"
#include "flash_storage.h"

static const char *TAG = "example";

// Enable this config,  we will print debug formated string, which in return can be captured and parsed by Serial-Studio
#define SERIAL_STUDIO_DEBUG CONFIG_SERIAL_STUDIO_DEBUG

void app_main(void)
{
    ESP_ERROR_CHECK(flash_storage_init());

    static motor_control_system_t motor_system = {
        .motors = {},
        .last_pulse_counts = {}};

    ESP_LOGI(TAG, "Initializing %d motor(s)", NUM_MOTORS);

    // Initialize all motors
    for (int i = 0; i < NUM_MOTORS; i++)
    {
        motor_config_t config = get_motor_config(i);
        ESP_ERROR_CHECK(motor_init(i, &motor_system, &config));
    }

    ESP_LOGI(TAG, "Starting all motors");
    ESP_ERROR_CHECK(motor_start_all(&motor_system));

    ESP_LOGI(TAG, "Initializing battery monitoring");
    ESP_ERROR_CHECK(battery_init());

    ESP_LOGI(TAG, "Create a timer to do PID calculation periodically for all motors");
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = pid_loop_cb,
        .arg = &motor_system,
        .name = "pid_loop"};
    esp_timer_handle_t pid_loop_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &pid_loop_timer));

    ESP_LOGI(TAG, "Start motor speed loop");
    ESP_ERROR_CHECK(esp_timer_start_periodic(pid_loop_timer, BDC_PID_LOOP_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Initializing health RGB LED on GPIO%d", BUILTIN_LED_GPIO);

    led_init();
    led_set(0, 16, 0);
    extend_led_set(3, 0, 0, 8);

    // --- RP3 Receiver initialization ---
    ESP_ERROR_CHECK(diag_init());

    rp3_receiver_init();

    ros2_msgs_init();

    shell_uart_init();

    while (1)
    {
        led_set(16, 16, 16);
        vTaskDelay(pdMS_TO_TICKS(1000));
        led_set(0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
