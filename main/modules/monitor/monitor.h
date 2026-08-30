#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void monitor_init(void);
void monitor_set_ros2_enabled(bool enabled);
bool monitor_is_ros2_enabled(void);
void monitor_set_rp3_enabled(bool enabled);
bool monitor_is_rp3_enabled(void);
void monitor_set_diff_drive_enabled(bool enabled);
bool monitor_is_diff_drive_enabled(void);

#ifdef __cplusplus
}
#endif
