project(composer)
cmake_minimum_required(VERSION 3.10)
set(version 1.0.5)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)
include(GNUInstallDirs)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
            STRINGS "Release" "Debug" "MinSizeRel" "RelWithDebInfo")
endif ()
message("Build type: ${CMAKE_BUILD_TYPE}")
set(COMPOSER_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/composer")

add_library(composer SHARED
        src/rocc_cmd.cc
        src/util.cc
        src/alloc.cc
        src/fpga_handle.cc
        src/response_handle.cc
        src/rocc_response.cc
        src/verilator_server.cc
        include/composer/response_handle.h
        include/composer/rocc_cmd.h
        include/composer/rocc_response.h include/composer/allocator_ptr.h)

if (APPLE)
    message(Debug "Building for Darwin/OSX platform")
elseif (UNIX)
    message(Debug "Building for Linux platform")
    target_link_options(composer PUBLIC -lrt)
else ()
    message(FATAL_ERROR "Unsupported platform.")
endif ()

message("install interface is ${COMPOSER_INSTALL_INCLUDEDIR}")
target_include_directories(composer
        PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${COMPOSER_INSTALL_INCLUDEDIR}/>
        )
#
#if (EXISTS "$ENV{COMPOSER_ROOT}/aws-fpga" AND NOT "${FORCE_NO_AWS}" STREQUAL "1")
## If we can find the Amazon AWS FPGA, enable it a
#    include_directories($ENV{COMPOSER_ROOT}/aws-fpga/sdk/userspace/include)
#    message("Amazon AWS FPGA SDK detected! This installation will support the F1")
#else ()
#    message(WARNING "The $COMPOSER_AWS_SDK_DIR environment variable was not found. This installation will NOT support the F1")
#endif ()

set(CONFIG_EXPORT_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/composer")
set(EXPORT_TARGETS composer)
configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/Config.cmake.in"
        "${PROJECT_BINARY_DIR}/cmake/composerConfig.cmake"
        INSTALL_DESTINATION "${CONFIG_EXPORT_DIR}"
        PATH_VARS COMPOSER_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR CONFIG_EXPORT_DIR
)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerConfigVersion.cmake"
        VERSION "${version}"
        COMPATIBILITY AnyNewerVersion
)

install(TARGETS composer
        EXPORT composerTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION "${COMPOSER_INSTALL_INCLUDEDIR}"
        FILES_MATCHING PATTERN "*.h")

install(EXPORT composerTargets
        DESTINATION "${CONFIG_EXPORT_DIR}"
        NAMESPACE APEX::)


install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/composerConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/composer
        )
