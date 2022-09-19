cmake_minimum_required(VERSION 3.10)
message("Path is $ENV{AWS_INCLUDE}")
message("CMAKE_CXX_FLAGS_DEBUG is ${CMAKE_CXX_FLAGS_DEBUG}")
message("CMAKE_CXX_FLAGS_RELEASE is ${CMAKE_CXX_FLAGS_RELEASE}")

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")
SET(lib_src src/rocc.cc src/memrocc.cc src/fpga_handle.cc src/fpga_transfer.cc src/composer_util.cc)

add_library(composer-lib STATIC ${lib_src})

add_executable(template src/template.cc ${lib_src})

target_include_directories(composer-lib PRIVATE include $ENV{AWS_INCLUDE})