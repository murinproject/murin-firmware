#pragma once

#include "framed_link.h"
#include "usb_bridge.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ROS2 messages transported over custom serial protocol.
 *
 * Frame format:
 *   | SOF | Type | Seq | Length | Payload | CRC16 |
 *
 * Field definitions:
 *   SOF     : 1 byte  (always 0xAA)
 *   Type    : 1 byte  Message type ID
 *   Seq     : 1 byte  Sequence counter
 *   Length  : 2 bytes Payload length in bytes (little-endian)
 *   Payload : Variable length, byte-stuffed for frame transparency
 *   CRC16   : 2 bytes CRC-16 checksum (little-endian)
 *
 * CRC coverage:
 *   Type + Seq + Length + Payload
 *   (SOF is excluded)
 */

typedef size_t (*ros2_msgs_write_fn_t)(uint8_t *data, size_t len);
typedef size_t (*ros2_msgs_read_fn_t)(uint8_t *data, size_t len);
typedef void (*ros2_msgs_monitor_fn_t)(uint8_t msg_type, uint8_t seq, const uint8_t *payload, size_t payload_len);

typedef enum {
  ROS2_MSG_HEARTBEAT = 0x00,
  ROS2_MSG_CMD_MOTOR = 0x01,
  ROS2_MSG_CMD_SERVO = 0x02,
  ROS2_MSG_TELEMETRY = 0x03,
  ROS2_MSG_CMD_CONFIG = 0x10,
  ROS2_MSG_ACK = 0x7E,
  ROS2_MSG_NACK = 0x7F,
} ros2_msg_type_t;

typedef enum {
  ROS2_CFG_TELEM_ENABLE = 0x01,
  ROS2_CFG_TELEM_RATE_MS = 0x02,
  ROS2_CFG_TELEM_MASK = 0x03,
  ROS2_CFG_TELEM_TIMEOUT_MS = 0x04,
} ros2_config_id_t;

typedef enum {
  ROS2_MSG_ERR_CRC = 0x01,
  ROS2_MSG_ERR_LEN = 0x02,
  ROS2_MSG_ERR_TYPE = 0x03,
  ROS2_MSG_ERR_CFG = 0x04,
  ROS2_MSG_ERR_RANGE = 0x05,
} ros2_error_code_t;

typedef struct {
  framed_link_t link;
  uint8_t tx_seq;
  ros2_msgs_write_fn_t write;
  ros2_msgs_read_fn_t read;
} ros2_msgs_ctx_t;

void ros2_msgs_init(void);
void ros2_msgs_on_rx(void);
void ros2_msgs_set_monitor(ros2_msgs_monitor_fn_t monitor);
void ros2_msgs_set_telemetry_enabled(bool enabled);
bool ros2_msgs_get_telemetry_enabled(void);
uint64_t ros2_msgs_get_total_runtime_ms(void);
void ros2_msgs_send_frame(ros2_msgs_ctx_t *msgs, uint8_t msg_type, uint8_t seq, const uint8_t *payload,
                          size_t payload_len);
void ros2_msgs_send_telemetry(ros2_msgs_ctx_t *msgs, uint8_t seq);

#ifdef UNIT_TEST
void ros2_msgs_test_set_write(ros2_msgs_write_fn_t write);
void ros2_msgs_test_process_frame(uint8_t *data, size_t len);
#endif

#ifdef __cplusplus
}
#endif
