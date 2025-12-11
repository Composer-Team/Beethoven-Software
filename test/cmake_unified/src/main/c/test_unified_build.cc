/**
 * Test for the unified CMake build system.
 * This testbench exercises the beethoven_hardware() and beethoven_testbench()
 * CMake functions by running a simple accelerator test.
 */

#include <beethoven_hardware.h>
#include <beethoven/fpga_handle.h>
#include <cstdio>
#include <cstdlib>

using namespace beethoven;

int main(int argc, char* argv[]) {
    printf("=== Unified CMake Build System Test ===\n");
    printf("Testing beethoven_hardware() + beethoven_testbench()\n\n");

    // Initialize FPGA handle (simulation mode)
    fpga_handle_t handle;

    // Test parameters
    using dtype = int32_t;
    constexpr int vec_len = 16;
    constexpr dtype addend = 10;

    printf("Test: Add %d to %d elements\n", addend, vec_len);

    // Allocate device memory
    auto vec = handle.malloc(vec_len * sizeof(dtype));
    auto host = reinterpret_cast<dtype*>(vec.getHostAddr());

    // Initialize input data
    printf("Input:  ");
    for (int i = 0; i < vec_len; ++i) {
        host[i] = i * 10;
        printf("%d ", host[i]);
    }
    printf("\n");

    TestSystem::set_addend(0, addend);
    // Run accelerator: add 'addend' to all elements
    // TestSystem is defined in CMakeUnifiedTest.scala
    TestSystem::my_accel(0, vec_len, vec).get();

    TestSystem::ping(0, 4);

    // TestSystem::ping(0, 3).get();

    // Verify results
    printf("Output: ");
    int errors = 0;
    for (int i = 0; i < vec_len; ++i) {
        dtype expected = (i * 10) + addend;
        printf("%d ", host[i]);
        if (host[i] != expected) {
            errors++;
        }
    }
    printf("\n");

    // Report results
    printf("\n");
    if (errors == 0) {
        printf("PASS: All %d elements verified correctly\n", vec_len);
        printf("=== Unified CMake Build System Test PASSED ===\n");
        return 0;
    } else {
        printf("FAIL: %d/%d elements incorrect\n", errors, vec_len);
        printf("=== Unified CMake Build System Test FAILED ===\n");
        return 1;
    }
}
