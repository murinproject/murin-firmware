#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "rp3_receiver.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t diag_init(void);
void diag_log_rp3(const rp3_signal_sample_t *sample);
size_t diag_get_rp3_logs(rp3_signal_sample_t *records, size_t max_records);

#ifdef __cplusplus
}
#endif
