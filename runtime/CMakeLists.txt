cmake_minimum_required(VERSION 3.20)
project(beethoven_runtime LANGUAGES C CXX VERSION 0.0.0)

# === Required inputs (provided by the CLI) ===
foreach(_var BEETHOVEN_PROJECT_ROOT BEETHOVEN_BUILD_MODE BEETHOVEN_PLATFORM)
    if(NOT DEFINED ${_var})
        message(FATAL_ERROR "${_var} is required.")
    endif()
endforeach()

if(NOT DEFINED BEETHOVEN_SIMULATOR AND BEETHOVEN_BUILD_MODE STREQUAL "simulation")
    # Default to icarus: verilator is faster but currently breaks on
    # SimulationPlatform's wide buses (see docs/issues/verilator-widebus.md).
    # Switch back to verilator here once that's fixed.
    set(BEETHOVEN_SIMULATOR "icarus")
endif()

# === Frontend selection (inferred from platform) ===
if(BEETHOVEN_PLATFORM STREQUAL "baremetal")
    set(BEETHOVEN_FRONTEND chipkit)
else()
    set(BEETHOVEN_FRONTEND axi)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# === Version-pinned dep on libbeethoven ===
find_package(beethoven 0.0.0 REQUIRED COMPONENTS ${BEETHOVEN_PLATFORM})

# === DRAMsim3 (vendored) ===
add_subdirectory(DRAMsim3 EXCLUDE_FROM_ALL)

# === Project-derived paths ===
set(_proj    ${BEETHOVEN_PROJECT_ROOT})
set(_binding ${_proj}/target/binding)
set(_hw      ${_proj}/target/${BEETHOVEN_BUILD_MODE}/hw)
set(_out     ${_proj}/target/${BEETHOVEN_BUILD_MODE}/runtime)

if(NOT EXISTS ${_binding}/beethoven_hardware.cc)
    message(FATAL_ERROR
        "Cannot find generated binding at ${_binding}/beethoven_hardware.cc. "
        "Run sbt to generate the hardware first.")
endif()

# === Daemon core (always compiled) ===
# Note: fpga_main.cc supplies main() only for synthesis (real FPGA) builds.
# Verilator and VPI frontends each provide their own entry point.
set(_core
    src/core/cmd_server.cc
    src/core/data_server.cc
    src/core/mmio.cc
    src/core/tick.cc
    src/core/mem_ctrl.cc
    src/core/data_wrapper.cc
    src/core/singleton_lock.cc)

# === Frontend sources (axi or chipkit) ===
if(BEETHOVEN_FRONTEND STREQUAL "axi")
    list(APPEND _core src/frontends/axi/front_bus_ctrl_axi.cc)
elseif(BEETHOVEN_FRONTEND STREQUAL "chipkit")
    list(APPEND _core
        src/frontends/chipkit/front_bus_ctrl_chipkit.cc
        src/frontends/chipkit/util.cc
        src/frontends/chipkit/state_machine.cc)
endif()

# === Synth-mode FPGA glue ===
if(BEETHOVEN_BUILD_MODE STREQUAL "synthesis")
    list(APPEND _core src/fpga/fpga_utils.c src/fpga/response_poller.cc)
endif()

# === Simulator-specific paths ===
if(BEETHOVEN_BUILD_MODE STREQUAL "simulation" AND BEETHOVEN_SIMULATOR STREQUAL "verilator")
    # === Verilator path ===
    find_package(verilator REQUIRED VERSION 5.0.0)

    file(GLOB _verilog_srcs ${_hw}/*.v ${_hw}/*.sv)
    if(_verilog_srcs STREQUAL "")
        message(FATAL_ERROR "No verilog sources found under ${_hw}/. Did sbt run?")
    endif()

    if(BEETHOVEN_FRONTEND STREQUAL "axi")
        list(APPEND _core
            src/frontends/axi/verilator_axi_frontend.cc
            src/frontends/axi/verilator.cc)
    else()
        list(APPEND _core src/frontends/chipkit/verilator_chipkit_frontend.cc)
    endif()

    add_executable(BeethovenRuntime
        ${_core}
        ${_binding}/beethoven_hardware.cc)

    verilate(BeethovenRuntime
        SOURCES ${_verilog_srcs}
        TOP_MODULE BeethovenTop
        PREFIX VBeethovenTop
        TRACE_FST
        INCLUDE_DIRS ${_hw}
        VERILATOR_ARGS --timescale 1ps/1ps --x-assign fast
                       -Wno-context -Wno-lint -Wno-style
                       -Wno-symrsvdword -Wno-multidriven -Wno-combdly
                       -Wno-moddup -Wno-unoptflat -Wno-stmtdly)

    target_compile_definitions(BeethovenRuntime PRIVATE VERILATOR=1 SIM=1)

    if(NOT "${KILL_SIM_AFTER}" STREQUAL "")
        target_compile_definitions(BeethovenRuntime PRIVATE KILL_SIM=${KILL_SIM_AFTER})
    endif()

elseif(BEETHOVEN_BUILD_MODE STREQUAL "simulation" AND BEETHOVEN_SIMULATOR STREQUAL "icarus")
    # === Icarus Verilog path ===
    find_program(IVERILOG iverilog REQUIRED)
    find_program(VVP      vvp      REQUIRED)

    # iverilog's vpi_user.h lives at different paths on different
    # distros (e.g. /usr/include/vpi_user.h on some, /usr/include/
    # iverilog/vpi_user.h on Debian/Ubuntu). Search both — the first
    # variant is found by default, the second via the PATH_SUFFIXES
    # extension to standard search dirs (/usr/include, /usr/local/include).
    find_path(IVERILOG_VPI_INCLUDE_DIR vpi_user.h
        PATH_SUFFIXES iverilog
        REQUIRED)

    if(NOT BEETHOVEN_FRONTEND STREQUAL "axi")
        message(FATAL_ERROR "Icarus + chipkit is not currently supported.")
    endif()
    list(APPEND _core src/frontends/axi/vpi_axi_frontend.cc)

    add_library(sim_BeethovenRuntime MODULE
        ${_core}
        ${_binding}/beethoven_hardware.cc)
    set_target_properties(sim_BeethovenRuntime PROPERTIES
        SUFFIX ".vpi"
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY ${_out})
    target_compile_definitions(sim_BeethovenRuntime PRIVATE
        ICARUS=1 SIM=1
        DRAMSIM_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/DRAMsim3/configs/DDR4_8Gb_x16_3200.ini")
    target_include_directories(sim_BeethovenRuntime PRIVATE
        include ${_binding} ${_hw} ${IVERILOG_VPI_INCLUDE_DIR})
    target_link_libraries(sim_BeethovenRuntime PRIVATE
        APEX::beethoven-${BEETHOVEN_PLATFORM} dramsim3)

    file(GLOB _verilog_srcs ${_hw}/*.v ${_hw}/*.sv)
    if(NOT EXISTS ${_hw}/BeethovenTopVCSHarness.v)
        message(FATAL_ERROR
            "Icarus path needs ${_hw}/BeethovenTopVCSHarness.v as the top. "
            "Check sbt's simulation hw output.")
    endif()
    add_custom_command(
        OUTPUT ${_out}/beethoven.vvp
        COMMAND ${IVERILOG}
                -DCLOCK_PERIOD=4 -DICARUS -DSYNTHESIS -DENABLE_TRACE
                -g2005-sv -I${_hw}
                -s BeethovenTopVCSHarness
                -o ${_out}/beethoven.vvp
                ${_verilog_srcs}
        DEPENDS ${_verilog_srcs}
        VERBATIM)
    add_custom_target(BeethovenRuntime ALL
        DEPENDS ${_out}/beethoven.vvp sim_BeethovenRuntime)

elseif(BEETHOVEN_BUILD_MODE STREQUAL "simulation" AND BEETHOVEN_SIMULATOR STREQUAL "vcs")
    # === VCS path ===
    find_program(VCS vcs REQUIRED)

    if(BEETHOVEN_FRONTEND STREQUAL "axi")
        list(APPEND _core src/frontends/axi/vpi_axi_frontend.cc)
    else()
        list(APPEND _core src/frontends/chipkit/vcs_chipkit_frontend.cc)
    endif()

    add_library(BeethovenRuntime SHARED
        ${_core}
        ${_binding}/beethoven_hardware.cc)
    set_target_properties(BeethovenRuntime PROPERTIES
        OUTPUT_NAME "BeethovenRuntime"
        LIBRARY_OUTPUT_DIRECTORY ${_out})
    target_compile_definitions(BeethovenRuntime PRIVATE
        VCS=1 SIM=1 USE_VCS=1
        DRAMSIM_CONFIG="${CMAKE_CURRENT_SOURCE_DIR}/DRAMsim3/configs/DDR4_8Gb_x16_3200.ini")

    file(GLOB _verilog_srcs ${_hw}/*.v ${_hw}/*.sv)
    if(EXISTS ${_hw}/BeethovenTopVCSHarness.v)
        list(APPEND _verilog_srcs ${_hw}/BeethovenTopVCSHarness.v)
    endif()
    add_custom_command(
        OUTPUT ${_out}/BeethovenTop
        COMMAND ${VCS} +vcs+loopreport -timescale=1ns/100ps
                -P ${CMAKE_CURRENT_SOURCE_DIR}/scripts/tab.tab -sverilog
                +incdir+${_hw} -full64 +vpi+1
                +define+CLOCK_PERIOD=2
                $<TARGET_FILE:BeethovenRuntime>
                -lrt -lbeethoven-${BEETHOVEN_PLATFORM} -ldramsim3
                ${_verilog_srcs}
                -o ${_out}/BeethovenTop
        DEPENDS BeethovenRuntime ${_verilog_srcs}
        WORKING_DIRECTORY ${_out}
        VERBATIM)
    add_custom_target(BeethovenRuntimeBin ALL
        DEPENDS ${_out}/BeethovenTop)

elseif(BEETHOVEN_BUILD_MODE STREQUAL "synthesis")
    # === Synthesis (FPGA) path — daemon talks to real silicon ===
    add_executable(BeethovenRuntime
        ${_core}
        src/core/fpga_main.cc
        ${_binding}/beethoven_hardware.cc)
    target_compile_definitions(BeethovenRuntime PRIVATE FPGA=1)

    string(TOUPPER ${BEETHOVEN_PLATFORM} _PLAT_UPPER)
    target_compile_definitions(BeethovenRuntime PRIVATE ${_PLAT_UPPER})

    if(BEETHOVEN_PLATFORM STREQUAL "discrete")
        if(NOT EXISTS "$ENV{AWS_FPGA_REPO_DIR}")
            message(FATAL_ERROR
                "BEETHOVEN_PLATFORM=discrete + synthesis requires AWS_FPGA_REPO_DIR. "
                "Source the aws-fpga sdk_setup.sh first.")
        endif()
        target_include_directories(BeethovenRuntime PRIVATE
            $ENV{AWS_FPGA_REPO_DIR}/sdk/userspace/include)
        target_link_libraries(BeethovenRuntime PRIVATE pthread fpga_mgmt)
    endif()

else()
    message(FATAL_ERROR
        "Unsupported BEETHOVEN_BUILD_MODE=${BEETHOVEN_BUILD_MODE} / "
        "BEETHOVEN_SIMULATOR=${BEETHOVEN_SIMULATOR} combination.")
endif()

# === Common settings for the executable variants (verilator + synthesis) ===
if(TARGET BeethovenRuntime AND NOT BEETHOVEN_SIMULATOR STREQUAL "icarus")
    target_include_directories(BeethovenRuntime PRIVATE
        include ${_binding} ${_hw})
    target_link_libraries(BeethovenRuntime PRIVATE
        APEX::beethoven-${BEETHOVEN_PLATFORM}
        dramsim3 pthread)
    if(UNIX AND NOT APPLE)
        target_link_libraries(BeethovenRuntime PRIVATE rt)
    endif()
    set_target_properties(BeethovenRuntime PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${_out}
        LIBRARY_OUTPUT_DIRECTORY ${_out})
    if(${VERBOSE})
        target_compile_definitions(BeethovenRuntime PRIVATE VERBOSE)
    endif()
endif()

