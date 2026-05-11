"""Pytest configuration for pytest tests."""

import time
import sys
from pathlib import Path

import pytest
import yaml


# Add utils directory to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent / "utils"))

from protocol_serial import SerialAgent

CONFIG_PATH = Path(__file__).with_name("test_config.yaml")


@pytest.fixture(scope="session")
def test_config():
    try:
        with CONFIG_PATH.open("r", encoding="utf-8") as f:
            return yaml.safe_load(f) or {}
    except FileNotFoundError:
        return {}


@pytest.fixture(scope="session")
def serial_agent(test_config):
    port = test_config.get("port")
    if not port:
        pytest.skip(f"No serial port configured in {CONFIG_PATH}")

    baudrate = test_config.get("baudrate", 115200)
    agent = SerialAgent(port, baudrate)
    agent.connect()
    agent.start_rx()
    time.sleep(0.2)

    yield agent
    agent.close()
