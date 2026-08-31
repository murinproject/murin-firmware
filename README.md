# Murin Firmware

Firmware and host-side tools for the Murin ESP32-S3 control system.

## Table of Contents

- [1. Prerequisites](#1-prerequisites)
- [2. Build and Run](#2-build-and-run)
- [3. Tools](#3-tools)
- [4. Testing](#4-testing)
  - [4.1 Run all tests](#41-run-all-tests)
  - [4.2 Test Suite Details](#42-test-suite-details)
- [5. Code Quality](#5-code-quality)
  - [5.1 Format code](#51-format-code)
  - [5.2 Static analysis](#52-static-analysis)
- [6. Troubleshooting](#6-troubleshooting)

## 1. Prerequisites

Install the Python and Node.js dependencies from the repository root:

```powershell
python -m pip install -r requirements.txt
npm install
```

## 2. Build and Flash

Activate the ESP-IDF environment in PowerShell before using `idf.py`:

```powershell
C:\esp\v6.0\esp-idf\export.ps1
```

If ESP-IDF is installed in a different location, update the path accordingly.

Build the firmware with:

```powershell
idf.py build
```

Flash the firmware and open the monitor with:

```powershell
idf.py flash monitor -p COM10
```

Connect the board over USB before running hardware-backed tests or monitor
tools.

## 3. Tools

The parser tools, serial configuration, and command examples are documented in
[`tools/README.md`](tools/README.md).

## 4. Testing

### 4.1 Run all tests

Run pytest and the GoogleTest suite from the repository root:

```powershell
.\scripts\test-all.ps1
```

Pass additional arguments to pytest:

```powershell
.\scripts\test-all.ps1 -v
```

### 4.2 Test Suite Details

Detailed pytest instructions, configuration, and troubleshooting are
documented in [`tests/pytest/README.md`](tests/pytest/README.md).

The GoogleTest build and direct commands are documented in
[`tests/gtest/README.md`](tests/gtest/README.md).

## 5. Code Quality

### 5.1 Format code

Run all configured formatters:

```powershell
.\scripts\format-all.ps1
```

The script formats:

- C/C++ files under `main` with `clang-format`
- Python files under `tests`, `tools`, and `utils` with Ruff
- CMake files with `cmake-format`

Install the Python formatters with:

```powershell
python -m pip install ruff cmakelang
```

The LLVM installation provides `clang-format`.

### 5.2 Static analysis

The repository includes [`.clang-tidy`](.clang-tidy). After generating the
compile database, run clang-tidy with:

```powershell
idf.py reconfigure
python "C:\Program Files\LLVM\bin\run-clang-tidy" -p build
```
