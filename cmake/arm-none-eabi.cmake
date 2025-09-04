set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross compiler
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# Don't run the linker on compiler check
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(TB_LOC $ENV{M55_SRC}/AT633-BU-50000-r1p0-00eac0/yamin/logical/testbench/execution_tb/tests/)
set(CMAKE_EXE_LINKER_FLAGS "-L ${TB_LOC} 
    -T ${TB_LOC}/Device/ARM/exectb_mcu/Source/GCC/mem.ld")

set(CMAKE_C_FLAGS "-fno-exceptions -march=armv8.1-m.main+mve -mtune=cortex-m55+nomve.fp -mcmse -mfloat-abi=hard -mfpu=fpv5-sp-d16 -fomit-frame-pointer -D__GCC")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")


# Disable shared libraries (bare metal doesn't support them)
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

# Set the target properties
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)