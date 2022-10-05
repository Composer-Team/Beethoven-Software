project(composer )
cmake_minimum_required(VERSION 3.10)
set(version 1.0.0)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
message("${CMAKE_INSTALL_INCLUDEDIR}")
set(CMAKE_CXX_STANDARD 17)
if(NOT ${CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
            STRINGS "Release" "Debug" "MinSizeRel" "RelWithDebInfo")
endif()

set(COMPOSER_HEADER_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/include)
set(COMPOSER_CMAKE_DIR ${CMAKE_INSTALL_PREFIX}/lib/cmake/composer)
message("${CMAKE_INSTALL_PREFIX}/lib/cmake/composer")

add_library(composer SHARED src/rocc.cc src/fpga_handle.cc src/fpga_transfer.cc src/composer_util.cc)
set_target_properties(composer PROPERTIES PUBLIC_HEADER "include/composer_consts.h;include/composer_util.h;include/fpga_handle.h;include/fpga_transfer.h;include/rocc.h;include/composer_verilator_server.h")

# If we can find the Amazon AWS FPGA, enable it a
if(DEFINED $ENV{COMPOSER_AWS_SDK_DIR})
    set(AWS_INCLUDE $ENV{COMPOSER_AWS_SDK_DIR}/sdk/userspace/include)
    target_include_directories(composer PUBLIC
            $<BUILD_INTERFACE:include> # for headers when building
            $<BUILD_INTERFACE:${AWS_INCLUDE}>
            $<INSTALL_INTERFACE:${COMPOSER_HEADER_INSTALL_DIR}>
            $<INSTALL_INTERFACE:${AWS_INCLUDE}> # for client in install mode
            )
    target_include_directories(composer PUBLIC include ${AWS_INCLUDE})
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -DUSE_AWS )
    message("Amazon AWS FPGA SDK detected! This installation will support the F1")
else()
    target_include_directories(composer PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> # for headers when building
            $<INSTALL_INTERFACE:${COMPOSER_HEADER_INSTALL_DIR}>
            )
    message(WARNING "The $COMPOSER_AWS_SDK_DIR environment variable was not found. This installation will NOT support the F1")
endif()



install(TARGETS composer
        EXPORT composerTargets DESTINATION ${COMPOSER_CMAKE_DIR}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        INCLUDES DESTINATION ${COMPOSER_HEADER_INSTALL_DIR}
        PUBLIC_HEADER DESTINATION ${COMPOSER_HEADER_INSTALL_DIR}
        COMPONENT library
        )

include(CMakePackageConfigHelpers)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerConfig.cmake"
        INSTALL_DESTINATION ${COMPOSER_CMAKE_DIR}
        )

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerConfigVersion.cmake"
        VERSION "${version}"
        COMPATIBILITY AnyNewerVersion
)

export(EXPORT composerTargets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerTargets.cmake"
        NAMESPACE APEX::)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerTargets.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerConfigVersion.cmake"
        DESTINATION ${COMPOSER_CMAKE_DIR}
        )


