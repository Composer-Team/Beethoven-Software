# Internal helper used by both beethovenConfig.cmake and
# beethoven_baremetalConfig.cmake. Not part of the public API — call
# beethoven_build() instead, which is defined in each Config file and
# delegates to _beethoven_build_common() here.
#
# Args:
#   TARGET       — name of the executable cmake target to add
#   LINK_TARGET  — imported target to link (e.g. APEX::beethoven-discrete)
#   SOURCES <…>  — user source files; the binding's beethoven_hardware.cc
#                  is appended automatically

function(_beethoven_build_common TARGET LINK_TARGET)
    cmake_parse_arguments(_BBC "" "" "SOURCES" ${ARGN})

    if(NOT BEETHOVEN_PROJECT_ROOT)
        message(FATAL_ERROR
            "beethoven_build: BEETHOVEN_PROJECT_ROOT is not set. "
            "This macro is meant to be invoked via the `beethoven` CLI.")
    endif()
    if(NOT _BBC_SOURCES)
        message(FATAL_ERROR "beethoven_build: SOURCES is required.")
    endif()

    set(_binding ${BEETHOVEN_PROJECT_ROOT}/target/binding)

    add_executable(${TARGET}
        ${_BBC_SOURCES}
        ${_binding}/beethoven_hardware.cc)

    set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES)

    target_include_directories(${TARGET} PRIVATE ${_binding})
    target_link_libraries(${TARGET} PRIVATE ${LINK_TARGET})
endfunction()
