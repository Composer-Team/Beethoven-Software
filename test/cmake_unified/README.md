# Unified CMake Build System Test

This test exercises the unified CMake build system for Beethoven, which allows
building both hardware (Scala/Chisel) and software (C++ testbench) from a
single CMakeLists.txt.

## What This Tests

1. `beethoven_hardware()` - Generates hardware by running sbt with a local `BEETHOVEN_PATH`
2. `beethoven_testbench()` - Builds a simulation testbench linked to the generated hardware

## Prerequisites

1. **Beethoven runtime library installed:**
   ```bash
   cd Beethoven-Software
   mkdir -p build && cd build
   cmake .. -DPLATFORM=discrete
   make -j && sudo make install
   ```

2. **Verilator installed** (5.0+):
   ```bash
   # macOS
   brew install verilator

   # Ubuntu
   apt install verilator
   ```

3. **sbt installed** for Scala builds

## Running the Test

### Quick Run
```bash
./run_test.sh
```

### Manual Run
```bash
mkdir build && cd build
export BEETHOVEN_HARDWARE_PATH=/path/to/Beethoven-Hardware
cmake ..
make
./test_unified
```

### Clean Rebuild
```bash
./run_test.sh --clean
```

## Expected Output

```
=== Unified CMake Build System Test ===
Testing beethoven_hardware() + beethoven_testbench()

Test: Add 42 to 16 elements
Input:  0 10 20 30 40 50 60 70 80 90 100 110 120 130 140 150
Output: 42 52 62 72 82 92 102 112 122 132 142 152 162 172 182 192

PASS: All 16 elements verified correctly
=== Unified CMake Build System Test PASSED ===
```

## Files

- `CMakeLists.txt` - Uses `beethoven_hardware()` and `beethoven_testbench()`
- `test_unified_build.cc` - C++ testbench source
- `run_test.sh` - Test runner script
- `../../Beethoven-Hardware/src/test/scala/basic/CMakeUnifiedTest.scala` - Scala config
