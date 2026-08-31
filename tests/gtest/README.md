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
