cmake_minimum_required(VERSION 3.20)
project(beethoven LANGUAGES C CXX ASM VERSION 0.0.0)

option(BEETHOVEN_BAREMETAL "Build the Cortex-M55 baremetal variant" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# libbeethoven (the installed library + cmake configs) lives under
# libbeethoven/. The runtime daemon is its own per-project cmake project
# under runtime/, installed as a source package — see libbeethoven's
# host CMakeLists for the install(DIRECTORY runtime/) rule.
add_subdirectory(libbeethoven)
