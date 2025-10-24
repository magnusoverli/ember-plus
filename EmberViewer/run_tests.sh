#!/bin/bash

# EmberViewer Test Runner Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/../build}"

echo "======================================"
echo "   EmberViewer Unit Test Runner"
echo "======================================"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "❌ Build directory not found: $BUILD_DIR"
    echo ""
    echo "Please build the project first:"
    echo "  mkdir -p build && cd build"
    echo "  cmake .. -DBUILD_TESTS=ON"
    echo "  make"
    exit 1
fi

# Navigate to build directory
cd "$BUILD_DIR/EmberViewer"

# Check if tests were built
if [ ! -d "tests" ]; then
    echo "❌ Tests not built. Please rebuild with -DBUILD_TESTS=ON"
    exit 1
fi

echo "📁 Build directory: $BUILD_DIR"
echo "🧪 Running tests..."
echo ""

# Navigate to tests subdirectory for CTest
cd tests

# Run tests with CTest
if [ "$1" == "-v" ] || [ "$1" == "--verbose" ]; then
    echo "Running in verbose mode..."
    ctest --output-on-failure -V
elif [ "$1" == "-vv" ]; then
    echo "Running in extra verbose mode..."
    for test in test_*; do
        if [ -x "$test" ]; then
            echo ""
            echo "========================================" 
            echo "Running: $test"
            echo "========================================"
            ./$test -v2
        fi
    done
elif [ -n "$1" ]; then
    echo "Running specific test: $1"
    if [ -x "$1" ]; then
        ./$1 "${@:2}"
    else
        echo "❌ Test not found: $1"
        echo "Available tests:"
        ls -1 test_* 2>/dev/null || echo "  (none found)"
        exit 1
    fi
else
    ctest --output-on-failure
fi

echo ""

# Check result
if [ $? -eq 0 ]; then
    echo "✅ All tests passed!"
    echo ""
    
    # Show test summary
    TEST_COUNT=$(ls test_* 2>/dev/null | grep -v autogen | wc -l)
    echo "📊 Test Summary:"
    echo "   Test suites: $TEST_COUNT"
    echo "   Test files:"
    for test in test_*; do
        if [ -x "$test" ] && [[ ! "$test" =~ autogen ]]; then
            echo "      - $test"
        fi
    done
else
    echo "❌ Some tests failed!"
    exit 1
fi

echo ""
echo "======================================"
echo "       Testing Complete"
echo "======================================"
