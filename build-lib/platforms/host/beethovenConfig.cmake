
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was beethovenConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

set_and_check(BEETHOVEN_INCLUDE_DIR     "${PACKAGE_PREFIX_DIR}/include")
# Source-package location of the runtime cmake project. The CLI passes
# this to `cmake -S ...` to build the per-project BeethovenRuntime daemon.
set_and_check(BEETHOVEN_RUNTIME_SRC_DIR "${PACKAGE_PREFIX_DIR}/share/beethoven/runtime-src")

# Default to discrete if no components requested
if(NOT beethoven_FIND_COMPONENTS)
    set(beethoven_FIND_COMPONENTS discrete)
endif()

foreach(_comp ${beethoven_FIND_COMPONENTS})
    set(_targets "${CMAKE_CURRENT_LIST_DIR}/beethoven-${_comp}-targets.cmake")
    if(EXISTS "${_targets}")
        include("${_targets}")
        set(beethoven_${_comp}_FOUND TRUE)
    else()
        set(beethoven_${_comp}_FOUND FALSE)
        if(beethoven_FIND_REQUIRED_${_comp})
            set(beethoven_FOUND FALSE)
            set(beethoven_NOT_FOUND_MESSAGE
                "beethoven component '${_comp}' was not installed. "
                "Available targets: ${CMAKE_CURRENT_LIST_DIR}/beethoven-*-targets.cmake")
        endif()
    endif()
endforeach()

check_required_components(beethoven)

# Shared body for beethoven_build() — defines _beethoven_build_common().
include("${CMAKE_CURRENT_LIST_DIR}/BeethovenBuildHelpers.cmake")

function(beethoven_build TARGET)
    if(NOT BEETHOVEN_PLATFORM)
        message(FATAL_ERROR "beethoven_build: BEETHOVEN_PLATFORM is not set.")
    endif()

    # Auto-load the platform's targets file if find_package didn't already
    # bring it in. Lets users write `find_package(beethoven REQUIRED)` without
    # having to thread COMPONENTS through their CMakeLists.
    if(NOT TARGET APEX::beethoven-${BEETHOVEN_PLATFORM})
        set(_targets "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/beethoven-${BEETHOVEN_PLATFORM}-targets.cmake")
        if(NOT EXISTS "${_targets}")
            message(FATAL_ERROR
                "beethoven_build: platform '${BEETHOVEN_PLATFORM}' is not in this libbeethoven install. "
                "Reinstall Beethoven-Software with -DBEETHOVEN_PLATFORMS containing '${BEETHOVEN_PLATFORM}'.")
        endif()
        include("${_targets}")
    endif()

    _beethoven_build_common(${TARGET} APEX::beethoven-${BEETHOVEN_PLATFORM} ${ARGN})
endfunction()
