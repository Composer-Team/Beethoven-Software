#!/bin/bash
#
# Test runner for the unified CMake build system
#
# This script:
# 1. Sets up environment variables
# 2. Ensures Beethoven runtime library is installed
# 3. Runs cmake configure and build
# 4. Executes the test
#
# Usage: ./run_test.sh [--clean]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BEETHOVEN_SOFTWARE="$(cd "$SCRIPT_DIR/../.." && pwd)"
COMPOSER_ROOT="$(cd "$BEETHOVEN_SOFTWARE/.." && pwd)"
BEETHOVEN_HARDWARE="$COMPOSER_ROOT/Beethoven-Hardware"
BUILD_DIR="$SCRIPT_DIR/build"

echo "=============================================="
echo "Unified CMake Build System Test"
echo "=============================================="
echo "Script dir:          $SCRIPT_DIR"
echo "Composer root:       $COMPOSER_ROOT"
echo "Beethoven-Software:  $BEETHOVEN_SOFTWARE"
echo "Beethoven-Hardware:  $BEETHOVEN_HARDWARE"
echo "Build dir:           $BUILD_DIR"
echo "=============================================="

# Check for --clean flag
if [[ "$1" == "--clean" ]]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Set environment variables
export BEETHOVEN_HARDWARE_PATH="$BEETHOVEN_HARDWARE"

# Check that Beethoven-Hardware exists
if [[ ! -f "$BEETHOVEN_HARDWARE/build.sbt" ]]; then
    echo "ERROR: Beethoven-Hardware not found at $BEETHOVEN_HARDWARE"
    exit 1
fi

# Check that beethoven package is installed
if ! cmake --find-package -DNAME=beethoven -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST 2>/dev/null; then
    echo ""
    echo "WARNING: beethoven CMake package may not be installed."
    echo "If cmake fails, install it first:"
    echo "  cd $BEETHOVEN_SOFTWARE"
    echo "  mkdir -p build && cd build"
    echo "  cmake .. -DPLATFORM=discrete -DBEETHOVEN_HARDWARE_PATH=$BEETHOVEN_HARDWARE"
    echo "  make -j && sudo make install"
    echo ""
    echo "The BEETHOVEN_HARDWARE_PATH option saves the path for future builds."
    echo ""
fi

# Create and enter build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ""
echo "Step 1: CMake Configure"
echo "------------------------"
cmake "$SCRIPT_DIR"

echo ""
echo "Step 2: Build (this will run sbt + verilator)"
echo "----------------------------------------------"
cmake --build . --parallel

echo ""
echo "Step 3: Run Test"
echo "-----------------"
./test_unified

echo ""
echo "=============================================="
echo "Test Complete"
echo "=============================================="
