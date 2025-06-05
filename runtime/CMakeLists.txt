cmake_minimum_required(VERSION 3.5.0)
project(BeethovenRuntime)

set(CMAKE_CXX_STANDARD 17)

find_package(beethoven REQUIRED)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


set(SRC src/data_server.cc src/cmd_server.cc src/mmio.cc)
if ("$ENV{BEETHOVEN_PATH}" STREQUAL "")
    message(FATAL_ERROR "Environment variable $BEETHOVEN_ROOT is not defined")
endif ()

set(BEETHOVEN_DIR "$ENV{BEETHOVEN_PATH}/build/")

if (NOT EXISTS ${BEETHOVEN_DIR}/beethoven_hardware.h)
    message(FATAL_ERROR "${BEETHOVEN_DIR}/beethoven_hardware.h does not exist. Try building your project or moving this file into the directory.")
endif ()

include_directories(${CMAKE_SOURCE_DIR}/include)

if ("${SIMULATOR}" STREQUAL "")
    set(SIMULATOR "verilator")
endif ()

set(AWS ("${BACKEND}" STREQUAL "F1" OR "${BACKEND}" STREQUAL "F2"))

if ("${TARGET}" STREQUAL "sim")
    message("BUILDING FOR SIM")
    set(SRC ${SRC} src/sim/tick.cc)
    if ("${FRONTEND}" STREQUAL "axi" OR "${FRONTEND}" STREQUAL "")
        message("BUILDING FOR AXI FRONTEND")
        set(SRC ${SRC} src/sim/axi/front_bus_ctrl_axi.cc)
        if ("${SIMULATOR}" STREQUAL "verilator")
            message("BUILDING FOR VERILATOR")
            set(SRC ${SRC} src/sim/axi/verilator_axi_frontend.cc)
            find_package(verilator REQUIRED VERSION 5.0.0)
            add_compile_definitions(USE_VERILATOR=1)
            #            add_link_options(-latomic)
        elseif ("${SIMULATOR}" STREQUAL "vcs")
            message("BUILDING FOR VCS")
	    option(VCS_INCLUDE_DIR "")
            if (NOT "${VCS_INCLUDE_DIR}" STREQUAL "")
            endif()
            set(SRC ${SRC} src/sim/axi/vpi_axi_frontend.cc)
            add_compile_definitions(USE_VCS=1)
        endif ()
    endif ()
    if ("${FRONTEND}" STREQUAL "chipkit")
        set(SRC ${SRC} src/sim/chipkit/util.cc)
        if ("${SIMULATOR}" STREQUAL "verilator")
            set(SRC ${SRC}
                    src/sim/chipkit/verilator_chipkit_frontend.cc
                    src/sim/chipkit/front_bus_ctrl_chipkit.cc)
        elseif ("${SIMULATOR}" STREQUAL "vcs")
            set(SRC ${SRC} src/sim/chipkit/vcs_chipkit_frontend.cc)
        endif ()
    endif ()

    set(SRC ${SRC} src/sim/mem_ctrl.cc)
    if (NOT DEFINED FRONTEND)
        set(FRONTEND "axi")
    endif ()


    if ("${USE_WAVES}" STREQUAL "")
        set(vargs "${vargs} --trace")
    endif ()

    if ("${SIMULATOR}" STREQUAL "verilator")
        add_subdirectory(DRAMsim3)
        find_package(verilator REQUIRED VERSION 5.0.0)
        add_executable(BeethovenRuntime ${SRC})
        message("Building with srcs: ${SRC}")

        target_link_libraries(BeethovenRuntime PRIVATE dramsim3 args)
        add_compile_definitions(VERILATOR=1)
    elseif ("${SIMULATOR}" STREQUAL "vcs" OR "${SIMULATOR}" STREQUAL "icarus")
        if (NOT "${DRAMSIM_CONFIG}" STREQUAL "")
            message("SET TO ${DRAMSIM_CONFIG}")
            add_compile_definitions(DRAMSIM_CONFIG="${DRAMSIM_CONFIG}")
            add_compile_definitions(USE_VCS=1)
        endif ()
        # NOTE: YOU ARE RESPONSIBLE FOR BUILDING AND LINKING DRAMSIM3
        add_subdirectory(DRAMsim3)
        install(TARGETS dramsim3
                DESTINATION $ENV{BEETHOVEN_PATH}/lib/)
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/tab.tab
                DESTINATION $ENV{BEETHOVEN_PATH}/inc/)
        add_library(BeethovenRuntime SHARED ${SRC})

        add_compile_definitions(VCS=1)
        target_include_directories(BeethovenRuntime PUBLIC $ENV{VCS_HOME}/include)
        target_link_libraries(BeethovenRuntime PRIVATE dramsim3 args)
    else ()
        message(FATAL_ERROR "Unsupported simulator")
    endif ()

    if (NOT "${KILL_SIM_AFTER}" STREQUAL "")
        target_compile_definitions(BeethovenRuntime PRIVATE KILL_SIM=${KILL_SIM_AFTER})
    endif ()

    target_compile_definitions(BeethovenRuntime PUBLIC SIM)
    set(BUILD_SIM 1)
elseif ("${TARGET}" STREQUAL "fpga")
    if ("${BACKEND}" STREQUAL "")
        message(FATAL_ERROR "Must define backend for FPGA. F1 or Kria")
    endif ()
    set(BUILD_SIM 0)
    add_executable(BeethovenRuntime ${SRC} src/response_poller.cc src/fpga_utils.c src/fpga_main.cc src/mmio.cc include/mmio.h)
    if (${AWS})
    	message("BUILDING FOR AWS")
	message("will include '$ENV{AWS_FPGA_REPO_DIR}/sdk/userspace/include'")
	if (NOT EXISTS $ENV{AWS_FPGA_REPO_DIR})
		message(FATAL_ERROR "Could not find SDK. Make sure you source the sdk_setup.sh in the aws-fpga repo")
	endif()
	include_directories($ENV{AWS_FPGA_REPO_DIR}/sdk/userspace/include)
        target_link_libraries(BeethovenRuntime PUBLIC pthread fpga_mgmt)
        add_executable(memcpy_test ${SRC} src/response_poller.cc src/fpga_utils.c src/mmio.cc include/mmio.h tests/memcpy.cc)
    endif ()
    target_compile_definitions(BeethovenRuntime PUBLIC FPGA=1 ${BACKEND})
    set(BUILD_FPGA 1)
else ()
    message(FATAL_ERROR "Must define build target: 'sim', or 'fpga'. Got '${TARGET}'. -DTARGET=<opt>")
endif ()

# Tie in beethoven
set_target_properties(BeethovenRuntime PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)
target_link_libraries(BeethovenRuntime PUBLIC APEX::beethoven)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fpermissive -Wno-attributes")


set(BEETHOVEN_HWDIR $ENV{BEETHOVEN_PATH}/build/)

if (APPLE)
    message("Building for Apple")
    set(verilator_args "-undefined dynamic_lookup ")
elseif (UNIX)
    message("Building for Linux")
    target_link_libraries(BeethovenRuntime PRIVATE rt)
else ()
    message(FATAL_ERROR "Unsupported platform")
endif ()

include_directories(${BEETHOVEN_HWDIR}/)

if ("${VERBOSE}" STREQUAL "1")
    target_compile_definitions(BeethovenRuntime PUBLIC VERBOSE)
endif ()

if ("${VTOP}" STREQUAL "")
    set(VTOP ${BEETHOVEN_HWIR}/hw/BeethovenTop.v)
endif ()

if ("${TOP}" STREQUAL "")
    set(TOP BeethovenTop)
endif ()

if ("${CONTROL_LITE}" STREQUAL "1")
    target_compile_definitions(BeethovenRuntime PUBLIC CONTROL_LITE)
endif ()

if ("${FRONTEND}" STREQUAL "chipkit")
    target_compile_definitions(BeethovenRuntime PUBLIC BAREMETAL_RUNTIME=1)
    message("Building for chipkit")
endif ()

if (${BUILD_SIM})
    include(${BEETHOVEN_DIR}/cmake_srcs.cmake)
    if ("${SIMULATOR}" STREQUAL "verilator")
        verilate(BeethovenRuntime
                SOURCES ${SRCS}
                INCLUDE_DIRS ${BEETHOVEN_DIR} $ENV{BEETHOVEN_PATH}/build/ ${BEETHOVEN_DIR}/beethoven.build/ ${ADDITIONAL_SEARCH}
                TOP_MODULE ${TOP}
                PREFIX ${TOP}
                TRACE_FST
                VERILATOR_ARGS --timescale 1ps/1ps --x-assign fast
                -Wno-context -Wno-lint -Wno-style -Wno-symrsvdword -Wno-multidriven -Wno-combdly
                -Wno-moddup -Wno-unoptflat -Wno-stmtdly
        )
      endif()
endif ()

add_executable(brt-kill src/kill.cc)
target_link_libraries(brt-kill PUBLIC APEX::beethoven)
