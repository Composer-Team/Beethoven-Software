project(Composer-Software)
cmake_minimum_required(VERSION 3.10)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 17)

add_library(composer SHARED src/rocc.cc src/fpga_handle.cc src/fpga_transfer.cc src/composer_util.cc)

target_include_directories(composer PUBLIC include $ENV{AWS_INCLUDE})
set_target_properties(composer PROPERTIES PUBLIC_HEADER "include/composer_consts.h;include/composer_util.h;include/fpga_handle.h;include/fpga_transfer.h;include/rocc.h")


install(TARGETS composer
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include
        COMPONENT library
        )
