#include "diag.h"

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/task.h"

#define DIAG_RP3_LOG_CAPACITY 4096

static const char *TAG = "diag";
static rp3_signal_sample_t *s_rp3_log;
static size_t s_rp3_log_head;
static size_t s_rp3_log_count;
static portMUX_TYPE s_diag_lock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t diag_init(void)
{
    s_rp3_log = heap_caps_calloc(DIAG_RP3_LOG_CAPACITY,
                                 sizeof(*s_rp3_log),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_rp3_log == NULL)
    {
        ESP_LOGE(TAG, "Unable to allocate RP3 log in PSRAM");
        return ESP_ERR_NO_MEM;
    }

    s_rp3_log_head = 0;
    s_rp3_log_count = 0;
    ESP_LOGI(TAG, "RP3 diagnostic log allocated in PSRAM (%u records)",
             (unsigned)DIAG_RP3_LOG_CAPACITY);
    return ESP_OK;
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
