#!/usr/bin/env python3
"""Send randomized differential-drive motor commands to the controller.

The controller expects ``CMD_MOTOR`` payloads containing two little-endian
signed 16-bit values: left and right motor velocity/PWM commands.
"""

import argparse
import random
import sys
import time
from pathlib import Path

import serial
import yaml


# Keep this tool runnable from the repository root as well as from ``tools``.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "utils"))

from protocol_common import MSG_CMD_MOTOR, build_frame, encode_motor  # noqa: E402


CONFIG_PATH = Path(__file__).with_name("config.yaml")


def load_config() -> dict:
    try:
        with CONFIG_PATH.open("r", encoding="utf-8") as config_file:
            return yaml.safe_load(config_file) or {}
    except FileNotFoundError:
        return {}


def build_arg_parser() -> argparse.ArgumentParser:
    config = load_config()
    parser = argparse.ArgumentParser(
        description="Continuously send random CMD_MOTOR differential-drive commands."
    )
    parser.add_argument("--port", default=config.get("port"), help="Serial port, e.g. COM11")
    parser.add_argument(
        "--baudrate", type=int, default=config.get("baudrate", 2_000_000)
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=100.0,
        metavar="MILLISECONDS",
        help="Delay between commands in milliseconds (default: 100)",
    )
    parser.add_argument(
        "--max-velocity",
        type=int,
        default=512,
        metavar="PWM",
        help="Maximum absolute motor command (default: 512)",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=None,
        metavar="SECONDS",
        help="Stop after this many seconds; run until Ctrl+C when omitted",
    )
    parser.add_argument("--once", action="store_true", help="Send one random command and stop")
    parser.add_argument("--seed", type=int, help="Seed the random generator for repeatable output")
    return parser


def validate_args(args: argparse.Namespace, parser: argparse.ArgumentParser) -> None:
    if not args.port:
        parser.error(f"No serial port configured. Pass --port or set it in {CONFIG_PATH}.")
    if args.interval <= 0:
        parser.error("--interval must be greater than zero")
    if not 0 <= args.max_velocity <= 0x7FFF:
        parser.error("--max-velocity must be in the range 0..32767")
    if args.duration is not None and args.duration <= 0:
        parser.error("--duration must be greater than zero")


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()
    validate_args(args, parser)
    generator = random.Random(args.seed)
    sequence = 0
    started = time.monotonic()
    ser = None

    try:
        ser = serial.Serial(args.port, args.baudrate, timeout=0.05)
        ser.dtr = True
        ser.rts = False
        print(f"[SERIAL] Connected to {args.port} @ {args.baudrate}")

        while True:
            if args.duration is not None and time.monotonic() - started >= args.duration:
                break

            left = generator.randint(-args.max_velocity, args.max_velocity)
            right = generator.randint(-args.max_velocity, args.max_velocity)
            payload = encode_motor(left, right)
            ser.write(build_frame(MSG_CMD_MOTOR, sequence, payload))
            print(f"[TX] CMD_MOTOR seq={sequence} left={left} right={right}")
            sequence = (sequence + 1) & 0xFF

            if args.once:
                break
            time.sleep(args.interval / 1000.0)
    except KeyboardInterrupt:
        print("\n[SERIAL] Stopped")
    except serial.SerialException as exc:
        print(f"[SERIAL] {exc}", file=sys.stderr)
        return 1
    finally:
        # Always leave the drive stopped, including on Ctrl+C.
        if ser is not None and ser.is_open:
            ser.write(build_frame(MSG_CMD_MOTOR, sequence, encode_motor(0, 0)))
            ser.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
