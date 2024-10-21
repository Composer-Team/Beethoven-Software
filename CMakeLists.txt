project(beethoven)
cmake_minimum_required(VERSION 3.0)
set(version 1.0.6)
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

option(PLATFORM "Platform to build for")

message("Build type: ${CMAKE_BUILD_TYPE}")
set(BEETHOVEN_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/beethoven")

if ("${PLATFORM}" STREQUAL "discrete")
    add_definitions(-DDiscrete)
    set(BUILD_SHARED ON)
elseif ("${PLATFORM}" STREQUAL "kria")
    add_definitions(-DKria)
    set(BUILD_SHARED ON)
elseif ("${PLATFORM}" STREQUAL "baremetal")
    add_definitions(-DBAREMETAL)
    set(BUILD_SHARED OFF)
else ()
    message(FATAL_ERROR "Unsupported device. Supported devices are discrete, kria, and baremetal.")
endif ()

if (BUILD_SHARED)
    message("BUILDING SHARED LIBRARY")
    add_library(beethoven SHARED
            src/rocc_cmd.cc
            src/util.cc
            src/alloc.cc
            src/rocc_response.cc
            include/beethoven/response_handle.h
            include/beethoven/rocc_cmd.h
            include/beethoven/allocator/device_allocator.h)
else()
    message("BUILDING STATIC LIBRARY")
    add_library(beethoven STATIC
            src/rocc_cmd.cc
            src/util.cc
            src/alloc.cc
            src/rocc_response.cc
            include/beethoven/response_handle.h
            include/beethoven/rocc_cmd.h
            include/beethoven/allocator/device_allocator.h)
endif()

if ("${PLATFORM}" STREQUAL "discrete")
    target_sources(beethoven PRIVATE
            src/fpga_handle_impl/fpga_handle_discrete.cc
            src/response_handle.cc
            src/verilator_server.cc)
elseif ("${PLATFORM}" STREQUAL "kria")
    target_sources(beethoven PRIVATE
            src/fpga_handle_impl/fpga_handle_kria.cc
            src/response_handle.cc
            src/verilator_server.cc
    )
elseif ("${PLATFORM}" STREQUAL "baremetal")
    target_sources(beethoven PRIVATE
            src/fpga_handle_impl/fpga_handle_baremetal.cc
            $ENV{BEETHOVEN_ROOT}/Beethoven-Hardware/vsim/generated-src/generated_beethoven_src.cc
    )
    target_include_directories(
            beethoven
            PRIVATE
            $ENV{BEETHOVEN_ROOT}/Beethoven-Hardware/vsim/generated-src/
    )
else ()
    message(FATAL_ERROR "Unsupported device. Supported devices are discrete, kria, and baremetal.")
endif ()

add_executable(merge_sort src/tests/merge_sort.cc)
target_link_libraries(merge_sort beethoven)
add_executable(alloc_sizes src/tests/alloc_sizes.cc)
target_link_libraries(alloc_sizes beethoven)


if (NOT "${PLATFORM}" STREQUAL "baremetal")

    if (APPLE)
        message(Debug "Building for Darwin/OSX platform")
    elseif (UNIX)
        message(Debug "Building for Linux platform")
        target_link_libraries(beethoven PUBLIC rt pthread)
    else ()
        message(FATAL_ERROR "Unsupported platform.")
    endif ()

    message("install interface is ${BEETHOVEN_INSTALL_INCLUDEDIR}")
    target_include_directories(beethoven
            PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${BEETHOVEN_INSTALL_INCLUDEDIR}/>
    )

    set(CONFIG_EXPORT_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/beethoven")
    set(EXPORT_TARGETS beethoven)
    configure_package_config_file(
            "${PROJECT_SOURCE_DIR}/cmake/Config.cmake.in"
            "${PROJECT_BINARY_DIR}/cmake/beethovenConfig.cmake"
            INSTALL_DESTINATION "${CONFIG_EXPORT_DIR}"
            PATH_VARS BEETHOVEN_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR CONFIG_EXPORT_DIR
    )

    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/beethovenConfigVersion.cmake"
            VERSION "${version}"
            COMPATIBILITY AnyNewerVersion
    )

    install(TARGETS beethoven
            EXPORT beethovenTargets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
            DESTINATION "${BEETHOVEN_INSTALL_INCLUDEDIR}"
            FILES_MATCHING PATTERN "*.h")

    install(EXPORT beethovenTargets
            DESTINATION "${CONFIG_EXPORT_DIR}"
            NAMESPACE APEX::)


    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/beethovenConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/beethovenConfigVersion.cmake"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/beethoven
    )
endif ()


