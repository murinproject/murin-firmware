#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1

typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef unsigned int TickType_t;
typedef void *TaskHandle_t;
typedef void *TimerHandle_t;
typedef void *SemaphoreHandle_t;

#define pdTRUE 1
#define portMAX_DELAY ((TickType_t)~0u)
#define CONFIG_TINYUSB_CDC_RX_BUFSIZE 1024
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_ERR_NO_MEM -2
#define ESP_ERROR_CHECK(expr) (void)(expr)

typedef struct
{
    float voltage;
    float current;
    float power;
    float energy;
    uint32_t timestamp;
    bool valid;
} battery_data_t;

static inline esp_err_t battery_fetch_data(battery_data_t *data)
{
    if (data == NULL) return ESP_FAIL;
    data->valid = true;
    data->timestamp = 123456789U;
    data->voltage = 12.34f;
    data->current = 1.23f;
    data->power = 15.2f;
    data->energy = 123.4f;
    return ESP_OK;
}

static inline void led_set(uint32_t red, int32_t green, int32_t blue)
{
    (void)red; (void)green; (void)blue;
}

#define ESP_LOGI(tag, fmt, ...) (void)0
#define ESP_LOGW(tag, fmt, ...) (void)0
#define ESP_LOGD(tag, fmt, ...) (void)0

static inline BaseType_t xTaskCreate(void (*task)(void *), const char *name,
                                     uint32_t stack_depth, void *arg,
                                     UBaseType_t priority, TaskHandle_t *task_handle)
{
    (void)task; (void)name; (void)stack_depth; (void)arg; (void)priority;
    if (task_handle != NULL) *task_handle = (TaskHandle_t)1;
    return pdTRUE;
}

static inline void xTaskNotifyGive(TaskHandle_t task) { (void)task; }
static inline uint32_t ulTaskNotifyTake(BaseType_t clear_count_on_exit, TickType_t ticks_to_wait)
{
    (void)clear_count_on_exit; (void)ticks_to_wait; return 0;
}
static inline TimerHandle_t xTimerCreate(const char *name, TickType_t period,
                                         BaseType_t auto_reload, void *timer_id,
                                         void (*callback)(TimerHandle_t))
{
    (void)name; (void)period; (void)auto_reload; (void)timer_id; (void)callback;
    return (TimerHandle_t)1;
}
static inline BaseType_t xTimerStart(TimerHandle_t timer, TickType_t ticks_to_wait)
{
    (void)timer; (void)ticks_to_wait; return pdTRUE;
}
static inline BaseType_t xTimerStop(TimerHandle_t timer, TickType_t ticks_to_wait)
{
    (void)timer; (void)ticks_to_wait; return pdTRUE;
}

static inline SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    return (SemaphoreHandle_t)1;
}

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t ticks_to_wait)
{
    (void)semaphore; (void)ticks_to_wait; return pdTRUE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    (void)semaphore; return pdTRUE;
}

static inline BaseType_t xTimerChangePeriod(TimerHandle_t timer, TickType_t period, TickType_t ticks_to_wait)
{
    (void)timer; (void)period; (void)ticks_to_wait; return pdTRUE;
}
