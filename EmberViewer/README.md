# EmberViewer

A modern, cross-platform Ember+ protocol viewer built with Qt5 and libember.

## Features

- **Cross-Platform**: Builds and runs on Linux, Windows, and macOS
- **Modern GUI**: Clean Qt5-based interface
- **Connection Manager**: Easy host/port configuration
- **Tree View**: Browse Ember+ provider hierarchy
- **Property Panel**: Inspect parameter details
- **Console Log**: Monitor protocol messages and debugging info
- **S101 Support**: Built-in S101 frame encoding/decoding

## Screenshots

*EmberViewer provides a professional interface for browsing and controlling Ember+ providers*

## Building

### Prerequisites

- CMake 3.9 or later
- C++11 compatible compiler
- Qt5 (Core, Gui, Widgets, Network modules)

### Linux

```bash
mkdir build
cd build
cmake ..
make EmberViewer
```

### Windows

```bash
mkdir build
cd build
cmake ..
cmake --build . --target EmberViewer
```

### macOS

```bash
mkdir build
cd build
cmake ..
make EmberViewer
```

## Running

After building, the executable will be located at:
- Linux/macOS: `build/EmberViewer/EmberViewer`
- Windows: `build\EmberViewer\EmberViewer.exe`

Or install system-wide:
```bash
sudo make install
```

## Usage

1. Launch EmberViewer
2. Enter the host and port of your Ember+ provider (default: localhost:9092)
3. Click "Connect"
4. Browse the tree structure of parameters and nodes
5. View the console log for protocol details

## Architecture

EmberViewer is built using:

- **libember**: Core Ember+ protocol implementation
- **libs101**: S101 framing protocol
- **libformula**: Formula evaluation support
- **Qt5**: Cross-platform GUI framework

### Project Structure

```
EmberViewer/
├── CMakeLists.txt           # Build configuration
├── README.md                # This file
├── include/                 # Header files
│   ├── MainWindow.h        # Main application window
│   └── EmberConnection.h   # Ember+ connection handler
├── src/                     # Source files
│   ├── main.cpp            # Application entry point
│   ├── MainWindow.cpp      # Main window implementation
│   └── EmberConnection.cpp # Connection implementation
├── ui/                      # Qt UI files (future)
└── resources/               # Resources (icons, etc.)
```

## Development Roadmap

### Completed ✅
- [x] Basic application structure
- [x] Qt GUI with tree view
- [x] Connection management
- [x] S101 frame decoding
- [x] Console logging
- [x] Cross-platform build system

### Planned 🚧
- [ ] Full Glow message encoding/decoding
- [ ] Parameter editing (set values)
- [ ] Matrix controls
- [ ] Function invocation
- [ ] Stream subscription
- [ ] Advanced tree features (search, filter)
- [ ] Connection profiles
- [ ] Service discovery (mDNS)
- [ ] Export/import tree structure

## Contributing

Contributions are welcome! This is part of the Ember+ project:
- https://github.com/magnusoverli/ember-plus
- Original project: https://github.com/Lawo/ember-plus

## License

Distributed under the Boost Software License, Version 1.0.
See LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt

## Author

Magnus Overli - [@magnusoverli](https://github.com/magnusoverli)

Based on the Ember+ protocol from Lawo GmbH.

