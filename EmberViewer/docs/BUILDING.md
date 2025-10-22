# Building EmberViewer

Detailed build instructions for all supported platforms.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Linux](#linux)
- [Windows](#windows)
- [macOS](#macos)
- [Build Options](#build-options)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

### All Platforms
- **CMake** 3.9 or later
- **C++11** compatible compiler
- **Qt5** 5.9 or later

### Specific Platform Requirements

#### Linux
- GCC 7+ or Clang 5+
- Qt5 development packages
- Make or Ninja

#### Windows
- Visual Studio 2017 or later (with C++ Desktop workload)
- Qt5 (from official installer or vcpkg)
- CMake (included in VS or separate install)

#### macOS
- Xcode 10 or later
- Qt5 (from official installer or Homebrew)
- CMake (from Homebrew or official installer)

---

## Linux

### Ubuntu/Debian

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install build-essential cmake qtbase5-dev libqt5network5 git

# Clone repository (if not already done)
git clone https://github.com/magnusoverli/ember-plus.git
cd ember-plus

# Create build directory
mkdir -p build
cd build

# Configure
cmake ..

# Build EmberViewer (and dependencies)
make EmberViewer -j$(nproc)

# Optional: Build everything
make -j$(nproc)

# Optional: Install system-wide
sudo make install
```

### Fedora/RHEL/CentOS

```bash
# Install dependencies
sudo dnf install gcc-c++ cmake qt5-qtbase-devel git

# Build steps same as Ubuntu above
```

### Arch Linux

```bash
# Install dependencies
sudo pacman -S base-devel cmake qt5-base git

# Build steps same as Ubuntu above
```

### Run

```bash
# From build directory
./EmberViewer/EmberViewer

# Or if installed
EmberViewer
```

---

## Windows

### Option 1: Visual Studio with Qt

#### Setup

1. **Install Visual Studio 2019 or 2022**
   - Include "Desktop development with C++" workload
   - Include CMake tools

2. **Install Qt5**
   - Download from https://www.qt.io/download-qt-installer
   - Install Qt 5.15.x for MSVC
   - Note the installation path (e.g., `C:\Qt\5.15.2\msvc2019_64`)

3. **Set Qt path** (one of these methods):
   ```cmd
   :: Method 1: Environment variable
   set CMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64

   :: Method 2: Qt Creator users - use Qt Creator IDE

   :: Method 3: Pass to CMake directly (see below)
   ```

#### Build with CMake GUI

1. Open CMake GUI
2. Set source dir: `C:\path\to\ember-plus`
3. Set build dir: `C:\path\to\ember-plus\build`
4. Click "Configure"
5. Select generator: "Visual Studio 16 2019" (or your version)
6. If Qt not found, set `CMAKE_PREFIX_PATH` to Qt install path
7. Click "Generate"
8. Click "Open Project" or run:
   ```cmd
   cmake --build build --config Release --target EmberViewer
   ```

#### Build with Command Line

```cmd
:: Clone repository
git clone https://github.com/magnusoverli/ember-plus.git
cd ember-plus

:: Create build directory
mkdir build
cd build

:: Configure (adjust Qt path as needed)
cmake .. -G "Visual Studio 16 2019" -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64

:: Build
cmake --build . --config Release --target EmberViewer

:: Run
Release\EmberViewer\EmberViewer.exe
```

### Option 2: MinGW with Qt

```cmd
:: Install Qt for MinGW (from Qt installer)
:: Add Qt and MinGW to PATH

mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\mingw81_64
mingw32-make EmberViewer -j4
```

### Deploying

Windows needs Qt DLLs alongside the executable:

```cmd
:: Automatic deployment
cd build\EmberViewer\Release
C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe EmberViewer.exe

:: Manual deployment (copy these DLLs):
:: Qt5Core.dll, Qt5Gui.dll, Qt5Widgets.dll, Qt5Network.dll
:: Plus platform plugin: platforms\qwindows.dll
```

---

## macOS

### Using Homebrew

```bash
# Install dependencies
brew install cmake qt@5 git

# Clone repository
git clone https://github.com/magnusoverli/ember-plus.git
cd ember-plus

# Build
mkdir build
cd build

# Configure (Qt5 path from Homebrew)
cmake .. -DCMAKE_PREFIX_PATH=/usr/local/opt/qt@5

# Or for Apple Silicon
cmake .. -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt@5

# Build
make EmberViewer -j$(sysctl -n hw.ncpu)

# Run
./EmberViewer/EmberViewer.app/Contents/MacOS/EmberViewer

# Or open as app
open ./EmberViewer/EmberViewer.app
```

### Using Qt Installer

```bash
# Install Qt from official installer
# Default location: ~/Qt/5.15.2/clang_64

mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=~/Qt/5.15.2/clang_64
make EmberViewer -j$(sysctl -n hw.ncpu)
```

### Creating DMG (Optional)

```bash
# Install create-dmg
brew install create-dmg

# After building
cd build/EmberViewer
macdeployqt EmberViewer.app -dmg
```

---

## Build Options

### CMake Configuration Options

```bash
# Build type
cmake .. -DCMAKE_BUILD_TYPE=Release      # or Debug, RelWithDebInfo, MinSizeRel

# Use shared libraries (smaller executable, need to distribute .so/.dll)
cmake .. -DBUILD_SHARED_LIBS=ON

# Use static libraries (larger executable, standalone)
cmake .. -DBUILD_SHARED_LIBS=OFF

# Custom install prefix
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/emberviewer

# Specify Qt location
cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt5

# Use Ninja instead of Make (faster)
cmake .. -G Ninja
ninja EmberViewer
```

### Build Targets

```bash
# Build only EmberViewer
make EmberViewer

# Build all (including libember, TinyEmber, etc.)
make

# Build and install
make install

# Build specific component
make ember-static      # libember static library
make ember-shared      # libember shared library
make TinyEmberPlus     # TinyEmberPlus application
```

### Build Configurations

```bash
# Debug build (with symbols, no optimization)
cmake .. -DCMAKE_BUILD_TYPE=Debug
make EmberViewer

# Release build (optimized, no symbols)
cmake .. -DCMAKE_BUILD_TYPE=Release
make EmberViewer

# Release with debug info
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
make EmberViewer
```

---

## Troubleshooting

### Qt Not Found

**Problem**: `CMake Error: Could not find Qt5`

**Solution**:
```bash
# Find Qt installation
# Linux: qmake --version or which qmake
# Windows: Check C:\Qt\
# macOS: brew list qt@5 or check ~/Qt/

# Then set CMAKE_PREFIX_PATH
cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt5/installation
```

### Compiler Errors

**Problem**: `error: 'nullptr' was not declared in this scope`

**Solution**: You need a C++11 compatible compiler
- Linux: GCC 4.8.1+ or Clang 3.3+
- Windows: Visual Studio 2015+
- macOS: Xcode 5+

### Linking Errors

**Problem**: `undefined reference to libember symbols`

**Solution**: Make sure you're building from the root ember-plus directory, not just EmberViewer. The build system needs to build libember first.

```bash
# Don't do this:
cd EmberViewer
mkdir build
cmake ..  # WRONG - needs parent project

# Do this:
cd ember-plus
mkdir build
cmake ..  # Correct - builds all dependencies
make EmberViewer
```

### Windows: Missing MSVCR120.dll or similar

**Problem**: Application won't start due to missing Visual C++ Runtime

**Solution**: Install Visual C++ Redistributable:
- VS 2015-2019: https://aka.ms/vs/16/release/vc_redist.x64.exe
- Or use static runtime (advanced):
  ```cmake
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  ```

### macOS: Qt plugins not found

**Problem**: Application starts but shows errors about missing plugins

**Solution**: Run macdeployqt:
```bash
cd build/EmberViewer
macdeployqt EmberViewer.app
```

### Build is slow

**Solutions**:
```bash
# Use multiple cores
make -j$(nproc)          # Linux
make -j$(sysctl -n hw.ncpu)  # macOS

# Use Ninja (faster than Make)
cmake .. -G Ninja
ninja EmberViewer

# Use ccache (caches compiled objects)
sudo apt-get install ccache
export CMAKE_CXX_COMPILER_LAUNCHER=ccache
```

### Out of memory during build

**Solution**:
```bash
# Reduce parallel jobs
make -j2 EmberViewer  # Use only 2 cores
```

---

## Development Builds

### Quick Iteration

```bash
# One-time setup
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON

# After code changes (from build directory)
make EmberViewer && ./EmberViewer/EmberViewer
```

### With IDE

#### Qt Creator
1. Open `ember-plus/CMakeLists.txt`
2. Configure project (Qt Creator detects CMake automatically)
3. Select EmberViewer target
4. Press Ctrl+R to run

#### Visual Studio
1. Open folder: `ember-plus`
2. VS detects CMakeLists.txt
3. Configure CMake settings
4. Select EmberViewer.exe as startup item
5. Press F5 to debug

#### CLion
1. Open `ember-plus` directory
2. CLion loads CMake project
3. Select EmberViewer configuration
4. Press Shift+F10 to run

---

## Cross-Compilation

### Linux to Windows (MinGW)

```bash
# Install MinGW toolchain
sudo apt-get install mingw-w64 cmake

# Get Qt5 for MinGW (from Qt installer or manually)
# Create toolchain file: mingw-toolchain.cmake
# [toolchain content omitted for brevity]

# Build
mkdir build-win
cd build-win
cmake .. -DCMAKE_TOOLCHAIN_FILE=../mingw-toolchain.cmake
make EmberViewer
```

---

## Packaging

### Create Installer/Package

```bash
# Linux: DEB package
cpack -G DEB

# Linux: RPM package
cpack -G RPM

# Windows: NSIS installer
cpack -G NSIS

# macOS: DMG
cpack -G DragNDrop
```

---

## Getting Help

- **Build issues**: Check [GitHub Issues](https://github.com/magnusoverli/ember-plus/issues)
- **General questions**: Open a discussion
- **Documentation**: See README.md and other docs/ files

