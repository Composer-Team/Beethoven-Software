# BeethovenSimulator.cmake
# Provides beethoven_testbench() function for building simulation testbenches
#
# Usage:
#   beethoven_testbench(my_test
#     SOURCES test.cc                     # Required: testbench source files
#     HARDWARE my_accel                   # Required: beethoven_hardware() target name
#     SIMULATOR verilator                 # Optional: verilator|icarus|vcs (default: verilator)
#     DRAMSIM_CONFIG path/to/config.ini   # Optional: DRAMsim3 config file
#   )

# Find the runtime directory
# First check if we're in an installed location (share/beethoven/runtime exists)
# Otherwise fall back to source tree location
