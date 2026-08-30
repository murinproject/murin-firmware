#include "diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

#define DIAG_RP3_LOG_CAPACITY 4096
#define DIAG_ROS2_LOG_CAPACITY 4096
#define DIAG_SYSTEM_LOG_CAPACITY 1024

static const char *TAG = "diag";
static rp3_signal_sample_t *s_rp3_log;
static size_t s_rp3_log_head;
static size_t s_rp3_log_count;
static ros2_diag_message_t *s_ros2_log;
static size_t s_ros2_log_head;
static size_t s_ros2_log_count;
static diag_system_log_t *s_system_log;
static size_t s_system_log_head;
static size_t s_system_log_count;
static vprintf_like_t s_default_vprintf;
static portMUX_TYPE s_diag_lock = portMUX_INITIALIZER_UNLOCKED;

static int diag_log_vprintf(const char *format, va_list args)
{
    char message[DIAG_SYSTEM_LOG_MESSAGE_MAX];
    va_list copy;

    va_copy(copy, args);
    vsnprintf(message, sizeof(message), format, copy);
    va_end(copy);
    diag_log_system(message);

    if (s_default_vprintf != NULL)
        return s_default_vprintf(format, args);
    return vprintf(format, args);
}

void diag_init(void)
{
    s_rp3_log = heap_caps_calloc(DIAG_RP3_LOG_CAPACITY,
                                 sizeof(*s_rp3_log),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_rp3_log == NULL)
    {
        ESP_LOGE(TAG, "Unable to allocate RP3 log in PSRAM");
        return;
    }

    s_ros2_log = heap_caps_calloc(DIAG_ROS2_LOG_CAPACITY,
                                  sizeof(*s_ros2_log),
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_ros2_log == NULL)
    {
        heap_caps_free(s_rp3_log);
        s_rp3_log = NULL;
        ESP_LOGE(TAG, "Unable to allocate ROS2 log in PSRAM");
        return;
    }

    s_system_log = heap_caps_calloc(DIAG_SYSTEM_LOG_CAPACITY,
                                    sizeof(*s_system_log),
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_system_log == NULL)
    {
        heap_caps_free(s_ros2_log);
        s_ros2_log = NULL;
        heap_caps_free(s_rp3_log);
        s_rp3_log = NULL;
        ESP_LOGE(TAG, "Unable to allocate system log in PSRAM");
        return;
    }

    s_rp3_log_head = 0;
    s_rp3_log_count = 0;
    s_ros2_log_head = 0;
    s_ros2_log_count = 0;
    s_system_log_head = 0;
    s_system_log_count = 0;
    s_default_vprintf = esp_log_set_vprintf(diag_log_vprintf);
    ESP_LOGI(TAG, "Diagnostic logs allocated in PSRAM (RP3=%u, ROS2=%u records)",
             (unsigned)DIAG_RP3_LOG_CAPACITY, (unsigned)DIAG_ROS2_LOG_CAPACITY);
}

void diag_log_system(const char *message)
{
    if (s_system_log == NULL || message == NULL)
        return;

    taskENTER_CRITICAL(&s_diag_lock);
    diag_system_log_t *record = &s_system_log[s_system_log_head];
    record->timestamp_us = esp_timer_get_time();
    strncpy(record->message, message, sizeof(record->message) - 1);
    record->message[sizeof(record->message) - 1] = '\0';
    s_system_log_head = (s_system_log_head + 1) % DIAG_SYSTEM_LOG_CAPACITY;
    if (s_system_log_count < DIAG_SYSTEM_LOG_CAPACITY)
        s_system_log_count++;
    taskEXIT_CRITICAL(&s_diag_lock);
}

size_t diag_get_system_logs(diag_system_log_t *records, size_t max_records)
{
    if (s_system_log == NULL || records == NULL || max_records == 0)
        return 0;

    taskENTER_CRITICAL(&s_diag_lock);

    size_t record_count = s_system_log_count < max_records ? s_system_log_count : max_records;
    size_t first = (s_system_log_head + DIAG_SYSTEM_LOG_CAPACITY - record_count) % DIAG_SYSTEM_LOG_CAPACITY;
    for (size_t i = 0; i < record_count; i++)
        records[i] = s_system_log[(first + i) % DIAG_SYSTEM_LOG_CAPACITY];

    taskEXIT_CRITICAL(&s_diag_lock);
    return record_count;
}

void diag_log_rp3(const rp3_signal_sample_t *sample)
{
    if (s_rp3_log == NULL || sample == NULL)
        return;

    taskENTER_CRITICAL(&s_diag_lock);
    s_rp3_log[s_rp3_log_head] = *sample;
    s_rp3_log_head = (s_rp3_log_head + 1) % DIAG_RP3_LOG_CAPACITY;
    if (s_rp3_log_count < DIAG_RP3_LOG_CAPACITY)
        s_rp3_log_count++;
    taskEXIT_CRITICAL(&s_diag_lock);
}

size_t diag_get_rp3_logs(rp3_signal_sample_t *records, size_t max_records)
{
    if (s_rp3_log == NULL || records == NULL || max_records == 0)
        return 0;

    taskENTER_CRITICAL(&s_diag_lock);

    size_t record_count = s_rp3_log_count < max_records ? s_rp3_log_count : max_records;
    size_t first = (s_rp3_log_head + DIAG_RP3_LOG_CAPACITY - record_count) % DIAG_RP3_LOG_CAPACITY;
    for (size_t i = 0; i < record_count; i++)
        records[i] = s_rp3_log[(first + i) % DIAG_RP3_LOG_CAPACITY];

    taskEXIT_CRITICAL(&s_diag_lock);
    return record_count;
}

void diag_log_ros2(uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t payload_len)
{
    if (s_ros2_log == NULL || (payload_len > 0 && payload == NULL))
        return;

    if (payload_len > FRAMED_LINK_MAX_PAYLOAD_LEN)
        payload_len = FRAMED_LINK_MAX_PAYLOAD_LEN;

    taskENTER_CRITICAL(&s_diag_lock);
    ros2_diag_message_t *record = &s_ros2_log[s_ros2_log_head];
    record->timestamp_us = esp_timer_get_time();
    record->msg_type = msg_type;
    record->seq = seq;
    record->payload_len = (uint16_t)payload_len;
    if (payload_len > 0)
        memcpy(record->payload, payload, payload_len);
    s_ros2_log_head = (s_ros2_log_head + 1) % DIAG_ROS2_LOG_CAPACITY;
    if (s_ros2_log_count < DIAG_ROS2_LOG_CAPACITY)
        s_ros2_log_count++;
    taskEXIT_CRITICAL(&s_diag_lock);
}

size_t diag_get_ros2_logs(ros2_diag_message_t *records, size_t max_records)
{
    if (s_ros2_log == NULL || records == NULL || max_records == 0)
        return 0;

    taskENTER_CRITICAL(&s_diag_lock);

    size_t record_count = s_ros2_log_count < max_records ? s_ros2_log_count : max_records;
    size_t first = (s_ros2_log_head + DIAG_ROS2_LOG_CAPACITY - record_count) % DIAG_ROS2_LOG_CAPACITY;
    for (size_t i = 0; i < record_count; i++)
        records[i] = s_ros2_log[(first + i) % DIAG_ROS2_LOG_CAPACITY];

    taskEXIT_CRITICAL(&s_diag_lock);
    return record_count;
}
