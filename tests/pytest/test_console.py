"""UART shell console integration tests."""

import re
import time

import pytest
import serial


class ConsoleSerial:
    """Small serial helper for the line-oriented UART shell."""

    def __init__(self, port, baudrate):
        self.serial = serial.Serial(port, baudrate, timeout=0.05)

    def close(self):
        self.serial.close()

    def read_until(self, pattern, timeout=2.0):
        expression = re.compile(pattern, re.DOTALL)
        deadline = time.monotonic() + timeout
        data = bytearray()

        while time.monotonic() < deadline:
            chunk = self.serial.read(self.serial.in_waiting or 1)
            if chunk:
                data.extend(chunk)
                text = data.decode("utf-8", errors="replace")
                if expression.search(text):
                    return text

        text = data.decode("utf-8", errors="replace")
        raise AssertionError(
            f"Timed out waiting for /{pattern}/; received: {text!r}"
        )

    def command(self, command, timeout=2.0):
        self.serial.write(command.encode("ascii") + b"\r")
        return self.read_until(r"shell> ", timeout)


@pytest.fixture
def console_serial(test_config):
    port = test_config.get("console_port")
    if not port:
        pytest.skip("No console_port configured in tests/pytest/test_config.yaml")

    # shell_uart.c defaults to CONFIG_ROS2_TRANSPORT_UART_BAUD_RATE, which
    # defaults to 115200. Keep this independent from the USB protocol baudrate.
    baudrate = int(test_config.get("console_baudrate", 115200))
    try:
        console = ConsoleSerial(port, baudrate)
    except serial.SerialException as exc:
        pytest.skip(f"Unable to open console port {port}: {exc}")

    try:
        # Provoke a prompt if the startup prompt was emitted before the port
        # was opened.
        console.serial.write(b"\r")
        console.read_until(r"shell> ")
        yield console
    finally:
        console.close()


def test_shell_commands(console_serial):
    help_output = console_serial.command("help")
    assert re.search(r"Available commands:", help_output)
    assert re.search(r"(?m)^\s*stats\s+Print system status", help_output)
    assert re.search(r"(?m)^\s*diag rp3 <1-20>\s+Print latest RP3 telemetry logs", help_output)
    assert re.search(r"(?m)^\s*clear\s+Clear the terminal screen", help_output)

    stats_output = console_serial.command("stats")
    assert re.search(r"=== System status ===", stats_output)
    assert re.search(r"(?m)^Chip:\s+.+ revision \d+, \d+ core\(s\)", stats_output)
    assert re.search(r"(?m)^Uptime:\s+\d+ ms", stats_output)
    assert re.search(r"(?m)^Heap:\s+free=\d+", stats_output)

    diag_output = console_serial.command("diag rp3")
    assert re.search(r"Usage: diag rp3 <1-20>", diag_output)
    assert re.search(r"Error: command failed \(1\)", diag_output)

    clear_output = console_serial.command("clear")
    assert re.search(r"\x1b\[2J\x1b\[H", clear_output)
