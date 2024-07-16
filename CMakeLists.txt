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

if ("${KRIA}" STREQUAL "1")
    add_definitions(-DKria)
endif ()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
            STRINGS "Release" "Debug" "MinSizeRel" "RelWithDebInfo")
endif ()

message("Build type: ${CMAKE_BUILD_TYPE}")
set(BEETHOVEN_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/beethoven")

if (DEFINED BAREMETAL)
    add_compile_definitions(BAREMETAL)
    include_directories($ENV{BEETHOVEN_ROOT}/Beethoven-Hardware/vsim/generated-src/)
    add_library(beethoven STATIC
            src/baremetal.cc
            src/rocc_cmd.cc
            src/alloc.cc
            src/util.cc
            $ENV{BEETHOVEN_ROOT}/Beethoven-Hardware/vsim/generated-src/generated_beethoven_src.cc
            include/beethoven/response_handle.h
            include/beethoven/rocc_cmd.h)
else ()
    add_library(beethoven SHARED
            src/rocc_cmd.cc
            src/util.cc
            src/fpga_handle.cc
            src/alloc.cc
            src/response_handle.cc
            src/rocc_response.cc
            src/verilator_server.cc
            include/beethoven/response_handle.h
            include/beethoven/rocc_cmd.h)
endif ()

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
        $ENV{BEETHOVEN_ROOT}/Beethoven-Hardware/vsim/generated-src/
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

add_executable(merge_sort src/tests/merge_sort.cc)
target_link_libraries(merge_sort beethoven)
add_executable(alloc_sizes src/tests/alloc_sizes.cc)
target_link_libraries(alloc_sizes beethoven)

