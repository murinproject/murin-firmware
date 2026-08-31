#pragma once

#include <stddef.h>
#include <stdint.h>

#include "battery.h"
#include "esp_err.h"
#include "framed_link.h"
#include "rp3_receiver.h"

#ifdef __cplusplus
extern "C" {
#endif

void diag_init(void);
void diag_log_rp3(const rp3_signal_sample_t *sample);
size_t diag_get_rp3_logs(rp3_signal_sample_t *records, size_t max_records);

typedef struct {
  int64_t timestamp_us;
  esp_err_t status;
  battery_data_t data;
} battery_diag_record_t;

size_t diag_get_battery_logs(battery_diag_record_t *records, size_t max_records);

typedef struct {
  int64_t timestamp_us;
  uint8_t msg_type;
  uint8_t seq;
  uint16_t payload_len;
  uint8_t payload[FRAMED_LINK_MAX_PAYLOAD_LEN];
} ros2_diag_message_t;

void diag_log_ros2(uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t payload_len);
size_t diag_get_ros2_logs(ros2_diag_message_t *records, size_t max_records);

#define DIAG_SYSTEM_LOG_MESSAGE_MAX 192

typedef struct {
  int64_t timestamp_us;
  char message[DIAG_SYSTEM_LOG_MESSAGE_MAX];
} diag_system_log_t;

void diag_log_system(const char *message);
size_t diag_get_system_logs(diag_system_log_t *records, size_t max_records);

#ifdef __cplusplus
}
#endif
