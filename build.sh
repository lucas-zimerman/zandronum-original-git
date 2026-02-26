#!/bin/bash
# Build script for Zandronum on Arch Linux
# Based on the GitHub Actions workflow

set -e

FMOD_VER=44464
FMOD_SOVER=4.44.64
BUILD_DIR="build"
BUILD_TYPE="Release"

echo "=== Zandronum Build Script ==="
echo ""

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the zandronum root directory."
    exit 1
fi

# Install dependencies (Arch Linux)
echo "=== Checking dependencies ==="
MISSING_DEPS=""
for dep in cmake g++ make sdl zlib bzip2 libjpeg-turbo fluidsynth gtk2 nasm mesa glew openssl opus wget; do
    if ! pacman -Qi "$dep" &>/dev/null; then
        MISSING_DEPS="$MISSING_DEPS $dep"
    fi
done

if [ -n "$MISSING_DEPS" ]; then
    echo "Missing dependencies:$MISSING_DEPS"
    echo ""
    echo "Install them with:"
    echo "sudo pacman -S base-devel cmake sdl zlib bzip2 libjpeg-turbo fluidsynth gtk2 nasm mesa glew openssl opus wget"
    echo ""
    read -p "Continue anyway? (y/n) " -n 1 -r
    echo
fi

# Download FMOD if needed
if [ ! -d "fmodapi${FMOD_VER}linux" ]; then
    echo "=== Downloading FMOD ==="
    if [ ! -f "fmodapi${FMOD_VER}linux.tar.gz" ]; then
        echo "Downloading FMOD from GitHub releases..."
        wget "https://github.com/DrinkyBird/zan-ci-deps/releases/download/ci-dependencies/fmodapi${FMOD_VER}linux.tar.gz" || {
            echo "Error: Failed to download FMOD. You may need to download it manually."
            exit 1
        }
    fi
    echo "Extracting FMOD..."
    tar xf "fmodapi${FMOD_VER}linux.tar.gz"
fi

# Set FMOD environment variables
export FMOD_INCLUDE_DIR="${PWD}/fmodapi${FMOD_VER}linux/api/inc"
export FMOD_LIBRARY="${PWD}/fmodapi${FMOD_VER}linux/api/lib/libfmodex64-${FMOD_SOVER}.so"

if [ ! -f "$FMOD_LIBRARY" ]; then
    echo "Error: FMOD library not found at $FMOD_LIBRARY"
    exit 1
fi

echo "FMOD_INCLUDE_DIR=$FMOD_INCLUDE_DIR"
echo "FMOD_LIBRARY=$FMOD_LIBRARY"
echo ""

# Configure CMake
echo "=== Configuring CMake ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DRELEASE_WITH_DEBUG_FILE=ON \
    -DFMOD_INCLUDE_DIR="$FMOD_INCLUDE_DIR" \
    -DFMOD_LIBRARY="$FMOD_LIBRARY"

# Build
echo ""
echo "=== Building ==="
cmake --build . --config "$BUILD_TYPE" -j $(nproc)

echo ""
echo "=== Build complete! ==="
echo "Binaries should be in: $BUILD_DIR/"
