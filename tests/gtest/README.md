ROS2 msgs unit tests

This folder contains a CMake project that builds the `ros2_msgs` C source and a GoogleTest-based C++ test harness.

Quick run (Unix/WSL/macOS):

```bash
cmake -S tests -B tests/build
cmake --build tests/build --config Release -- -j
ctest --test-dir tests/build --output-on-failure
```

Notes
- The CMake project uses `FetchContent` to download GoogleTest into the build directory. The downloads are stored in `tests/build/_deps`.
- The GitHub Actions workflow caches `tests/build/_deps` between runs to speed up subsequent CI builds.
- If you run on Windows with the default generator, install a make/ninja generator or use the `-G Ninja` flag:

```bash
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build --config Release
ctest --test-dir tests/build --output-on-failure
```

CI
- The workflow `.github/workflows/unittest.yml` uses the cache action to preserve the `tests/build/_deps` directory. If you need more determinism, pin the Googletest URL to a specific tag in `tests/CMakeLists.txt`.
