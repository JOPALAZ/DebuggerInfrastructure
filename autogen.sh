#!/bin/bash

# Exit on error
set -e

# Configuration variables
BUILD_DIR="build"
BUILD_TYPE="Release"

# Determine the number of threads
if command -v nproc &> /dev/null; then
    # Linux: Get the number of threads
    NUM_THREADS=$(nproc)
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS: Get the number of threads
    NUM_THREADS=$(sysctl -n hw.ncpu)
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    # Windows: Get the number of threads
    NUM_THREADS=$(wmic cpu get NumberOfLogicalProcessors | grep -Eo '[0-9]+')
else
    echo "Unsupported OS. Please specify the number of threads manually."
    exit 1
fi

# Calculate number of processes (2 * threads)
NUM_PROCESSES=$((2 * NUM_THREADS))

# Print usage
function usage() {
    echo "Usage: $0 [-b build_dir] [-t build_type] [-j num_processes]"
    echo "  -b  Build directory (default: ${BUILD_DIR})"
    echo "  -t  Build type (default: ${BUILD_TYPE})"
    echo "  -j  Number of processes for parallel build (default: ${NUM_PROCESSES})"
    exit 1
}

# Parse arguments
while getopts "b:t:j:h" opt; do
    case ${opt} in
        b) BUILD_DIR="${OPTARG}" ;;
        t) BUILD_TYPE="${OPTARG}" ;;
        j) NUM_PROCESSES="${OPTARG}" ;;
        h) usage ;;
        *) usage ;;
    esac
done

# Create build directory
echo "Creating build directory: ${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# Run CMake configuration
echo "Configuring project with CMake..."
cmake -S . -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

# Build the project
echo "Building project with ${NUM_PROCESSES} processes..."
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -- -j "${NUM_PROCESSES}"

# Print success message
echo "Build completed successfully!"
