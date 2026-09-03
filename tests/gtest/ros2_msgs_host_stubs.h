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
typedef void *QueueHandle_t;

#define pdTRUE 1
#define portMAX_DELAY ((TickType_t)~0u)
#define CONFIG_TINYUSB_CDC_RX_BUFSIZE 1024
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_ERR_NO_MEM -2
#define ESP_ERROR_CHECK(expr) (void)(expr)

typedef struct {
  float voltage;
  float current;
  float power;
  float energy;
  uint32_t timestamp;
  bool valid;
} battery_data_t;

typedef struct {
  int64_t timestamp_us;
  float acceleration_mps2[3];
  float angular_velocity_rad_s[3];
  float magnetic_field_uT[3];
  float quaternion[4];
  bool data_valid;
} navigation_imu_sample_t;

typedef struct {
  bool imu_valid;
  navigation_imu_sample_t imu;
} navigation_snapshot_t;

typedef void (*navigation_telemetry_callback_t)(const navigation_imu_sample_t *sample);

static inline void navigation_set_telemetry_callback(navigation_telemetry_callback_t callback) { (void)callback; }

static inline esp_err_t battery_fetch_data(battery_data_t *data)
{
  if (data == NULL)
    return ESP_FAIL;
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
  (void)red;
  (void)green;
  (void)blue;
}

#define ESP_LOGI(tag, fmt, ...) (void)0
#define ESP_LOGW(tag, fmt, ...) (void)0
#define ESP_LOGD(tag, fmt, ...) (void)0
#define ESP_LOGE(tag, fmt, ...) (void)0

static inline BaseType_t xTaskCreate(void (*task)(void *), const char *name, uint32_t stack_depth, void *arg,
                                     UBaseType_t priority, TaskHandle_t *task_handle)
{
  (void)task;
  (void)name;
  (void)stack_depth;
  (void)arg;
  (void)priority;
  if (task_handle != NULL)
    *task_handle = (TaskHandle_t)1;
  return pdTRUE;
}

static inline void xTaskNotifyGive(TaskHandle_t task) { (void)task; }

static inline QueueHandle_t xQueueCreate(uint32_t length, uint32_t item_size)
{
  (void)length;
  (void)item_size;
  return (QueueHandle_t)1;
}

static inline BaseType_t xQueueOverwrite(QueueHandle_t queue, const void *item)
{
  (void)queue;
  (void)item;
  return pdTRUE;
}

static inline BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t ticks_to_wait)
{
  (void)queue;
  (void)item;
  (void)ticks_to_wait;
  return 0;
}

static inline uint32_t ulTaskNotifyTake(BaseType_t clear_count_on_exit, TickType_t ticks_to_wait)
{
  (void)clear_count_on_exit;
  (void)ticks_to_wait;
  return 0;
}

static inline TimerHandle_t xTimerCreate(const char *name, TickType_t period, BaseType_t auto_reload, void *timer_id,
                                         void (*callback)(TimerHandle_t))
{
  (void)name;
  (void)period;
  (void)auto_reload;
  (void)timer_id;
  (void)callback;
  return (TimerHandle_t)1;
}

static inline BaseType_t xTimerStart(TimerHandle_t timer, TickType_t ticks_to_wait)
{
  (void)timer;
  (void)ticks_to_wait;
  return pdTRUE;
}

static inline BaseType_t xTimerStop(TimerHandle_t timer, TickType_t ticks_to_wait)
{
  (void)timer;
  (void)ticks_to_wait;
  return pdTRUE;
}

static inline esp_err_t navigation_get_snapshot(navigation_snapshot_t *snapshot)
{
  if (snapshot == NULL)
    return ESP_FAIL;
  snapshot->imu_valid = true;
  snapshot->imu.timestamp_us = 987654321;
  snapshot->imu.acceleration_mps2[0] = 1.0f;
  snapshot->imu.acceleration_mps2[1] = 2.0f;
  snapshot->imu.acceleration_mps2[2] = 3.0f;
  snapshot->imu.angular_velocity_rad_s[0] = 4.0f;
  snapshot->imu.angular_velocity_rad_s[1] = 5.0f;
  snapshot->imu.angular_velocity_rad_s[2] = 6.0f;
  snapshot->imu.magnetic_field_uT[0] = 7.0f;
  snapshot->imu.magnetic_field_uT[1] = 8.0f;
  snapshot->imu.magnetic_field_uT[2] = 9.0f;
  snapshot->imu.quaternion[0] = 0.1f;
  snapshot->imu.quaternion[1] = 0.2f;
  snapshot->imu.quaternion[2] = 0.3f;
  snapshot->imu.quaternion[3] = 0.4f;
  snapshot->imu.data_valid = true;
  return ESP_OK;
}

static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t ticks_to_wait)
{
  (void)semaphore;
  (void)ticks_to_wait;
  return pdTRUE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
  (void)semaphore;
  return pdTRUE;
}

static inline BaseType_t xTimerChangePeriod(TimerHandle_t timer, TickType_t period, TickType_t ticks_to_wait)
{
  (void)timer;
  (void)period;
  (void)ticks_to_wait;
  return pdTRUE;
}
