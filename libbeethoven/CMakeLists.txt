# libbeethoven — the public C++ API installed once globally to
# ${CMAKE_INSTALL_PREFIX}/lib + /include + /lib/cmake.
#
# Two cmake packages live underneath: `beethoven` (host: discrete + zynq
# side-by-side via COMPONENTS) and `beethoven_baremetal` (Cortex-M55).
# Toggled by -DBEETHOVEN_BAREMETAL.
#
# This file is invocable two ways:
#   - From the repo-root CMakeLists.txt, via `add_subdirectory(libbeethoven)`.
#     The repo-root project() has already declared name + version; the
#     project() guard below skips the redundant declaration.
#   - Standalone: `cmake -S libbeethoven -B build`. In that mode this
#     file IS the top-level cmake project, so it declares its own.
#
# LIBBEETHOVEN_SOURCE_DIR is the absolute path to *this* directory —
# platforms/{host,baremetal}/CMakeLists.txt use it for `${_root}/include`,
# `${_root}/src`, etc.

cmake_minimum_required(VERSION 3.20)

if(NOT PROJECT_NAME)
    # Standalone invocation — declare our own project.
    project(libbeethoven LANGUAGES C CXX ASM VERSION 0.0.0)
    option(BEETHOVEN_BAREMETAL "Build the Cortex-M55 baremetal variant" OFF)
endif()

set(LIBBEETHOVEN_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

if(BEETHOVEN_BAREMETAL)
    add_subdirectory(platforms/baremetal)
else()
    add_subdirectory(platforms/host)
endif()
