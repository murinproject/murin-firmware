#include "ros2_msgs.h"

#include <string.h>
#ifdef UNIT_TEST
#include "ros2_msgs_host_stubs.h"
#else
#include "battery.h"
#include "board_led.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "sdkconfig.h"
#include "flash_storage.h"
#endif

#include "diag.h"

#if defined(CONFIG_ROS2_TRANSPORT_UART) && CONFIG_ROS2_TRANSPORT_UART
#include "uart_bridge.h"
#else
#include "usb_bridge.h"
#endif

static const char *TAG = "ros2_msgs";

#define ROS2_MSG_HEARTBEAT 0x00
#define ROS2_MSG_CMD_MOTOR 0x01
#define ROS2_MSG_CMD_SERVO 0x02
#define ROS2_MSG_TELEMETRY 0x03
#define ROS2_MSG_CMD_CONFIG 0x10
#define ROS2_MSG_ACK 0x7E
#define ROS2_MSG_NACK 0x7F

#define ROS2_CFG_TELEM_ENABLE 1
#define ROS2_CFG_TELEM_RATE_MS 2
#define ROS2_CFG_TELEM_MASK 3
#define ROS2_CFG_TELEM_TIMEOUT_MS 4

#define ROS2_MSG_ERR_CRC 0x01
#define ROS2_MSG_ERR_LEN 0x02
#define ROS2_MSG_ERR_TYPE 0x03
#define ROS2_MSG_ERR_CFG 0x04
#define ROS2_MSG_ERR_RANGE 0x05

#define ROS2_RUNTIME_SAVE_PERIOD_MS (60 * 1000)

static ros2_msgs_ctx_t ros2_ctx;

static TimerHandle_t telemetry_timer = NULL;

static volatile bool telemetry_enabled = true;
static uint64_t total_runtime_ms = 0;
#ifndef UNIT_TEST
static int64_t runtime_start_us = 0;
#endif
static uint32_t telemetry_period_ms = 20;
static uint32_t telemetry_mask = UINT32_MAX;
static SemaphoreHandle_t tx_mutex = NULL;
static TaskHandle_t command_task = NULL;
static TaskHandle_t telemetry_task = NULL;
#ifndef UNIT_TEST
static TaskHandle_t runtime_task = NULL;
#endif

static size_t ros2_msgs_read(ros2_msgs_ctx_t *msgs, uint8_t *data, size_t len);
static void ros2_msgs_handle_frame(void *ctx, uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t len);

#ifndef UNIT_TEST
static void ros2_msgs_load_settings(void)
{
    telemetry_enabled = flash_storage_get_telemetry_enabled(true);
    total_runtime_ms = flash_storage_get_total_runtime_ms();
}

static void ros2_msgs_save_runtime(void)
{
    const int64_t now_us = esp_timer_get_time();
    if (now_us >= runtime_start_us)
        total_runtime_ms += (uint64_t)((now_us - runtime_start_us) / 1000);
    runtime_start_us = now_us;

    ESP_ERROR_CHECK(flash_storage_set_total_runtime_ms(total_runtime_ms));
}

static void ros2_runtime_task(void *pvParameters)
{
    (void)pvParameters;
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(ROS2_RUNTIME_SAVE_PERIOD_MS));
        ros2_msgs_save_runtime();
    }
}
#endif

static void telemetry_timer_cb(TimerHandle_t xTimer)
{
    if (telemetry_task != NULL)
        xTaskNotifyGive(telemetry_task);
}

void ros2_msgs_on_rx(void)
{
    if (command_task != NULL)
    {
        xTaskNotifyGive(command_task);
    }
}

void ros2_msgs_set_telemetry_enabled(bool enabled)
{
    telemetry_enabled = enabled;

#ifndef UNIT_TEST
    flash_storage_set_telemetry_enabled(enabled);
#endif

    if (telemetry_timer == NULL)
        return;

    if (enabled)
        xTimerStart(telemetry_timer, 0);
    else
        xTimerStop(telemetry_timer, 0);
}

bool ros2_msgs_get_telemetry_enabled(void)
{
    return telemetry_enabled;
}

uint64_t ros2_msgs_get_total_runtime_ms(void)
{
#ifdef UNIT_TEST
    return total_runtime_ms;
#else
    const int64_t now_us = esp_timer_get_time();
    if (now_us <= runtime_start_us)
        return total_runtime_ms;
    return total_runtime_ms + (uint64_t)((now_us - runtime_start_us) / 1000);
#endif
}

void ros2_command_task(void *pvParameters)
{
    (void)pvParameters;
    ros2_msgs_ctx_t *msgs = (ros2_msgs_ctx_t *)pvParameters;
    static uint8_t rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE];

    ESP_LOGI(TAG, "ros2 command task start");

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));

        while (1)
        {
            const size_t rx_size = ros2_msgs_read(msgs, rx_buf, sizeof(rx_buf));
            if (rx_size == 0)
            {
                break;
            }

            framed_link_process(&msgs->link, rx_buf, rx_size);
        }
    }
}

void ros2_telemetry_task(void *pvParameters)
{
    (void)pvParameters;
    ros2_msgs_ctx_t *msgs = (ros2_msgs_ctx_t *)pvParameters;
    msgs->tx_seq = 0;

    ESP_LOGI(TAG, "ros2 telemetry task start");

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        ros2_msgs_send_telemetry(msgs, msgs->tx_seq++);
        // ESP_LOGI(TAG, "Sending telemetry %u", msgs->tx_seq);
    }
}

static size_t ros2_msgs_read(ros2_msgs_ctx_t *msgs, uint8_t *data, size_t len)
{
    return (msgs->read != NULL) ? msgs->read(data, len) : 0;
}

void ros2_msgs_send_frame(ros2_msgs_ctx_t *msgs, uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t payload_len)
{
    if (tx_mutex != NULL)
        xSemaphoreTake(tx_mutex, portMAX_DELAY);

    const size_t written = framed_link_send_frame(&msgs->link, msg_type, seq, payload, payload_len);
    if (written == 0)
        ESP_LOGW(TAG, "Frame TX failed type=0x%02X seq=%u", msg_type, seq);

    if (tx_mutex != NULL)
        xSemaphoreGive(tx_mutex);
}

static void ros2_msgs_send_ack(ros2_msgs_ctx_t *msgs, uint8_t seq)
{
    uint8_t payload = seq;
    ros2_msgs_send_frame(msgs, FRAMED_LINK_MSG_ACK, seq, &payload, 1);
}

static void ros2_msgs_send_nack(ros2_msgs_ctx_t *msgs, uint8_t seq, uint8_t err)
{
    uint8_t payload[2] = {seq, err};
    ros2_msgs_send_frame(msgs, FRAMED_LINK_MSG_NACK, seq, payload, sizeof(payload));
}

void ros2_msgs_send_telemetry(ros2_msgs_ctx_t *msgs, uint8_t seq)
{
    uint8_t payload[256];
    size_t len = 0;
    battery_data_t battery_data;
    const esp_err_t battery_ret = battery_read_data(&battery_data);

    payload[len++] = (battery_ret == ESP_OK && battery_data.valid) ? 1 : 0;
    payload[len++] = (uint8_t)battery_ret;

    memcpy(payload + len, &battery_data.timestamp, sizeof(battery_data.timestamp));
    len += sizeof(battery_data.timestamp);
    memcpy(payload + len, &battery_data.voltage, sizeof(battery_data.voltage));
    len += sizeof(battery_data.voltage);
    memcpy(payload + len, &battery_data.current, sizeof(battery_data.current));
    len += sizeof(battery_data.current);
    memcpy(payload + len, &battery_data.power, sizeof(battery_data.power));
    len += sizeof(battery_data.power);
    memcpy(payload + len, &battery_data.energy, sizeof(battery_data.energy));
    len += sizeof(battery_data.energy);
    memcpy(payload + len, &battery_data.voltage_raw, sizeof(battery_data.voltage_raw));
    len += sizeof(battery_data.voltage_raw);
    memcpy(payload + len, &battery_data.current_raw, sizeof(battery_data.current_raw));
    len += sizeof(battery_data.current_raw);

    ros2_msgs_send_frame(msgs, ROS2_MSG_TELEMETRY, seq, payload, len);
}

static void ros2_msgs_handle_message(ros2_msgs_ctx_t *msgs, uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t len)
{
    diag_log_ros2(msg_type, seq, payload, len);

    switch (msg_type)
    {
    case ROS2_MSG_HEARTBEAT:
        ESP_LOGD(TAG, "Received HEARTBEAT seq=%u", seq);
        ros2_msgs_send_ack(msgs, seq);
        break;

    case ROS2_MSG_CMD_MOTOR:
        if (len == 4)
        {
            const int16_t left = (int16_t)(payload[0] | (payload[1] << 8));
            const int16_t right = (int16_t)(payload[2] | (payload[3] << 8));
            ESP_LOGD(TAG, "Received CMD_MOTOR seq=%u left=%d right=%d", seq, left, right);
            ros2_msgs_send_ack(msgs, seq);
        }
        else
        {
            ros2_msgs_send_nack(msgs, seq, ROS2_MSG_ERR_LEN);
        }
        break;

    case ROS2_MSG_CMD_SERVO:
        if (len == 3)
        {
            const uint8_t channel = payload[0];
            const uint16_t pulse = payload[1] | (payload[2] << 8);
            ESP_LOGD(TAG, "Received CMD_SERVO seq=%u channel=%u pulse=%u", seq, channel, pulse);
            ros2_msgs_send_ack(msgs, seq);
        }
        else
        {
            ros2_msgs_send_nack(msgs, seq, ROS2_MSG_ERR_LEN);
        }
        break;

    case ROS2_MSG_CMD_CONFIG:
        if (len == 5)
        {
            const uint8_t key = payload[0];
            const uint32_t raw_value = (uint32_t)payload[1] |
                                       ((uint32_t)payload[2] << 8) |
                                       ((uint32_t)payload[3] << 16) |
                                       ((uint32_t)payload[4] << 24);
            const int32_t value = (int32_t)raw_value;

            switch (key)
            {
            case ROS2_CFG_TELEM_ENABLE:
                ros2_msgs_set_telemetry_enabled(value != 0);

                ESP_LOGI(TAG, "Telemetry %s", telemetry_enabled ? "enabled" : "disabled");
                break;

            case ROS2_CFG_TELEM_RATE_MS:
                if (value >= 10 && value <= 5000)
                {
                    telemetry_period_ms = (uint32_t)value;
                    if (telemetry_timer != NULL)
                        xTimerChangePeriod(telemetry_timer, pdMS_TO_TICKS(telemetry_period_ms), 0);
                    ESP_LOGI(TAG, "Telemetry rate set to %u ms", telemetry_period_ms);
                }
                else
                {
                    ros2_msgs_send_nack(msgs, seq, ROS2_MSG_ERR_RANGE);
                    return;
                }
                break;

            case ROS2_CFG_TELEM_MASK:
                telemetry_mask = (uint32_t)value;
                ESP_LOGI(TAG, "Telemetry mask set to 0x%08X", telemetry_mask);
                break;

            default:
                ros2_msgs_send_nack(msgs, seq, ROS2_MSG_ERR_CFG);
                return;
            }

            ESP_LOGI(TAG,
                     "CONFIG seq=%u key=%u value=%ld",
                     seq, key, (long)value);

            ros2_msgs_send_ack(msgs, seq);
        }
        else
        {
            ros2_msgs_send_nack(msgs, seq, ROS2_MSG_ERR_LEN);
        }
        break;

    default:
        led_set(16, 0, 0);
        ESP_LOGW(TAG, "Unknown message type 0x%02X seq=%u", msg_type, seq);
        ros2_msgs_send_nack(msgs, seq, ROS2_MSG_ERR_TYPE);
        break;
    }
}

static void ros2_msgs_handle_frame(void *ctx, uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t len)
{
    ros2_msgs_handle_message((ros2_msgs_ctx_t *)ctx, msg_type, seq, payload, len);
}

void ros2_msgs_init(void)
{
#ifndef UNIT_TEST
    ros2_msgs_load_settings();
    runtime_start_us = esp_timer_get_time();
#endif

#if defined(CONFIG_ROS2_TRANSPORT_UART) && CONFIG_ROS2_TRANSPORT_UART
    uart_bridge_init();
#else
    usb_bridge_init();
    ros2_ctx.write = usb_bridge_write_bytes;
    ros2_ctx.read = usb_bridge_read_bytes;
    framed_link_init(&ros2_ctx.link, usb_bridge_write_bytes, &ros2_ctx, ros2_msgs_handle_frame, &ros2_ctx);
#endif
    // memset(msgs, 0, sizeof(*msgs));
    ESP_LOGI(TAG, "ros2 msgs service init");

    tx_mutex = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(tx_mutex != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    telemetry_timer = xTimerCreate(
        "telemetry_tmr",
        pdMS_TO_TICKS(telemetry_period_ms),
        pdTRUE, /* auto reload */
        NULL,
        telemetry_timer_cb);

    xTaskCreate(ros2_command_task, "ros2_command", 4096, &ros2_ctx, 5, &command_task);
    xTaskCreate(ros2_telemetry_task, "ros2_telemetry", 4096, &ros2_ctx, 5, &telemetry_task);
#ifndef UNIT_TEST
    xTaskCreate(ros2_runtime_task, "ros2_runtime", 3072, NULL, 2, &runtime_task);
#endif
    usb_bridge_set_callback((usb_bridge_cb_t)ros2_msgs_on_rx);

    if (telemetry_enabled)
        xTimerStart(telemetry_timer, 0);
}
