#include "ros2_msgs.h"

#include <string.h>
#ifdef UNIT_TEST
#include "ros2_msgs_host_stubs.h"
#else
#include "battery.h"
#include "board_led.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "sdkconfig.h"
#endif

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

static ros2_msgs_ctx_t ros2_ctx;

static TimerHandle_t telemetry_timer = NULL;

static volatile bool telemetry_enabled = true;
// static ros2_msgs_ctx_t ros2_msgs_ctx;
static TaskHandle_t command_task = NULL;
static TaskHandle_t telemetry_task = NULL;

static size_t ros2_msgs_read(ros2_msgs_ctx_t *msgs, uint8_t *data, size_t len);
static void ros2_msgs_handle_frame(void *ctx, uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t len);

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
    framed_link_send_frame(&msgs->link, msg_type, seq, payload, payload_len);
}

static void ros2_msgs_send_ack(ros2_msgs_ctx_t *msgs, uint8_t seq)
{
    framed_link_send_ack(&msgs->link, seq);
}

static void ros2_msgs_send_nack(ros2_msgs_ctx_t *msgs, uint8_t seq, uint8_t err)
{
    framed_link_send_nack(&msgs->link, seq, err);
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
    switch (msg_type)
    {
    case ROS2_MSG_HEARTBEAT:
        led_set(8, 0, 8);
        ESP_LOGI(TAG, "Received HEARTBEAT seq=%u", seq);
        ros2_msgs_send_ack(msgs, seq);
        break;

    case ROS2_MSG_CMD_MOTOR:
        led_set(8, 8, 0);
        if (len == 4)
        {
            const int16_t left = (int16_t)(payload[0] | (payload[1] << 8));
            const int16_t right = (int16_t)(payload[2] | (payload[3] << 8));
            ESP_LOGI(TAG, "Received CMD_MOTOR seq=%u left=%d right=%d", seq, left, right);
            ros2_msgs_send_ack(msgs, seq);
        }
        else
        {
            ros2_msgs_send_nack(msgs, seq, ROS2_MSG_ERR_LEN);
        }
        break;

    case ROS2_MSG_CMD_SERVO:
        led_set(0, 0, 16);
        if (len == 3)
        {
            const uint8_t channel = payload[0];
            const uint16_t pulse = payload[1] | (payload[2] << 8);
            ESP_LOGI(TAG, "Received CMD_SERVO seq=%u channel=%u pulse=%u", seq, channel, pulse);
            ros2_msgs_send_ack(msgs, seq);
        }
        else
        {
            ros2_msgs_send_nack(msgs, seq, ROS2_MSG_ERR_LEN);
        }
        break;

    case ROS2_MSG_CMD_CONFIG:
        led_set(0, 8, 8);
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
                telemetry_enabled = (value != 0);
                if (telemetry_timer == NULL)
                    break;

                if (telemetry_enabled)
                    xTimerStart(telemetry_timer, 0);
                else
                    xTimerStop(telemetry_timer, 0);

                ESP_LOGI(TAG, "Telemetry %s", telemetry_enabled ? "enabled" : "disabled");
                break;

            case ROS2_CFG_TELEM_RATE_MS:
                if (value >= 10 && value <= 5000)
                {
                    uint32_t telemetry_period_ms = value;
                    ESP_LOGI(TAG, "Telemetry rate set to %u ms", telemetry_period_ms);
                }
                else
                {
                    ros2_msgs_send_nack(msgs, seq, ROS2_MSG_ERR_RANGE);
                    return;
                }
                break;

            case ROS2_CFG_TELEM_MASK:
                uint32_t telemetry_mask = (uint32_t)value;
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

    telemetry_timer = xTimerCreate(
        "telemetry_tmr",
        pdMS_TO_TICKS(20),
        pdTRUE, /* auto reload */
        NULL,
        telemetry_timer_cb);

    xTaskCreate(ros2_command_task, "ros2_command", 4096, &ros2_ctx, 5, &command_task);
    xTaskCreate(ros2_telemetry_task, "ros2_telemetry", 4096, &ros2_ctx, 5, &telemetry_task);
    usb_bridge_set_callback((usb_bridge_cb_t)ros2_msgs_on_rx);

    if (telemetry_enabled)
        xTimerStart(telemetry_timer, 0);
}
