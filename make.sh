#!/bin/bash
set -euo pipefail

# Build script for timeframe builder

# EIC shell may export these with paths that break the selected Python runtime.
unset PYTHONHOME PYTHONPATH

echo "=== Building Timeframe Builder ==="

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_INSTALL_PREFIX=../install

# Build the application
make -j$(nproc)

# Install
make install

cd ..

echo "Build complete!"
echo ""
echo "To run the timeframe merger:"
echo "  ./install/bin/timeframe_builder [options] input_file1 [input_file2 ...]"
echo ""
echo "Example usage:"
echo "  ./install/bin/timeframe_builder -n 10 -d 1000.0 -s -e 2 input.edm4hep.root"
echo ""
echo "For help:"
echo "  ./install/bin/timeframe_builder -h"