"""Run pytest and GoogleTest with their native console output."""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
GTEST_ROOT = ROOT / "tests" / "gtest"
GTEST_BUILD = GTEST_ROOT / "build"


def run(command: list[str]) -> tuple[int, str]:
    """Run a command with live native output and retain it for summaries."""
    process = subprocess.Popen(
        command,
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=False,
    )
    output = bytearray()
    assert process.stdout is not None
    # Read raw chunks instead of lines. Pytest uses carriage returns for
    # progress updates, so readline() can wait until the entire run ends.
    for chunk in iter(lambda: os.read(process.stdout.fileno(), 4096), b""):
        output.extend(chunk)
        sys.stdout.buffer.write(chunk)
        sys.stdout.buffer.flush()
    return process.wait(), output.decode("utf-8", errors="replace")


def number(text: str, pattern: str) -> int:
    match = re.search(pattern, text)
    return int(match.group(1)) if match else 0


def main() -> int:
    pytest_code, pytest_output = run(
        [sys.executable, "-m", "pytest", "tests/pytest", *sys.argv[1:]]
    )

    gtest_output = ""
    gtest_code, _ = run(["cmake", "-S", str(GTEST_ROOT), "-B", str(GTEST_BUILD)])
    if gtest_code == 0:
        gtest_code, _ = run(["cmake", "--build", str(GTEST_BUILD), "--config", "Release"])
    if gtest_code == 0:
        gtest_code, gtest_output = run(
            ["ctest", "--test-dir", str(GTEST_BUILD), "--output-on-failure", "-C", "Release"]
        )

    pytest_counts = {
        "passed": number(pytest_output, r"(\d+)\s+passed"),
        "failed": number(pytest_output, r"(\d+)\s+failed"),
        "skipped": number(pytest_output, r"(\d+)\s+skipped"),
        "errors": number(pytest_output, r"(\d+)\s+errors?"),
    }
    pytest_counts["total"] = sum(pytest_counts.values())

    gtest_counts = {"passed": 0, "failed": 0, "skipped": 0, "errors": 0, "total": 0}
    gtest_summary = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", gtest_output)
    if gtest_summary:
        gtest_counts["failed"] = int(gtest_summary.group(2))
        gtest_counts["total"] = int(gtest_summary.group(3))
        gtest_counts["passed"] = gtest_counts["total"] - gtest_counts["failed"]
    gtest_counts["skipped"] = number(gtest_output, r"(\d+) tests? not run")

    print("\n=== Test Summary ===")
    for name, counts, code in (("pytest", pytest_counts, pytest_code), ("gtest", gtest_counts, gtest_code)):
        result = "PASSED" if code == 0 else "FAILED"
        print(
            f"{name:<10}: {result} (total={counts['total']}, passed={counts['passed']}, "
            f"failed={counts['failed']}, skipped={counts['skipped']}, errors={counts['errors']})"
        )

    return int(pytest_code != 0 or gtest_code != 0)


if __name__ == "__main__":
    raise SystemExit(main())
