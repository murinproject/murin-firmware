"""Run pytest, GoogleTest, and LLVM coverage with native console output."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
GTEST_ROOT = ROOT / "tests" / "gtest"
GTEST_BUILD = GTEST_ROOT / "build"
GTEST_COVERAGE_BUILD = GTEST_ROOT / "build-coverage"
ROS2_MSGS_SOURCE = ROOT / "main" / "modules" / "ros2" / "ros2_msgs.c"
FLASH_STORAGE_SOURCE = ROOT / "main" / "modules" / "nvs" / "flash_storage.c"


def run(command: list[str], env: dict[str, str] | None = None) -> tuple[int, str]:
    """Run a command with live native output and retain it for summaries."""
    sys.stdout.flush()
    process = subprocess.Popen(
        command,
        cwd=ROOT,
        env=env,
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


def executable(build_dir: Path, name: str) -> Path:
    """Return the executable path produced by the single-config Ninja build."""
    suffix = ".exe" if os.name == "nt" else ""
    return build_dir / f"{name}{suffix}"


def run_coverage() -> int:
    """Build instrumented GTests and generate terminal and HTML reports."""
    required_tools = ("clang", "clang++", "ninja", "llvm-profdata", "llvm-cov")
    tools = {name: shutil.which(name) for name in required_tools}
    missing = [name for name, path in tools.items() if path is None]
    if missing:
        print(f"Coverage unavailable: missing tools: {', '.join(missing)}")
        return 1

    configure_command = [
        "cmake",
        "-S",
        str(GTEST_ROOT),
        "-B",
        str(GTEST_COVERAGE_BUILD),
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_C_FLAGS=-fprofile-instr-generate -fcoverage-mapping",
        "-DCMAKE_CXX_FLAGS=-fprofile-instr-generate -fcoverage-mapping",
    ]
    googletest_source = GTEST_BUILD / "_deps" / "googletest-src"
    if googletest_source.is_dir():
        configure_command.append(
            f"-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST={googletest_source}"
        )

    configure_env = os.environ.copy()
    configure_env["CC"] = tools["clang"]
    configure_env["CXX"] = tools["clang++"]
    code, _ = run(configure_command, env=configure_env)
    if code != 0:
        return code
    code, _ = run(["cmake", "--build", str(GTEST_COVERAGE_BUILD)])
    if code != 0:
        return code

    for profile in GTEST_COVERAGE_BUILD.glob("*.profraw"):
        profile.unlink()

    coverage_env = os.environ.copy()
    coverage_env["LLVM_PROFILE_FILE"] = str(
        GTEST_COVERAGE_BUILD / "%p-%m.profraw"
    )
    code, _ = run(
        [
            "ctest",
            "--test-dir",
            str(GTEST_COVERAGE_BUILD),
            "--output-on-failure",
        ],
        env=coverage_env,
    )
    if code != 0:
        return code

    profiles = sorted(GTEST_COVERAGE_BUILD.glob("*.profraw"))
    if not profiles:
        print("Coverage failed: no raw profiles were generated")
        return 1

    profile_data = GTEST_COVERAGE_BUILD / "coverage.profdata"
    code, _ = run(
        [
            tools["llvm-profdata"],
            "merge",
            "-sparse",
            *(str(profile) for profile in profiles),
            "-o",
            str(profile_data),
        ]
    )
    if code != 0:
        return code

    ros2_executable = executable(GTEST_COVERAGE_BUILD, "ros2_msgs_unittest")
    flash_executable = executable(GTEST_COVERAGE_BUILD, "flash_storage_unittest")
    common_coverage_args = [
        str(ros2_executable),
        f"-object={flash_executable}",
        f"-instr-profile={profile_data}",
    ]
    sources = [str(ROS2_MSGS_SOURCE), str(FLASH_STORAGE_SOURCE)]

    print("\n=== LLVM Coverage ===")
    code, _ = run(
        [tools["llvm-cov"], "report", *common_coverage_args, *sources]
    )
    if code != 0:
        return code

    html_dir = GTEST_COVERAGE_BUILD / "html"
    code, _ = run(
        [
            tools["llvm-cov"],
            "show",
            *common_coverage_args,
            "-format=html",
            f"-output-dir={html_dir}",
            "-show-line-counts-or-regions",
            "-show-branches=count",
            *sources,
        ]
    )
    if code == 0:
        print(f"HTML coverage report: {html_dir / 'index.html'}")
    return code


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

    coverage_code = run_coverage()

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
    print(f"{'coverage':<10}: {'PASSED' if coverage_code == 0 else 'FAILED'}")

    return int(pytest_code != 0 or gtest_code != 0 or coverage_code != 0)


if __name__ == "__main__":
    raise SystemExit(main())
