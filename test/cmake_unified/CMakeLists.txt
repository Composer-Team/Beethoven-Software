cmake_minimum_required(VERSION 3.5.0)
project(beethoven_cmake_unified_test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# Find Beethoven package (must be installed first)
find_package(beethoven REQUIRED)

message(STATUS "==============================================")
message(STATUS "Unified CMake Build System Test")
message(STATUS "==============================================")

# Generate hardware from Scala configuration
# This sets BEETHOVEN_PATH to a local directory for this sbt invocation
beethoven_hardware(test_accel
    MAIN_CLASS basic.CMakeUnifiedTestBuild
)

# Build testbench with Verilator simulator
beethoven_testbench(test_unified
    SOURCES src/main/c/test_unified_build.cc
    HARDWARE test_accel
    SIMULATOR verilator
)

message(STATUS "==============================================")
message(STATUS "Build with: cmake --build .")
message(STATUS "Run with: ./test_unified")
message(STATUS "==============================================")
