#include "usb_bridge.h"
#include <stdbool.h>

size_t usb_bridge_write_bytes(uint8_t *data, size_t len)
{
    (void)data;
    return len;
}

size_t usb_bridge_read_bytes(uint8_t *data, size_t len)
{
    (void)data;
    (void)len;
    return 0;
}

void usb_bridge_init(void) {}

void usb_bridge_set_callback(usb_bridge_cb_t callback)
{
    (void)callback;
}

void diag_log_ros2(uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t payload_len)
{
    (void)msg_type;
    (void)seq;
    (void)payload;
    (void)payload_len;
}

bool motor_set(float left_mps, float right_mps)
{
    (void)left_mps;
    (void)right_mps;
    return true;
}
