#!/usr/bin/env python3
"""
test_protocol.py - Pytest for ESP32 USB protocol testing.

Usage:
    pytest pytest/test_protocol.py -v
"""

import struct

import pytest

from protocol_common import (
    CFG_TELEM_ENABLE,
    CFG_TELEM_MASK,
    CFG_TELEM_RATE_MS,
    ERR_CFG,
    ERR_LEN,
    ERR_RANGE,
    ERR_TYPE,
    MSG_CMD_CONFIG,
    MSG_CMD_MOTOR,
    MSG_CMD_SERVO,
    MSG_HEARTBEAT,
    encode_config,
    encode_motor,
    encode_servo,
)


def assert_nack(payload: bytes, seq: int, err: int):
    assert payload == bytes([seq, err])


def test_ros2_heartbeat_command(serial_agent):
    seq = serial_agent.send_frame(MSG_HEARTBEAT, b"")
    assert serial_agent.wait_for_ack(seq)


@pytest.mark.parametrize(
    ("left_velocity", "right_velocity"),
    [
        (0.25, -0.25),
        (0, 0),
        (0.5, -0.5),
    ],
)
def test_ros2_motor_command(serial_agent, left_velocity, right_velocity):
    payload = encode_motor(left_velocity, right_velocity)
    seq = serial_agent.send_frame(MSG_CMD_MOTOR, payload)
    assert serial_agent.wait_for_ack(seq)


def test_ros2_motor_command_stuffed_payload(serial_agent):
    # These valid float32 values contain SOF (0xAA) and ESCAPE (0x1B) bytes.
    left_velocity = struct.unpack("<f", bytes.fromhex("AA00003E"))[0]
    right_velocity = struct.unpack("<f", bytes.fromhex("1B0000BE"))[0]
    payload = encode_motor(left_velocity, right_velocity)
    seq = serial_agent.send_frame(MSG_CMD_MOTOR, payload)
    assert serial_agent.wait_for_ack(seq)


def test_motor_command(serial_agent):
    payload = encode_motor(0.25, -0.25)
    seq = serial_agent.send_frame(MSG_CMD_MOTOR, payload)
    assert serial_agent.wait_for_ack(seq)


@pytest.mark.parametrize(
    ("channel", "pulse_us"),
    [
        (0, 1500),
        (1, 1000),
        (2, 2000),
    ],
)
def test_ros2_servo_command(serial_agent, channel, pulse_us):
    payload = encode_servo(channel, pulse_us)
    seq = serial_agent.send_frame(MSG_CMD_SERVO, payload)
    assert serial_agent.wait_for_ack(seq)


def test_servo_command(serial_agent):
    payload = encode_servo(0, 1500)
    seq = serial_agent.send_frame(MSG_CMD_SERVO, payload)
    assert serial_agent.wait_for_ack(seq)


@pytest.mark.parametrize(
    ("key", "value"),
    [
        (CFG_TELEM_ENABLE, 0),
        (CFG_TELEM_ENABLE, 1),
        (CFG_TELEM_RATE_MS, 10),
        (CFG_TELEM_RATE_MS, 5000),
        (CFG_TELEM_MASK, -1),
    ],
)
def test_ros2_config_command(serial_agent, key, value):
    payload = encode_config(key, value)
    seq = serial_agent.send_frame(MSG_CMD_CONFIG, payload)
    assert serial_agent.wait_for_ack(seq)


def test_config_command(serial_agent):
    payload = encode_config(CFG_TELEM_ENABLE, 1)
    seq = serial_agent.send_frame(MSG_CMD_CONFIG, payload)
    assert serial_agent.wait_for_ack(seq)


def test_invalid_command(serial_agent):
    seq = serial_agent.send_frame(0xFF, b"test")
    payload = serial_agent.wait_for_nack(seq)
    assert_nack(payload, seq, ERR_TYPE)


def test_invalid_motor_length(serial_agent):
    seq = serial_agent.send_frame(MSG_CMD_MOTOR, b"\x00")
    payload = serial_agent.wait_for_nack(seq)
    assert_nack(payload, seq, ERR_LEN)


def test_ros2_invalid_servo_length(serial_agent):
    seq = serial_agent.send_frame(MSG_CMD_SERVO, b"\x00\x01")
    payload = serial_agent.wait_for_nack(seq)
    assert_nack(payload, seq, ERR_LEN)


def test_ros2_invalid_config_length(serial_agent):
    seq = serial_agent.send_frame(MSG_CMD_CONFIG, b"\x01\x00")
    payload = serial_agent.wait_for_nack(seq)
    assert_nack(payload, seq, ERR_LEN)


def test_ros2_invalid_config_key(serial_agent):
    seq = serial_agent.send_frame(MSG_CMD_CONFIG, encode_config(0xFF, 1))
    payload = serial_agent.wait_for_nack(seq)
    assert_nack(payload, seq, ERR_CFG)


@pytest.mark.parametrize("value", [9, 5001])
def test_ros2_config_rate_out_of_range(serial_agent, value):
    seq = serial_agent.send_frame(
        MSG_CMD_CONFIG, encode_config(CFG_TELEM_RATE_MS, value)
    )
    payload = serial_agent.wait_for_nack(seq)
    assert_nack(payload, seq, ERR_RANGE)
