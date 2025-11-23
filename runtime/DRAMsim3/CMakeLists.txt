cmake_minimum_required(VERSION 3.0.0)
set(VERSION 0.0.0)
project(dramsim3)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

add_library(inih INTERFACE)
target_include_directories(inih INTERFACE ext/headers)

add_library(format INTERFACE)
target_include_directories(format INTERFACE ext/fmt/include)
target_compile_definitions(format INTERFACE FMT_HEADER_ONLY=1)

# argparsing library, only used in main program not the library
add_library(args INTERFACE)
target_include_directories(args INTERFACE ext/headers)

add_library(json INTERFACE)
target_include_directories(json INTERFACE ext/headers)

# Main DRAMSim Lib
add_library(dramsim3 SHARED
    src/bankstate.cc
    src/channel_state.cc
    src/command_queue.cc
    src/common.cc
    src/configuration.cc
    src/controller.cc
    src/dram_system.cc
    src/hmc.cc
    src/refresh.cc
    src/simple_stats.cc
    src/timing.cc
    src/memory_system.cc
)

target_include_directories(dramsim3 PUBLIC
 $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
 $<INSTALL_INTERFACE:include/>
)
target_compile_options(dramsim3 PRIVATE -Wall)
target_link_libraries(dramsim3 PRIVATE inih format)
set_target_properties(dramsim3 PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# trace CPU, .etc
add_executable(dramsim3main src/main.cc src/cpu.cc)
target_link_libraries(dramsim3main PRIVATE dramsim3 args)
target_compile_options(dramsim3main PRIVATE)
set_target_properties(dramsim3main PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Unit testing
add_library(Catch INTERFACE)
target_include_directories(Catch INTERFACE ext/headers)

add_executable(dramsim3test EXCLUDE_FROM_ALL
    tests/test_config.cc
    tests/test_dramsys.cc
    tests/test_hmcsys.cc # IDK somehow this can literally crush your computer
)
target_link_libraries(dramsim3test Catch dramsim3)
target_include_directories(dramsim3test PRIVATE src/)

# We have to use this custome command because there's a bug in cmake
# that if you do `make test` it doesn't build your updated test files
# so we're stucking with `make dramsim3test` for now
add_custom_command(
    TARGET dramsim3test POST_BUILD
    COMMAND dramsim3test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    DEPENDS dramsim3test dramsim3
)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)
include(GNUInstallDirs)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CONFIG_FILE "${PROJECT_SOURCE_DIR}/cmake/Config.cmake.in")
set(CONFIG_EXPORT_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/dramsim3")
 set(CONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}/cmake/dramsime3")

configure_package_config_file(
        ${CONFIG_FILE}
        "${PROJECT_BINARY_DIR}/cmake/dramsim3Config.cmake"
        INSTALL_DESTINATION "${CONFIG_EXPORT_DIR}"
        PATH_VARS CMAKE_INSTALL_PREFIX CMAKE_INSTALL_LIBDIR CONFIG_EXPORT_DIR
)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/dramsim3ConfigVersion.cmake"
        VERSION "${VERSION}"
        COMPATIBILITY AnyNewerVersion
)

install(TARGETS dramsim3
        EXPORT dramsim3Targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/dramsim3/
        DESTINATION "include/dramsim3"
        FILES_MATCHING PATTERN "*.h")

install(EXPORT dramsim3Targets
        DESTINATION "${CONFIG_EXPORT_DIR}")


install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/dramsim3Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/dramsim3ConfigVersion.cmake"
        DESTINATION ${CONFIG_LIBDIR}
)
