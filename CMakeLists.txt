project(Composer-Software)
cmake_minimum_required(VERSION 3.18.2)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

SET(lib_src src/rocc.cc src/fpga_handle.cc src/fpga_transfer.cc src/composer_util.cc)
add_library(composer STATIC ${lib_src})

target_include_directories(composer PUBLIC include $ENV{AWS_INCLUDE})

install(TARGETS composer)
