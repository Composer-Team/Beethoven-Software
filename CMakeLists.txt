cmake_minimum_required(VERSION 3.5.0)
project(beethoven)
set(version 1.0.6)
set(CMAKE_CXX_STANDARD 17)

option(PLATFORM "Platform to build for")

message("Build type: ${CMAKE_BUILD_TYPE}")

if (NOT "${PLATFORM}" STREQUAL "baremetal")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
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
endif ()

if ("${PLATFORM}" STREQUAL "discrete" OR "${PLATFORM}" STREQUAL "sim")
    add_definitions(-DDiscrete)
    set(BUILD_SHARED ON)
elseif ("${PLATFORM}" STREQUAL "kria")
    add_definitions(-DKria)
    set(BUILD_SHARED ON)
elseif ("${PLATFORM}" STREQUAL "baremetal")
    message(STATUS "Running in baremetal mode")
    add_definitions(-DBAREMETAL)
    set(BUILD_SHARED OFF)
    if ("${M55_SRC}" STREQUAL "")
        message(FATAL_ERROR "M55_SRC not set. Please set M55_SRC to the path of the cortex-m55 source code.")
    endif ()
    # set M55_INC to M55_SRC/yamin/logical/testbench/execution_tb/tests/
    set(M55_INC ${M55_SRC}/yamin/logical/testbench/execution_tb/tests/)
    # ===== added for cortex-m55 =====
    # Copy CMSIS and Device directories to build directory
    file(COPY ${M55_INC}/CMSIS DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    file(COPY ${M55_INC}/Device DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    # Specify the C compiler
#    set(CMAKE_C_COMPILER arm-none-eabi-gcc)
#    set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
#     IF YOU GET ERRORS ABOUT COLORED DIAGNOSTICS, USE THESE SCRIPTS
    set(CMAKE_C_COMPILER /home/chriskjellqvist/Code/Composer-Software/m55_bin/gcc_filter)
    set(CMAKE_CXX_COMPILER /home/chriskjellqvist/Code/Composer-Software/m55_bin/g++_filter)
    # Compiler flags
    set(CMAKE_C_FLAGS "-mthumb -march=armv8.1-m.main+mve -mtune=cortex-m55+nomve.fp -mcmse -mfloat-abi=hard -mfpu=fpv5-sp-d16 -fomit-frame-pointer -D__GCC")
    # Include directories
    include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}/CMSIS/Include
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/Device/ARM/exectb_mcu/Include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${M55_SRC}/yamin/logical/yamin/pmc_files
        ${M55_SRC}/pmc100/software/api
    )
    # Preprocessor definitions
    add_definitions(-D_FPU -D_EPU -D_MVE -D_MVE1 -D_EPU -D_DCACHE_PRESENT -D_ICACHE_PRESENT)
    # Add startup file separately
    set(STARTUP_FILE ${CMAKE_CURRENT_BINARY_DIR}/Device/ARM/exectb_mcu/Source/GCC/startup_exectb_mcu.S)
    # Set properties for the startup file
    set_property(SOURCE ${STARTUP_FILE} PROPERTY LANGUAGE C)
    set_property(SOURCE ${STARTUP_FILE} PROPERTY COMPILE_FLAGS "-x assembler-with-cpp")
    # Linker script
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${CMAKE_CURRENT_BINARY_DIR}/Device/ARM/exectb_mcu/Source/GCC/mem.ld --specs=nosys.specs")
    set(M55_SOURCES
        ${CMAKE_CURRENT_BINARY_DIR}/Device/ARM/exectb_mcu/Source/system_exectb_mcu.c
        ${CMAKE_CURRENT_SOURCE_DIR}/retarget_exectb_mcu.c
        ${CMAKE_CURRENT_SOURCE_DIR}/EXECTB_tests.c
    )
    # ===== end of added for cortex-m55 =====
else ()
    message(FATAL_ERROR "Unsupported device. Supported devices are discrete, sim, kria, and baremetal.")
endif ()

if (BUILD_SHARED)
    add_library(beethoven SHARED
            src/rocc_cmd.cc
            src/util.cc
            src/allocator/alloc.cc
            src/rocc_response.cc
            include/beethoven/response_handle.h
            include/beethoven/rocc_cmd.h
            include/beethoven/allocator/device_allocator.h)
endif ()

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
    add_library(beethoven STATIC
            src/rocc_cmd.cc
            src/util.cc
            src/rocc_response.cc
            src/allocator/alloc_baremetal.cpp
            src/fpga_handle_impl/fpga_handle_baremetal.cc
            $ENV{BEETHOVEN_ROOT}/Beethoven-Hardware/vsim/generated-src/generated_beethoven_src.cc)

    target_include_directories(
            beethoven
            PUBLIC
            $ENV{BEETHOVEN_ROOT}/Beethoven-Hardware/vsim/generated-src/
            include
    )
else ()
    message(FATAL_ERROR "Unsupported device. Supported devices are discrete, kria, and baremetal.")
endif ()

add_executable(merge_sort test/merge_sort.cc)
target_link_libraries(merge_sort beethoven)
add_executable(alloc_sizes test/alloc_sizes.cc)
target_link_libraries(alloc_sizes beethoven)


if (NOT ("${PLATFORM}" STREQUAL "baremetal"))
    if (APPLE)
        message(Debug "Building for Darwin/OSX platform")
    elseif (UNIX)
        message(Debug "Building for Linux platform")
        target_link_libraries(beethoven PUBLIC rt pthread)
    else ()
        message(FATAL_ERROR "Unsupported platform.")
    endif ()

    message("install interface is ${CMAKE_INSTALL_PREFIX}/include")

    target_include_directories(beethoven PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include/>
    )

    set(CONFIG_EXPORT_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/beethoven")
    set(EXPORT_TARGETS beethoven)
    configure_package_config_file(
            "${PROJECT_SOURCE_DIR}/cmake/Config.cmake.in"
            "${PROJECT_BINARY_DIR}/cmake/beethovenConfig.cmake"
            INSTALL_DESTINATION "${CONFIG_EXPORT_DIR}"
            PATH_VARS CMAKE_INSTALL_PREFIX CMAKE_INSTALL_LIBDIR CONFIG_EXPORT_DIR
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
            DESTINATION include/beethoven
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


