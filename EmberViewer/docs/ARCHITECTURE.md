# EmberViewer Architecture

## Overview

EmberViewer is a modern Qt5-based application designed to interact with Ember+ providers using the libember protocol implementation.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────┐
│                    EmberViewer GUI                      │
│                     (Qt5 Widgets)                       │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │
│  │  MainWindow  │  │  TreeView    │  │  Property   │  │
│  │              │  │              │  │  Panel      │  │
│  └──────┬───────┘  └──────────────┘  └─────────────┘  │
│         │                                              │
│         │                                              │
│  ┌──────▼───────────────────────────────────────────┐ │
│  │         EmberConnection                          │ │
│  │  (Protocol Handler & State Manager)              │ │
│  └──────┬───────────────────────────────────────────┘ │
│         │                                              │
├─────────┼──────────────────────────────────────────────┤
│         │         Qt Network Layer                     │
│  ┌──────▼────────┐                                    │
│  │  QTcpSocket   │                                    │
│  └──────┬────────┘                                    │
├─────────┼──────────────────────────────────────────────┤
│         │         libember Stack                       │
│  ┌──────▼────────┐  ┌─────────────┐  ┌─────────────┐ │
│  │  S101 Codec   │  │  BER Codec  │  │ Glow Types  │ │
│  │  (libs101)    │  │  (libember) │  │ (libember)  │ │
│  └───────────────┘  └─────────────┘  └─────────────┘ │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
              ┌──────────────────┐
              │  Ember+ Provider │
              │  (Device/Server) │
              └──────────────────┘
```

## Component Responsibilities

### MainWindow
- **Purpose**: Main application UI container
- **Responsibilities**:
  - Layout management (tree, properties, console)
  - Menu bar and toolbar
  - Connection controls (host/port input)
  - Status display
  - User event handling

### EmberConnection
- **Purpose**: Ember+ protocol communication handler
- **Responsibilities**:
  - TCP socket management
  - S101 frame encoding/decoding
  - Glow message processing
  - Connection state management
  - Signal emission for UI updates

### Tree View
- **Purpose**: Hierarchical display of Ember+ tree
- **Responsibilities**:
  - Display nodes, parameters, functions, matrices
  - Handle user selection
  - Context menus
  - Expand/collapse navigation

### Property Panel
- **Purpose**: Display and edit selected item properties
- **Responsibilities**:
  - Show parameter values
  - Allow value editing
  - Display metadata (min, max, format, etc.)
  - Invoke functions

### Console Log
- **Purpose**: Protocol debugging and monitoring
- **Responsibilities**:
  - Display connection events
  - Show decoded messages
  - Log errors and warnings
  - Timestamp all entries

## Data Flow

### Connection Sequence

```
User Action → MainWindow::onConnectClicked()
                    │
                    ▼
            EmberConnection::connectToHost()
                    │
                    ▼
            QTcpSocket::connectToHost()
                    │
                    ▼
            [TCP Connection Established]
                    │
                    ▼
            EmberConnection::sendGetDirectory()
                    │
                    ▼
            S101 Encoder → Glow Message → Socket
```

### Message Reception

```
Socket Data Available → EmberConnection::onDataReceived()
                              │
                              ▼
                       S101 Decoder (callback-based)
                              │
                              ▼
                       Message Type Check
                              │
                    ┌─────────┴─────────┐
                    ▼                   ▼
            EmBER Message          Keep-Alive
                    │                   │
                    ▼                   ▼
            Glow Decoder          Process & Respond
                    │
                    ▼
            DOM Tree Update
                    │
                    ▼
            Emit signals to MainWindow
                    │
                    ▼
            Update UI (Tree/Properties)
```

## Threading Model

Currently **single-threaded** on the main Qt event loop:
- All network I/O is async via Qt signals/slots
- S101 decoding is synchronous but fast
- UI updates happen on main thread

**Future consideration**: Move heavy Glow parsing to worker thread if needed.

## State Management

### Connection States
- `Disconnected` - No active connection
- `Connecting` - TCP connection in progress
- `Connected` - TCP connected, handshake complete
- `Error` - Connection failed or lost

### UI State
- Connection controls enabled/disabled based on state
- Tree cleared on disconnect
- Status bar reflects current state

## Protocol Handling

### S101 Frame Structure
```
┌──────┬─────────┬─────────┬─────────┬───────┬─────┬──────────┬──────┐
│ BOF  │  Slot   │ Message │ Command │ Ver   │Flags│   DTD    │ Data │
│ 0xFE │ 1 byte  │ 1 byte  │ 1 byte  │1 byte │1 b. │  1 byte  │  ... │
└──────┴─────────┴─────────┴─────────┴───────┴─────┴──────────┴──────┘
```

### Glow Message Types (BER-encoded)
- **GetDirectory** - Request tree structure
- **QualifiedParameter** - Parameter with full path
- **QualifiedNode** - Node with full path
- **Command** - Subscribe, Unsubscribe, etc.
- **StreamEntry** - Subscribed parameter updates

## Error Handling

### Network Errors
- Socket errors caught and logged
- Auto-reconnect (future enhancement)
- Graceful degradation

### Protocol Errors
- Invalid S101 frames discarded
- Malformed Glow messages logged
- Continue processing valid messages

## Configuration

Currently hardcoded defaults:
- Host: `localhost`
- Port: `9092`

**Future**: Save/load connection profiles from QSettings

## Dependencies

### Build-time
- Qt5 (Core, Gui, Widgets, Network)
- libember (Ember+ protocol)
- libs101 (S101 framing)
- libformula (Formula evaluation)
- CMake 3.9+
- C++11 compiler

### Run-time
- Qt5 libraries
- libember shared library (if built shared)

## Cross-Platform Considerations

### Windows
- Uses Qt's native Windows look
- windeployqt for DLL deployment
- Optional app icon resource

### Linux
- Native GTK/Qt theme integration
- Desktop entry file (future)
- Standard paths for config

### macOS
- Application bundle support
- Retina display support
- macOS-specific menu bar

## Performance Characteristics

### Memory
- Base: ~80-100 MB (Qt + libember)
- Per tree node: ~500 bytes
- Large trees (10k+ nodes): ~100-150 MB

### CPU
- Idle: < 1%
- Active streaming: 2-5%
- Large tree parsing: 10-20% spike

### Network
- Minimal bandwidth (< 1 KB/s idle)
- Scales with subscribed parameters
- Efficient S101 framing

## Security Considerations

- **No authentication**: Currently no user/password support
- **Plain text**: No TLS/encryption
- **Local network**: Designed for trusted networks
- **Future**: Add authentication and encryption support

## Future Architecture Changes

### Planned Enhancements
1. **Async Glow parsing**: Move to background thread
2. **Connection pooling**: Multiple simultaneous providers
3. **Plugin system**: Custom parameter renderers
4. **Database caching**: Persist tree structure
5. **Real-time graphing**: Parameter value visualization

### Scalability
- Current: < 10k nodes
- Target: 100k+ nodes with lazy loading
- Solution: Virtual tree with on-demand loading

