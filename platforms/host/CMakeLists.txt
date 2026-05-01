set(BEETHOVEN_PLATFORMS "discrete;zynq" CACHE STRING
    "Host-CPU platforms to build (semicolon-separated subset of: discrete zynq)")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(_root ${CMAKE_SOURCE_DIR})

set(_core_sources
    ${_root}/src/rocc_cmd.cc
    ${_root}/src/util.cc
    ${_root}/src/rocc_response.cc
    ${_root}/src/response_handle.cc
    ${_root}/src/allocator/alloc.cc
    ${_root}/src/runtime_ipc.cc)

option(ENABLE_CACHE_FLUSH "Enable software cache flushing on Zynq (ARM64 only)" OFF)

foreach(_plat IN LISTS BEETHOVEN_PLATFORMS)
    set(_lib beethoven-${_plat})

    add_library(${_lib} SHARED
        ${_core_sources}
        ${_root}/src/fpga_handle_impl/fpga_handle_${_plat}.cc)

    string(TOUPPER ${_plat} _PLAT_UPPER)
    target_compile_definitions(${_lib} PUBLIC ${_PLAT_UPPER})

    set_target_properties(${_lib} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        POSITION_INDEPENDENT_CODE ON)

    target_include_directories(${_lib} PUBLIC
        $<BUILD_INTERFACE:${_root}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    if(UNIX AND NOT APPLE)
        target_link_libraries(${_lib} PUBLIC rt pthread)
    elseif(APPLE)
        target_link_libraries(${_lib} PUBLIC pthread)
    endif()

    if(_plat STREQUAL "zynq" AND ENABLE_CACHE_FLUSH)
        target_compile_definitions(${_lib} PUBLIC ENABLE_CACHE_FLUSH)
        message(STATUS "Software cache flushing enabled for libbeethoven-zynq")
    endif()

    install(TARGETS ${_lib}
            EXPORT beethoven-${_plat}-targets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(EXPORT beethoven-${_plat}-targets
            NAMESPACE APEX::
            FILE beethoven-${_plat}-targets.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/beethoven)
endforeach()

install(DIRECTORY ${_root}/include/beethoven/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/beethoven
        FILES_MATCHING PATTERN "*.h")

configure_package_config_file(
    ${_root}/cmake/beethovenConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/beethovenConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/beethoven
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/beethovenConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/beethovenConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/beethovenConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/beethoven)

if("discrete" IN_LIST BEETHOVEN_PLATFORMS)
    add_executable(merge_sort  ${_root}/test/merge_sort.cc)
    add_executable(alloc_sizes ${_root}/test/alloc_sizes.cc)
    target_link_libraries(merge_sort  PRIVATE beethoven-discrete)
    target_link_libraries(alloc_sizes PRIVATE beethoven-discrete)
endif()

install(CODE "
    set(_dir \"\$ENV{HOME}/.cmake/packages/beethoven\")
    string(MD5 _hash \"${CMAKE_INSTALL_PREFIX}\")
    file(MAKE_DIRECTORY \"\${_dir}\")
    file(WRITE \"\${_dir}/\${_hash}\"
         \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/beethoven\")
    message(STATUS \"Registered beethoven package at \${_dir}/\${_hash}\")
")
