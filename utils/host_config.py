"""Load the shared local configuration for firmware tests and tools."""

from __future__ import annotations

import os
from pathlib import Path
import shutil

import yaml

ROOT_DIR = Path(__file__).resolve().parent.parent
CONFIG_PATH = ROOT_DIR / "config.yaml"
CONFIG_EXAMPLE_PATH = ROOT_DIR / "config.yaml.example"


def ensure_config(
    config_path: Path = CONFIG_PATH,
    example_path: Path = CONFIG_EXAMPLE_PATH,
) -> Path:
    """Create a private local config from the tracked example when absent."""
    if config_path.exists():
        return config_path
    try:
        descriptor = os.open(config_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        return config_path
    try:
        with os.fdopen(descriptor, "wb") as destination:
            with example_path.open("rb") as source:
                shutil.copyfileobj(source, destination)
    except BaseException:
        config_path.unlink(missing_ok=True)
        raise
    print(f"Created local configuration: {config_path}")
    return config_path


def load_config(config_path: Path = CONFIG_PATH) -> dict:
    """Return the local YAML configuration as a mapping."""
    path = ensure_config(config_path)
    with path.open("r", encoding="utf-8") as config_file:
        config = yaml.safe_load(config_file) or {}
    if not isinstance(config, dict):
        raise ValueError(f"Configuration in {path} must be a YAML mapping")
    return config
