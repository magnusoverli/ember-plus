# Device Snapshot Feature

## Overview
The "Save Ember Device" feature allows users to capture and save a complete snapshot of a connected Ember+ device, including all nodes, parameters, matrices, and functions with their current values and states.

## Implementation

### Files Added
- `include/DeviceSnapshot.h` - Data structures and serialization interface
- `src/DeviceSnapshot.cpp` - JSON serialization implementation

### Files Modified
- `include/MainWindow.h` - Added snapshot capture methods
- `src/MainWindow.cpp` - Implemented capture logic and UI integration
- `CMakeLists.txt` - Added new source files to build

## Usage

### Saving a Device
1. Connect to an Ember+ device
2. Wait for the device tree to populate
3. Select **File → Save Ember Device...** (or press **Ctrl+S**)
4. Choose a location and filename
5. The device snapshot will be saved as a JSON file

### File Format
Snapshots are saved in JSON format (`.json` extension) with the following structure:

```json
{
  "formatVersion": 1,
  "deviceName": "localhost",
  "captureTime": "2025-10-26T20:30:00",
  "hostAddress": "localhost",
  "port": 9092,
  "statistics": {
    "nodes": 45,
    "parameters": 234,
    "matrices": 3,
    "functions": 12
  },
  "nodes": [...],
  "parameters": [...],
  "matrices": [...],
  "functions": [...]
}
```

### What Gets Captured

**Nodes:**
- Path (e.g., "1.2.3")
- Identifier
- Description
- Online status
- Child element paths

**Parameters:**
- Path
- Identifier
- Current value
- Type (Integer, Real, String, Boolean, Enum, Trigger)
- Access level (ReadOnly, WriteOnly, ReadWrite)
- Constraints (minimum, maximum)
- Enumeration options (if applicable)
- Online status

**Matrices:**
- Path
- Identifier
- Description
- Type (1:N, 1:1, N:N)
- Dimensions (source/target counts)
- Source and target labels
- All connection states (which sources are connected to which targets)

**Functions:**
- Path
- Identifier
- Description
- Argument names and types
- Result names and types

## Technical Details

### Data Structures
- `NodeData` - Node information
- `ParameterData` - Parameter values and metadata
- `MatrixData` - Matrix configuration and connections
- `FunctionData` - Function signatures
- `DeviceSnapshot` - Container for all device data

### JSON Serialization
Each data structure implements:
- `toJson()` - Serialize to QJsonObject
- `fromJson()` - Deserialize from QJsonObject

The `DeviceSnapshot` class provides:
- `toJson()` - Create complete JSON document
- `saveToFile()` - Write to file with indentation
- `fromJson()` - Parse JSON document
- `loadFromFile()` - Read from file (for future use)

### Capture Process
1. Iterate through all tree items
2. Extract data based on element type (Node/Parameter/Matrix/Function)
3. For parameters: read metadata from Qt item UserRole data
4. For matrices: extract connection states from MatrixWidget
5. For functions: read from cached FunctionInfo map
6. Serialize all data to JSON
7. Save to file with pretty-printing (indented format)

## Future Enhancements

Possible additions:
- Load snapshot for viewing (read-only)
- Compare current device vs snapshot (diff view)
- Export to other formats (XML, CSV)
- Compress large snapshots (gzip)
- Batch capture multiple devices
- Schedule automatic snapshots

## Notes

- Snapshots are point-in-time captures - parameter values reflect the state at capture time
- Must be connected to a device to capture
- Large devices (1000+ parameters) may take a few seconds to capture
- JSON format is human-readable and can be inspected/edited in any text editor
- Format version field allows for future format evolution
