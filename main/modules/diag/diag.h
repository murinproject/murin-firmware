#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "framed_link.h"
#include "rp3_receiver.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t diag_init(void);
void diag_log_rp3(const rp3_signal_sample_t *sample);
size_t diag_get_rp3_logs(rp3_signal_sample_t *records, size_t max_records);

typedef struct {
    int64_t timestamp_us;
    uint8_t msg_type;
    uint8_t seq;
    uint16_t payload_len;
    uint8_t payload[FRAMED_LINK_MAX_PAYLOAD_LEN];
} ros2_diag_message_t;

void diag_log_ros2(uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t payload_len);
size_t diag_get_ros2_logs(ros2_diag_message_t *records, size_t max_records);

#ifdef __cplusplus
}
#endif
