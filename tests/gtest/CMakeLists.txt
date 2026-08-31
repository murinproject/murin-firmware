cmake_minimum_required(VERSION 3.14)
project(ros2_msgs_unittest C CXX)

include(FetchContent)
set(FETCHCONTENT_BASE_DIR
    ${CMAKE_BINARY_DIR}/_deps
    CACHE PATH "Directory used by FetchContent to store downloaded content")
message(STATUS "FetchContent base dir: ${FETCHCONTENT_BASE_DIR}")
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# For Windows: prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Build the ros2_msgs C library (compiled as C)
add_library(
  ros2_msgs STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/ros2/ros2_msgs.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/link/framed_link.c
  ${CMAKE_CURRENT_SOURCE_DIR}/ros2_msgs_host_stubs.c)
target_include_directories(
  ros2_msgs
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
          ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/ros2
          ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/link
          ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/usb
          ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/diag
          ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/motor)
target_compile_definitions(ros2_msgs PRIVATE UNIT_TEST)
set_target_properties(ros2_msgs PROPERTIES LINKER_LANGUAGE C)

# Test executable (C++ using GoogleTest)
add_executable(ros2_msgs_unittest
               ${CMAKE_CURRENT_SOURCE_DIR}/test_ros2_msgs.cpp)
target_include_directories(
  ros2_msgs_unittest
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/ros2
          ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/link
          ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/usb
          ${CMAKE_CURRENT_SOURCE_DIR}/../../main/modules/motor)
target_compile_definitions(ros2_msgs_unittest PRIVATE UNIT_TEST)
target_compile_features(ros2_msgs_unittest PRIVATE cxx_std_11)
target_link_libraries(ros2_msgs_unittest PRIVATE ros2_msgs gtest_main)

add_test(NAME ros2_msgs_unittest COMMAND ros2_msgs_unittest)
