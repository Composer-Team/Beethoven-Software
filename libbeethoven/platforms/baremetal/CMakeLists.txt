include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if("${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
    message(FATAL_ERROR
        "Baremetal build requires a toolchain file. Use "
        "-DCMAKE_TOOLCHAIN_FILE=${LIBBEETHOVEN_SOURCE_DIR}/cmake/arm-none-eabi.cmake")
endif()

if("$ENV{M55_SRC}" STREQUAL "")
    message(FATAL_ERROR "M55_SRC env var not set. Point it at the Cortex-M55 source tree.")
endif()
set(M55_SRC $ENV{M55_SRC})
set(TB_LOC ${M55_SRC}/AT633-BU-50000-r1p0-00eac0/yamin/logical/testbench/execution_tb/tests)
set(STARTUP_FILE ${TB_LOC}/Device/ARM/exectb_mcu/Source/GCC/startup_exectb_mcu.S)
set_source_files_properties(${STARTUP_FILE} PROPERTIES
    LANGUAGE C
    COMPILE_FLAGS "-x assembler-with-cpp")

set(M55_SOURCES
    ${TB_LOC}/Device/ARM/exectb_mcu/Source/system_exectb_mcu.c
    ${TB_LOC}/retarget_exectb_mcu.c
    ${TB_LOC}/EXECTB_tests.c
    ${M55_SRC}/TM840-BU-50000-r2p0-00rel0/daplite2/logical/testbench/soc_tb/system/gcc_retarget.c
    ${STARTUP_FILE})

set(_root ${LIBBEETHOVEN_SOURCE_DIR})

add_library(beethoven_baremetal STATIC
    ${_root}/src/rocc_cmd.cc
    ${_root}/src/util.cc
    ${_root}/src/rocc_response.cc
    ${_root}/src/fpga_handle_impl/fpga_handle_baremetal.cc
    ${M55_SOURCES})

set_target_properties(beethoven_baremetal PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(beethoven_baremetal PUBLIC
    BAREMETAL BEETHOVEN_BAREMETAL=1
    _FPU _EPU _MVE _MVE1 _DCACHE_PRESENT _ICACHE_PRESENT)

target_include_directories(beethoven_baremetal PUBLIC
    $<BUILD_INTERFACE:${_root}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    ${TB_LOC}
    ${TB_LOC}/Device/ARM/exectb_mcu/Include
    ${TB_LOC}/CMSIS/Include
    ${TB_LOC}/Device/ARM/exectb_mcu
    ${M55_SRC}/AT633-BU-50000-r1p0-00eac0/yamin/logical/yamin/pmc_files
    ${M55_SRC}/AT637-BU-50000-r0p0-00eac0/pmc100/software/api)

install(TARGETS beethoven_baremetal
        EXPORT beethoven_baremetalTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(DIRECTORY ${_root}/include/beethoven/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/beethoven
        FILES_MATCHING PATTERN "*.h")

install(EXPORT beethoven_baremetalTargets
        NAMESPACE APEX::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/beethoven_baremetal)

configure_package_config_file(
    ${_root}/cmake/beethoven_baremetalConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/beethoven_baremetalConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/beethoven_baremetal
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/beethoven_baremetalConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/beethoven_baremetalConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/beethoven_baremetalConfigVersion.cmake
    ${_root}/cmake/BeethovenBuildHelpers.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/beethoven_baremetal)

install(FILES ${_root}/cmake/arm-none-eabi.cmake
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/beethoven_baremetal)

install(CODE "
    set(_dir \"\$ENV{HOME}/.cmake/packages/beethoven_baremetal\")
    string(MD5 _hash \"${CMAKE_INSTALL_PREFIX}\")
    file(MAKE_DIRECTORY \"\${_dir}\")
    file(WRITE \"\${_dir}/\${_hash}\"
         \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/beethoven_baremetal\")
    message(STATUS \"Registered beethoven_baremetal package at \${_dir}/\${_hash}\")
")
