# EmberViewer Logging System

## Overview

EmberViewer includes a cross-platform structured logging system that writes JSON-formatted logs to disk. This helps diagnose issues when the application is running on user systems.

## Features

- ✅ **Structured JSON logs** - Easy to parse and analyze
- ✅ **Automatic log rotation** - Prevents unbounded disk usage (10MB per file)
- ✅ **Cross-platform paths** - Uses platform-appropriate directories
- ✅ **Session tracking** - Each session has a unique ID
- ✅ **Configurable levels** - Debug, Info, Warning, Error, Critical
- ✅ **Qt integration** - Captures qDebug/qWarning/etc automatically

## Log File Locations

The application stores logs in platform-specific directories:

### Linux
```
~/.local/share/EmberViewer/logs/
```

### Windows
```
C:\Users\<username>\AppData\Local\EmberViewer\logs\
```

### macOS
```
~/Library/Application Support/EmberViewer/logs/
```

## Accessing Logs

From the EmberViewer application:

1. **Help** → **Open Log Directory** - Opens the log folder in your file manager
2. **Help** → **Show Log File Path** - Displays the current log file path

## Log Format

Each log entry is a single-line JSON object:

```json
{
  "timestamp": "2025-10-23T14:30:00",
  "level": "INFO",
  "category": "connection",
  "message": "Successfully connected to Ember+ provider",
  "session_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "metadata": {
    "host": "192.168.1.100",
    "port": 9000
  }
}
```

## Log Categories

- **application** - App lifecycle events (startup, shutdown)
- **connection** - Network connection events
- **parameter** - Parameter value changes
- **matrix** - Matrix connection operations
- **qt** - Qt framework messages

## Log Rotation

- **Max file size**: 10 MB per file
- **Max log files**: 5 files kept (oldest deleted automatically)
- Files are named with timestamps: `ember-viewer_2025-10-23_14-30-00.log`

## For Developers

### Using the Logger

```cpp
#include "Logger.h"

// Simple logging
LOG_INFO("category", "Message");

// With metadata
QJsonObject metadata;
metadata["key"] = "value";
LOG_INFO("category", "Message with context", metadata);

// All levels
LOG_DEBUG("category", "Debug message");
LOG_INFO("category", "Info message");
LOG_WARNING("category", "Warning message");
LOG_ERROR("category", "Error message");
LOG_CRITICAL("category", "Critical message");
```

### Configuration

```cpp
// Set minimum log level (default: Info)
Logger::instance()->setMinimumLevel(Logger::Level::Debug);

// Change rotation settings
Logger::instance()->setMaxLogFileSize(20 * 1024 * 1024); // 20MB
Logger::instance()->setMaxLogFiles(10); // Keep 10 files
```

## Analyzing Logs

Since logs are JSON, you can use standard tools:

```bash
# View recent logs
tail -f ~/.local/share/EmberViewer/logs/ember-viewer_*.log | jq .

# Filter by category
cat ember-viewer_*.log | jq 'select(.category == "connection")'

# Find errors
cat ember-viewer_*.log | jq 'select(.level == "ERROR")'

# Extract all connection metadata
cat ember-viewer_*.log | jq 'select(.category == "connection") | .metadata'
```

## Privacy Considerations

Logs may contain:
- ✅ Connection host/port information
- ✅ Parameter paths and values
- ✅ Matrix operations
- ✅ Error messages
- ❌ No passwords or authentication tokens
- ❌ No user-identifiable information

Logs are stored locally only and are not transmitted anywhere.

