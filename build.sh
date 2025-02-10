#!/bin/bash
# Exit on error
set -e

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure cmake
cmake_flags="-DCMAKE_BUILD_TYPE=Release"
# If debug flag is passed, use Debug build
if [ "$1" == "--debug" ]; then
  cmake_flags="-DCMAKE_BUILD_TYPE=Debug"
fi

# Check for Python environment
if ! command -v python3 &>/dev/null; then
  echo "Error: Python 3 is required but not found"
  exit 1
fi

# Function to install pybind11 from source
install_pybind11_from_source() {
  echo "Installing pybind11 from source..."
  cd ..
  if [ ! -d "pybind11" ]; then
    git clone https://github.com/pybind/pybind11.git
  fi
  cd pybind11
  git fetch --tags
  git checkout v2.11.1
  cmake -S . -B build -DPYBIND11_TEST=OFF
  cmake --build build
  sudo cmake --install build
  cd ../build
}

# Check pybind11 version and install from source if needed
if pkg-config --exists pybind11; then
  PYBIND_VERSION=$(pkg-config --modversion pybind11)
  if [ "$(echo "$PYBIND_VERSION 2.10.0" | awk '{if ($1 >= $2) print 1; else print 0;}')" -eq 0 ]; then
    echo "pybind11 version $PYBIND_VERSION is too old, installing from source..."
    install_pybind11_from_source
  else
    echo "Found compatible pybind11 version $PYBIND_VERSION"
  fi
else
  echo "pybind11 not found, installing from source..."
  install_pybind11_from_source
fi

# Configure project
echo "Configuring project..."
cmake .. $cmake_flags

# Build using all available cores
echo "Building project..."
cmake --build . -- -j$(nproc)

# Run tests if they exist
if [ -f "tests/run_tests" ]; then
  echo "Running tests..."
  ./tests/run_tests
fi

echo "Build complete!"
cd ..

# Create symbolic links to the binaries in the project root
if [ -f "build/rug_pull_detector" ]; then
  ln -sf build/rug_pull_detector .
fi

# Create symbolic link to the Python module
py_module=$(find build -name "rugpull_detector*.so")
if [ -n "$py_module" ]; then
  ln -sf "$py_module" .
  echo "Python module built successfully: $(basename "$py_module")"
fi

# Check if build was successful
echo
if [ -f "rug_pull_detector" ] && [ -n "$py_module" ]; then
  echo "Build successful! Both C++ executable and Python module are available."
  echo "  - C++ executable: ./rug_pull_detector"
  echo "  - Python module: $(basename "$py_module")"
else
  echo "Warning: Some components may not have built correctly."
  [ ! -f "rug_pull_detector" ] && echo "  - C++ executable not found"
  [ -z "$py_module" ] && echo "  - Python module not found"
fi
