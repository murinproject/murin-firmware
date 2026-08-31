#include "monitor.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "diag.h"
#include "diff_drive.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "ros2_msgs.h"
#include "shell_uart.h"

#define ROS2_MONITOR_QUEUE_LENGTH 16
#define ROS2_MONITOR_TASK_STACK_SIZE 4096
#define ROS2_MONITOR_TASK_PRIORITY 4
#define RP3_MONITOR_UPDATE_PERIOD_US 100000
#define RP3_CHANNEL_VALUE_MIN 172
#define RP3_CHANNEL_VALUE_MAX 1811

typedef enum {
  MONITOR_EVENT_ROS2,
  MONITOR_EVENT_RP3,
  MONITOR_EVENT_DIFF_DRIVE,
} monitor_event_type_t;

typedef struct {
  monitor_event_type_t type;

  union {
    ros2_diag_message_t ros2;
    rp3_signal_sample_t rp3;
    diff_drive_state_t diff_drive;
  } data;
} monitor_event_t;

static const char *TAG = "monitor";
static QueueHandle_t s_monitor_queue;
static volatile bool s_monitor_enabled;
static volatile bool s_rp3_monitor_enabled;
static volatile bool s_diff_drive_monitor_enabled;
static volatile bool s_rp3_display_started;

static void ros2_monitor_callback(uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t payload_len)
{
  if (!s_monitor_enabled || s_monitor_queue == NULL)
    return;

  monitor_event_t event = {
      .type = MONITOR_EVENT_ROS2,
      .data.ros2 =
          {
              .timestamp_us = esp_timer_get_time(),
              .msg_type = msg_type,
              .seq = seq,
              .payload_len =
                  (uint16_t)(payload_len > FRAMED_LINK_MAX_PAYLOAD_LEN ? FRAMED_LINK_MAX_PAYLOAD_LEN : payload_len),
          },
  };
  if (event.data.ros2.payload_len > 0)
    memcpy(event.data.ros2.payload, payload, event.data.ros2.payload_len);

  /* Never block the ROS2 command task on shell output or a full queue. */
  (void)xQueueSend(s_monitor_queue, &event, 0);
}

static void rp3_monitor_callback(const rp3_signal_sample_t *sample)
{
  if (!s_rp3_monitor_enabled || s_monitor_queue == NULL || sample == NULL)
    return;

  monitor_event_t event = {
      .type = MONITOR_EVENT_RP3,
      .data.rp3 = *sample,
  };
  (void)xQueueSend(s_monitor_queue, &event, 0);
}

static void diff_drive_monitor_callback(const diff_drive_state_t *state)
{
  if (!s_diff_drive_monitor_enabled || s_monitor_queue == NULL || state == NULL)
    return;

  monitor_event_t event = {
      .type = MONITOR_EVENT_DIFF_DRIVE,
      .data.diff_drive = *state,
  };
  (void)xQueueSend(s_monitor_queue, &event, 0);
}

static void append_bar(char *line, size_t line_size, size_t *length, uint16_t value)
{
  const uint16_t clamped = value < RP3_CHANNEL_VALUE_MIN
                               ? RP3_CHANNEL_VALUE_MIN
                               : (value > RP3_CHANNEL_VALUE_MAX ? RP3_CHANNEL_VALUE_MAX : value);
  const unsigned filled =
      ((unsigned)(clamped - RP3_CHANNEL_VALUE_MIN) * 4U) / (RP3_CHANNEL_VALUE_MAX - RP3_CHANNEL_VALUE_MIN);
  if (*length >= line_size)
    return;
  *length += (size_t)snprintf(line + *length, line_size - *length, "|");
  for (unsigned i = 0; i < 4; i++) {
    if (*length >= line_size)
      return;
    *length += (size_t)snprintf(line + *length, line_size - *length, "%c", i < filled ? '-' : ' ');
  }
}

static void ros2_monitor_task(void *arg)
{
  (void)arg;
  monitor_event_t event;
  char line[320];
  int64_t last_rp3_update_us = 0;

  while (1) {
    if (xQueueReceive(s_monitor_queue, &event, portMAX_DELAY) != pdTRUE)
      continue;

    if (event.type == MONITOR_EVENT_RP3) {
      if (!s_rp3_monitor_enabled)
        continue;

      /* Keep only the newest pending RP3 sample.  Link statistics and
       * RC frames can arrive back-to-back for the same radio update. */
      monitor_event_t pending;
      while (xQueueReceive(s_monitor_queue, &pending, 0) == pdTRUE) {
        if (pending.type == MONITOR_EVENT_RP3)
          event = pending;
      }

      const int64_t now_us = esp_timer_get_time();
      if (now_us - last_rp3_update_us < RP3_MONITOR_UPDATE_PERIOD_US)
        continue;
      last_rp3_update_us = now_us;

      size_t length = (size_t)snprintf(line, sizeof(line),
                                       "%s\r\033[2KLINK UL LQ=%3u RSSI=%4d SNR=%3d | "
                                       "DL LQ=%3u RSSI=%4d SNR=%3d\r\n\033[2KCH ",
                                       s_rp3_display_started ? "\033[1A" : "",
                                       (unsigned)event.data.rp3.uplink_link_quality, event.data.rp3.uplink_rssi_dbm,
                                       event.data.rp3.uplink_snr_db, (unsigned)event.data.rp3.downlink_link_quality,
                                       event.data.rp3.downlink_rssi_dbm, event.data.rp3.downlink_snr_db);
      s_rp3_display_started = true;
      for (size_t i = 0; i < 8; i++) {
        append_bar(line, sizeof(line), &length, event.data.rp3.rc_channels_valid ? event.data.rp3.rc_channels[i] : 0);
      }
      if (length < sizeof(line))
        length += (size_t)snprintf(line + length, sizeof(line) - length, "|\r");
      shell_write(line);
      continue;
    }

    if (event.type == MONITOR_EVENT_DIFF_DRIVE) {
      const diff_drive_state_t *state = &event.data.diff_drive;
      shell_printf("[DIFF] PWM=%3u %3u %3u %3u  BRAKE=%1u %1u  DIR=%1u %1u\r\n", (unsigned)state->pwm_percent[0],
                   (unsigned)state->pwm_percent[1], (unsigned)state->pwm_percent[2], (unsigned)state->pwm_percent[3],
                   state->brake[0] ? 1U : 0U, state->brake[1] ? 1U : 0U, state->direction[0] ? 1U : 0U,
                   state->direction[1] ? 1U : 0U);
      continue;
    }

    if (!s_monitor_enabled)
      continue;

    const ros2_diag_message_t *message = &event.data.ros2;

    int length =
        snprintf(line, sizeof(line), "[ROS2] %" PRId64 " us type=0x%02X seq=%u len=%u payload=", message->timestamp_us,
                 (unsigned)message->msg_type, (unsigned)message->seq, (unsigned)message->payload_len);
    if (length > 0)
      shell_write(line);

    for (size_t i = 0; i < message->payload_len; i++) {
      char byte[3];
      snprintf(byte, sizeof(byte), "%02X", (unsigned)message->payload[i]);
      shell_write(byte);
    }
    shell_write("\r\n");
  }
}

void monitor_init(void)
{
  if (s_monitor_queue != NULL)
    return;

  s_monitor_queue = xQueueCreate(ROS2_MONITOR_QUEUE_LENGTH, sizeof(monitor_event_t));
  if (s_monitor_queue == NULL) {
    ESP_LOGE(TAG, "Unable to create ROS2 monitor queue");
    return;
  }

  ros2_msgs_set_monitor(ros2_monitor_callback);
  rp3_receiver_set_monitor(rp3_monitor_callback);
  motor_set_monitor_callback(diff_drive_monitor_callback);
  if (xTaskCreate(ros2_monitor_task, "monitor", ROS2_MONITOR_TASK_STACK_SIZE, NULL, ROS2_MONITOR_TASK_PRIORITY, NULL) !=
      pdPASS) {
    ESP_LOGE(TAG, "Unable to create ROS2 monitor task");
    vQueueDelete(s_monitor_queue);
    s_monitor_queue = NULL;
    ros2_msgs_set_monitor(NULL);
    rp3_receiver_set_monitor(NULL);
    motor_set_monitor_callback(NULL);
  }
}

void monitor_set_ros2_enabled(bool enabled)
{
  s_monitor_enabled = enabled;
  if (!enabled && s_monitor_queue != NULL)
    xQueueReset(s_monitor_queue);
}

void monitor_set_rp3_enabled(bool enabled)
{
  s_rp3_monitor_enabled = enabled;
  if (!enabled)
    s_rp3_display_started = false;
  if (!enabled && s_monitor_queue != NULL)
    xQueueReset(s_monitor_queue);
}

bool monitor_is_rp3_enabled(void) { return s_rp3_monitor_enabled; }

bool monitor_is_ros2_enabled(void) { return s_monitor_enabled; }

void monitor_set_diff_drive_enabled(bool enabled)
{
  s_diff_drive_monitor_enabled = enabled;
  if (!enabled && s_monitor_queue != NULL)
    xQueueReset(s_monitor_queue);
}

bool monitor_is_diff_drive_enabled(void) { return s_diff_drive_monitor_enabled; }
