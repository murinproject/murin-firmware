# ROS2 message unit tests

This directory contains a standalone CMake project that builds the `ros2_msgs`
C source and runs its GoogleTest-based C++ test harness.

## Run the GoogleTest suite

From the repository root:

```powershell
cmake -S tests/gtest -B tests/gtest/build
cmake --build tests/gtest/build --config Release
ctest --test-dir tests/gtest/build --output-on-failure -C Release
```

The project uses CMake `FetchContent` to download GoogleTest into
`tests/gtest/build/_deps` on the first configure.

The suite also builds `flash_storage.c` against a host NVS mock backed by the
text file `flash_storage_gtest.txt`. The file is created in the test working
directory and cleaned up by the flash-storage test fixture.

Host-only stubs live under their subsystem directories in `mocks/`. Shared
ESP-IDF compatibility headers live in `mocks/include/`; the GTest project root
contains only test entry files and project metadata.

## Run coverage analysis

LLVM source-based coverage requires Clang, `llvm-profdata`, and `llvm-cov`.
From the repository root in PowerShell, configure a separate instrumented
Debug build:

```powershell
cmake -S tests/gtest -B tests/gtest/build-coverage -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  "-DCMAKE_C_FLAGS=-fprofile-instr-generate -fcoverage-mapping" `
  "-DCMAKE_CXX_FLAGS=-fprofile-instr-generate -fcoverage-mapping"

cmake --build tests/gtest/build-coverage
```

Run both test executables and merge their raw profiles:

```powershell
$coverageDir = (Resolve-Path tests/gtest/build-coverage).Path

Get-ChildItem -LiteralPath $coverageDir -Filter "*.profraw" -File |
  Remove-Item -Force

$env:LLVM_PROFILE_FILE = "$coverageDir/%p-%m.profraw"
ctest --test-dir $coverageDir --output-on-failure
$env:LLVM_PROFILE_FILE = $null

$profiles = Get-ChildItem -LiteralPath $coverageDir -Filter "*.profraw" -File |
  Select-Object -ExpandProperty FullName

llvm-profdata merge -sparse $profiles `
  -o "$coverageDir/coverage.profdata"
```

Print a summary for the two production modules:

```powershell
$ros2Source = (Resolve-Path main/modules/ros2/ros2_msgs.c).Path
$flashSource = (Resolve-Path main/modules/nvs/flash_storage.c).Path

llvm-cov report "$coverageDir/ros2_msgs_unittest.exe" `
  "-object=$coverageDir/flash_storage_unittest.exe" `
  "-instr-profile=$coverageDir/coverage.profdata" `
  $ros2Source $flashSource
```

Generate a line-by-line HTML report:

```powershell
llvm-cov show "$coverageDir/ros2_msgs_unittest.exe" `
  "-object=$coverageDir/flash_storage_unittest.exe" `
  "-instr-profile=$coverageDir/coverage.profdata" `
  -format=html `
  "-output-dir=$coverageDir/html" `
  -show-line-counts-or-regions `
  -show-branches=count `
  $ros2Source $flashSource
```

Open `tests/gtest/build-coverage/html/index.html` in a browser to inspect the
annotated source report.
