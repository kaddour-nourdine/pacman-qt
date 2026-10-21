#!/bin/bash

# Coverage report for the core model.
#
# Builds the model and its tests with the compiler's coverage counters,
# runs the whole test suite and prints the report of gcovr. The numbers in
# docs/coverage.md come from this script.
#
# Usage: ./tools/coverage.sh [build-dir]

set -e

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT/build-cov}"

if ! command -v gcovr &> /dev/null; then
    echo "Error: gcovr is not installed (pip install gcovr)."
    exit 1
fi

echo "Configuring $BUILD_DIR with coverage counters..."
cmake -S "$ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DPACMAN_BUILD_APP=OFF \
    -DPACMAN_BUILD_TOOLS=ON \
    -DPACMAN_BUILD_TESTS=ON \
    -DCMAKE_CXX_FLAGS="--coverage -O0 -g" \
    -DCMAKE_EXE_LINKER_FLAGS="--coverage"

echo "Building..."
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "Running the tests..."
ctest --test-dir "$BUILD_DIR" --output-on-failure

# --exclude-unreachable-branches drops the branches the compiler proved to
# be impossible, --exclude-throw-branches the exception edges, which would
# otherwise dominate the numbers of the branch coverage.
echo ""
echo "Coverage of core/src:"
cd "$BUILD_DIR"
gcovr --root "$ROOT" \
    --filter "$ROOT/core/src" \
    --exclude-unreachable-branches \
    --exclude-throw-branches \
    --print-summary

echo ""
echo "Per file:"
gcovr --root "$ROOT" \
    --filter "$ROOT/core/src" \
    --exclude-unreachable-branches \
    --exclude-throw-branches \
    --txt

echo ""
mkdir -p "$BUILD_DIR/coverage"
gcovr --root "$ROOT" \
    --filter "$ROOT/core/src" \
    --exclude-unreachable-branches \
    --exclude-throw-branches \
    --html-details "$BUILD_DIR/coverage/index.html" \
    > /dev/null
echo "An HTML report was written to $BUILD_DIR/coverage/index.html"
