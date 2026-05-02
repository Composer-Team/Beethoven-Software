cmake_minimum_required(VERSION 3.20)
project(beethoven LANGUAGES C CXX ASM VERSION 0.0.0)

option(BEETHOVEN_BAREMETAL "Build the Cortex-M55 baremetal variant" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# === libbeethoven ===
# The installed library + cmake configs. Self-contained — knows nothing
# about the sibling runtime/ tree.
add_subdirectory(libbeethoven)

# === runtime cmake project (source-package install only) ===
# The runtime daemon is design-coupled, so we don't build it here — the
# CLI cmake's it per project against the installed source tree below.
# Skipped entirely for baremetal: that variant has no daemon (the user
# binary mmio's the chip directly).
include(GNUInstallDirs)
if(NOT BEETHOVEN_BAREMETAL)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/runtime/
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/beethoven/runtime-src
            # Don't ship build artifacts or VCS metadata.
            PATTERN ".git"   EXCLUDE
            PATTERN "_cmake" EXCLUDE
            PATTERN "build"  EXCLUDE
            PATTERN "*.o"    EXCLUDE
            PATTERN "*.so"   EXCLUDE
            PATTERN "*.a"    EXCLUDE)
endif()
