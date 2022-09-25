project(Composer-Software)
cmake_minimum_required(VERSION 3.10)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)

add_library(composer SHARED src/rocc.cc src/fpga_handle.cc src/fpga_transfer.cc src/composer_util.cc)

target_include_directories(composer PUBLIC include $ENV{AWS_INCLUDE})

install(TARGETS composer
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include
        COMPONENT library
        )
