cmake_minimum_required(VERSION 3.10)
message("Path is $ENV{AWS_INCLUDE}")
message("CMAKE_CXX_FLAGS_DEBUG is ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELEASE is ${CMAKE_CXX_FLAGS_RELEASE}")

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")

add_library(composer-lib STATIC src/rocc.cc src/memrocc.cc src/fpga_handle.cc src/fpga_transfer.cc)

add_executable(template src/template.cc src/rocc.cc src/memrocc.cc src/fpga_handle.cc src/fpga_transfer.cc)

target_include_directories(composer-lib PRIVATE include $ENV{AWS_INCLUDE})