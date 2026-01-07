cmake_minimum_required(VERSION 3.28.0)
project(beethoven)
enable_language(CXX)
set(VERSION 1.0.0)

set(DISCRETE OFF)
set(ZYNQ OFF)
set(BAREMETAL OFF)
if ("${PLATFORM}" STREQUAL "baremetal")
    set(BAREMETAL ON)
elseif("${PLATFORM}" STREQUAL "discrete")
    set(DISCRETE ON)
elseif("${PLATFORM}" STREQUAL "zynq" OR "${PLATFORM}" STREQUAL "kria")
    set(ZYNQ ON)
    if ("${PLATFORM}" STREQUAL "kria")
        message(WARNING "Platform 'kria' is deprecated. Use 'zynq' instead.")
    endif()
else()
    message(FATAL_ERROR "platform should be baremetal, discrete, or zynq")
endif()

# Optional: Enable software cache flushing for Zynq platforms
option(ENABLE_CACHE_FLUSH "Enable software cache flushing (ARM64 only, requires ZYNQ platform)" OFF)

# Validate ENABLE_CACHE_FLUSH usage
if (ENABLE_CACHE_FLUSH AND NOT ZYNQ)
    message(FATAL_ERROR "ENABLE_CACHE_FLUSH requires PLATFORM=zynq. Cache flushing is only supported on Zynq platforms.")
endif()


if (${BAREMETAL})
    if("${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
        message(FATAL "Must provide toolchain file for baremetal (e.g., cmake/arm-none-eabi.cmake)")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
option(PLATFORM "Platform to build for")

# Optional: Path to Beethoven-Hardware for unified CMake build system
# If set, this path will be saved during installation for beethoven_hardware() to use
set(BEETHOVEN_HARDWARE_PATH "" CACHE PATH
    "Path to Beethoven-Hardware sbt project (optional, for unified CMake builds)")

message("Build type: ${CMAKE_BUILD_TYPE}")
set(LINKAGE_TYPE SHARED)
set(LIBRARY_NAME beethoven)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
            STRINGS "Release" "Debug" "MinSizeRel" "RelWithDebInfo")
endif ()
if (${BAREMETAL})
    set(LINKAGE_TYPE STATIC)
    SET(LIBRARY_NAME beethoven_baremetal)
    set(CMAKE_INSTALL_LIBDIR "/usr/local/lib")
endif()
include(GNUInstallDirs)

add_library(${LIBRARY_NAME} ${LINKAGE_TYPE}
        src/rocc_cmd.cc
        src/util.cc
        src/rocc_response.cc
        include/beethoven/response_handle.h
        include/beethoven/rocc_cmd.h
        include/beethoven/allocator/device_allocator.h)

if (${DISCRETE})
    target_sources(${LIBRARY_NAME} PRIVATE
            src/fpga_handle_impl/fpga_handle_discrete.cc
            src/response_handle.cc
            src/allocator/alloc.cc
            src/verilator_server.cc)
    add_definitions(-DDiscrete)
    set(BUILD_SHARED ON)
elseif (${ZYNQ})
    add_definitions(-DZYNQ)
    set(BUILD_SHARED ON)
    if (ENABLE_CACHE_FLUSH)
        add_definitions(-DENABLE_CACHE_FLUSH)
        message(STATUS "Software cache flushing enabled for Zynq platform")
    endif()
    target_sources(${LIBRARY_NAME} PRIVATE
            src/fpga_handle_impl/fpga_handle_zynq.cc
            src/response_handle.cc
            src/allocator/alloc.cc
            src/verilator_server.cc
    )
elseif (${BAREMETAL})
    message(STATUS "Running in baremetal mode")
    set(LINKAGE_TYPE STATIC)
    set(LIBRARY_NAME beethoven_baremetal)
    add_definitions(-DBAREMETAL)
    set(BUILD_SHARED OFF)
    if ("$ENV{M55_SRC}" STREQUAL "")
        message(FATAL_ERROR "M55_SRC not set. Please set M55_SRC to the path of the cortex-m55 source code.")
    else()
        set(M55_SRC $ENV{M55_SRC})
    endif()
    include_directories(
        ${M55_SRC}/AT633-BU-50000-r1p0-00eac0/yamin/logical/testbench/execution_tb/tests
        ${M55_SRC}/AT633-BU-50000-r1p0-00eac0/yamin/logical/testbench/execution_tb/tests/Device/ARM/exectb_mcu
        ${M55_SRC}/AT633-BU-50000-r1p0-00eac0/yamin/logical/yamin/pmc_files
        ${M55_SRC}/AT637-BU-50000-r0p0-00eac0/pmc100/software/api
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
    )
    # Preprocessor definitions
    add_definitions(-D_FPU -D_EPU -D_MVE -D_MVE1 -D_EPU -D_DCACHE_PRESENT -D_ICACHE_PRESENT)
    # Linker script
    set(TB_LOC ${M55_SRC}/AT633-BU-50000-r1p0-00eac0/yamin/logical/testbench/execution_tb/tests/)
    set(STARTUP_FILE ${TB_LOC}/Device/ARM/exectb_mcu/Source/GCC/startup_exectb_mcu.S)
    set_property(SOURCE ${STARTUP_FILE} PROPERTY LANGUAGE C)
    set_property(SOURCE ${STARTUP_FILE} PROPERTY COMPILE_FLAGS "-x assembler-with-cpp")
    set(M55_SOURCES
        ${TB_LOC}/Device/ARM/exectb_mcu/Source//system_exectb_mcu.c
        ${TB_LOC}/retarget_exectb_mcu.c
        ${TB_LOC}/EXECTB_tests.c
        ${TB_LOC}/Device/ARM/exectb_mcu/Source/system_exectb_mcu.c
        ${M55_SRC}/TM840-BU-50000-r2p0-00rel0/daplite2/logical/testbench/soc_tb/system/gcc_retarget.c
        ${STARTUP_FILE}
    )
    # ===== end of added for cortex-m55 =====
    add_compile_definitions(BEETHOVEN_BAREMETAL=1)
    target_sources(${LIBRARY_NAME} PRIVATE
            ${M55_SOURCES}
            src/fpga_handle_impl/fpga_handle_baremetal.cc
    )
    include_directories(
            $ENV{BEETHOVEN_PATH}/build/
            ${TB_LOC}/Device/ARM/exectb_mcu/Include/
            ${TB_LOC}/CMSIS/Include/
            "include"
            ${CMAKE_}
    )
else ()
    message(FATAL_ERROR "Unsupported device. Supported devices are discrete (aka 'sim'), kria, and baremetal.")
endif ()

if (NOT ${BAREMETAL})
    add_executable(merge_sort test/merge_sort.cc)
    target_link_libraries(merge_sort ${LIBRARY_NAME})
    add_executable(alloc_sizes test/alloc_sizes.cc)
    target_link_libraries(alloc_sizes ${LIBRARY_NAME})
endif()


if (APPLE)
    message(Debug "Building for Darwin/OSX platform")
elseif (UNIX)
    message(Debug "Building for Linux platform")
    target_link_libraries(${LIBRARY_NAME} PUBLIC rt pthread)
elseif ("${PLATFORM}" STREQUAL "baremetal")
    message(Debug "Build baremetal")
else()
    message(FATAL_ERROR "Unsupported platform.")
endif ()

message("install interface is ${CMAKE_INSTALL_PREFIX}/include")

target_include_directories(${LIBRARY_NAME} PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/>
)

add_subdirectory(runtime/DRAMsim3)

set(EXPORT_TARGETS ${LIBRARY_NAME})
if (${BAREMETAL})
    set(CONFIG_FILE "${PROJECT_SOURCE_DIR}/cmake/Config_baremetal.cmake.in")
    set(CONFIG_EXPORT_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/beethoven_baremetal")
    set(CONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}/cmake/beethoven_baremetal")
else ()
    set(CONFIG_FILE "${PROJECT_SOURCE_DIR}/cmake/Config.cmake.in")
    set(CONFIG_EXPORT_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/beethoven")
    set(CONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}/cmake/beethoven")
endif()

configure_package_config_file(
        ${CONFIG_FILE}
        "${PROJECT_BINARY_DIR}/cmake/${LIBRARY_NAME}Config.cmake"
        INSTALL_DESTINATION "${CONFIG_EXPORT_DIR}"
        PATH_VARS CMAKE_INSTALL_PREFIX CMAKE_INSTALL_LIBDIR CONFIG_EXPORT_DIR
)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/${LIBRARY_NAME}ConfigVersion.cmake"
        VERSION "${VERSION}"
        COMPATIBILITY AnyNewerVersion
)

install(TARGETS ${LIBRARY_NAME}
        EXPORT ${LIBRARY_NAME}Targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/beethoven/
        DESTINATION "include/${LIBRARY_NAME}"
        FILES_MATCHING PATTERN "*.h")

install(EXPORT ${LIBRARY_NAME}Targets
        DESTINATION "${CONFIG_EXPORT_DIR}"
        NAMESPACE APEX::)


install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/${LIBRARY_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/${LIBRARY_NAME}ConfigVersion.cmake"
        DESTINATION ${CONFIG_LIBDIR}
)

# Install unified build system modules
if (NOT ${BAREMETAL})
    # Install runtime sources for unified CMake build system
    # These are needed by beethoven_testbench() to build simulators
    set(BEETHOVEN_SHARE_DIR "share/beethoven")

    # Runtime C++ sources
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/runtime/src/"
            DESTINATION "${BEETHOVEN_SHARE_DIR}/runtime/src"
            FILES_MATCHING
            PATTERN "*.cc"
            PATTERN "*.c"
            PATTERN "*.h"
    )

    # Runtime headers
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/runtime/include/"
            DESTINATION "${BEETHOVEN_SHARE_DIR}/runtime/include"
            FILES_MATCHING PATTERN "*.h"
    )

    # Runtime scripts (e.g., tab.tab for VCS)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/runtime/scripts/"
            DESTINATION "${BEETHOVEN_SHARE_DIR}/runtime/scripts"
            FILES_MATCHING
            PATTERN "*.tab"
            PATTERN "*.py"
    )

    install(DIRECTORY "${PROJECT_SOURCE_DIR}/runtime/DRAMsim3/configs/"
            DESTINATION "${BEETHOVEN_SHARE_DIR}/runtime/configs"
            FILES_MATCHING
            PATTERN "*.ini"
          )


    install(FILES
            "Makefile"
            DESTINATION "${BEETHOVEN_SHARE_DIR}/runtime/")

    # Generate and install paths config file if BEETHOVEN_HARDWARE_PATH is set
    if(NOT "${BEETHOVEN_HARDWARE_PATH}" STREQUAL "")
        # Verify the path exists
        if(EXISTS "${BEETHOVEN_HARDWARE_PATH}/build.sbt")
            message(STATUS "Configuring Beethoven-Hardware path: ${BEETHOVEN_HARDWARE_PATH}")
            file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/beethoven_paths.cmake"
                "# Auto-generated by beethoven installation\n"
                "# This file stores the path to Beethoven-Hardware for beethoven_hardware()\n"
                "set(BEETHOVEN_HARDWARE_INSTALL_PATH \"${BEETHOVEN_HARDWARE_PATH}\")\n"
            )
            install(FILES "${CMAKE_CURRENT_BINARY_DIR}/beethoven_paths.cmake"
                    DESTINATION "${BEETHOVEN_SHARE_DIR}"
            )
        else()
            message(WARNING
                "BEETHOVEN_HARDWARE_PATH is set but does not appear to be a valid "
                "Beethoven-Hardware directory (no build.sbt found): ${BEETHOVEN_HARDWARE_PATH}")
        endif()
    endif()
endif()

if (${BAREMETAL})
install(FILES
        cmake/arm-none-eabi.cmake
        DESTINATION share/beethoven_baremetal)
endif()

