cmake_minimum_required(VERSION 3.20)
project(beethoven LANGUAGES C CXX ASM VERSION 0.0.0)

option(BEETHOVEN_BAREMETAL "Build the Cortex-M55 baremetal variant" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(BEETHOVEN_BAREMETAL)
    add_subdirectory(platforms/baremetal)
else()
    add_subdirectory(platforms/host)
endif()
