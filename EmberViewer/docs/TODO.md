# EmberViewer TODO List

Active development tasks and priorities.

> **Last Updated**: October 22, 2025  
> **Version**: 1.0.0 (Foundation Complete)

---

## 🎯 Current Sprint - v1.1 Preparation

### Critical Path Items

#### 1. Full Glow Protocol Implementation
**Status**: 🚧 Not Started  
**Priority**: P0 (Blocker for all other features)  
**Assignee**: Unassigned

**Tasks:**
- [ ] Study TinyEmberPlus Glow implementation as reference
- [ ] Implement AsyncDomReader integration
- [ ] Create GlowMessageHandler class
- [ ] Parse QualifiedParameter messages
- [ ] Parse QualifiedNode messages
- [ ] Parse QualifiedMatrix messages
- [ ] Parse QualifiedFunction messages
- [ ] Handle StreamEntry for subscriptions
- [ ] Add comprehensive error handling
- [ ] Unit tests for each message type

**Files to Create/Modify:**
- `include/GlowMessageHandler.h` (new)
- `src/GlowMessageHandler.cpp` (new)
- `src/EmberConnection.cpp` (modify)

**Reference:**
- `tinyember/TinyEmberPlus/glow/Consumer.cpp`
- Ember+ Documentation

---

#### 2. GetDirectory Command Implementation
**Status**: 🚧 Not Started  
**Priority**: P0  
**Assignee**: Unassigned

**Tasks:**
- [ ] Study S101 StreamEncoder API
- [ ] Implement Glow encoder wrapper
- [ ] Create GetDirectory request builder
- [ ] Send request on connection
- [ ] Handle GetDirectory responses
- [ ] Implement recursive directory requests

**Files to Modify:**
- `src/EmberConnection.cpp`
- May need `include/GlowEncoder.h` (new)

---

#### 3. Tree Population from Provider
**Status**: 🚧 Not Started  
**Priority**: P0  
**Depends on**: Tasks 1, 2  
**Assignee**: Unassigned

**Tasks:**
- [ ] Create tree model structure
- [ ] Map Glow nodes to QTreeWidgetItems
- [ ] Implement lazy loading
- [ ] Add node type icons
- [ ] Show parameter values in tree
- [ ] Handle tree updates
- [ ] Add expand/collapse state management

**Files to Modify:**
- `src/MainWindow.cpp`
- May need `include/TreeModel.h` (new)

---

### High Priority

#### 4. Property Panel Enhancement
**Status**: 🚧 Not Started  
**Priority**: P1  
**Depends on**: Task 1

**Tasks:**
- [ ] Design property panel layout
- [ ] Create PropertyWidget class
- [ ] Display all parameter metadata:
  - [ ] Value
  - [ ] Type
  - [ ] Min/Max
  - [ ] Format
  - [ ] Description
  - [ ] Schema
  - [ ] Enum values
  - [ ] Formula (if present)
- [ ] Add copy-to-clipboard functionality

**Files to Create:**
- `include/PropertyWidget.h`
- `src/PropertyWidget.cpp`
- `ui/PropertyPanel.ui` (Qt Designer file)

---

#### 5. Connection Robustness
**Status**: 🚧 Not Started  
**Priority**: P1

**Tasks:**
- [ ] Add connection timeout (10s default)
- [ ] Implement keep-alive mechanism
- [ ] Add auto-reconnect with backoff
- [ ] Better error messages
- [ ] Connection state indicators
- [ ] Graceful disconnect handling

**Files to Modify:**
- `src/EmberConnection.cpp`
- `src/MainWindow.cpp`

---

### Medium Priority

#### 6. Search & Filter
**Status**: 📋 Planned  
**Priority**: P2  
**Depends on**: Task 3

**Tasks:**
- [ ] Add search bar to UI
- [ ] Implement case-insensitive text search
- [ ] Filter by node type (param/node/matrix/function)
- [ ] Highlight matches in tree
- [ ] Navigate between search results
- [ ] Search history dropdown

---

#### 7. Settings & Preferences
**Status**: 📋 Planned  
**Priority**: P2

**Tasks:**
- [ ] Use QSettings for persistence
- [ ] Save window geometry
- [ ] Save connection history
- [ ] Save favorite connections
- [ ] Preferences dialog:
  - [ ] Default connection settings
  - [ ] UI theme selection
  - [ ] Auto-reconnect behavior
  - [ ] Log verbosity

**Files to Create:**
- `include/SettingsDialog.h`
- `src/SettingsDialog.cpp`
- `ui/SettingsDialog.ui`

---

#### 8. Keyboard Shortcuts
**Status**: 📋 Planned  
**Priority**: P2

**Shortcuts to Implement:**
- [ ] Ctrl+O - Quick Connect
- [ ] Ctrl+D - Disconnect
- [ ] Ctrl+F - Search
- [ ] Ctrl+R - Refresh tree
- [ ] Ctrl+W - Close connection
- [ ] Ctrl+Q - Quit
- [ ] F5 - Refresh
- [ ] Ctrl+C - Copy selected value
- [ ] Esc - Clear search

---

### Low Priority (Nice to Have)

#### 9. Console Enhancements
**Status**: 📋 Planned  
**Priority**: P3

**Tasks:**
- [ ] Add log level filtering (debug/info/warning/error)
- [ ] Color-coded messages
- [ ] Timestamp format options
- [ ] Clear console button
- [ ] Export logs to file
- [ ] Search in console
- [ ] Auto-scroll toggle

---

#### 10. UI Polish
**Status**: 📋 Planned  
**Priority**: P3

**Tasks:**
- [ ] Add application icon
- [ ] Add toolbar icons
- [ ] Add node type icons in tree
- [ ] Improve status bar indicators
- [ ] Add loading spinner during connection
- [ ] Tooltip improvements
- [ ] Better widget spacing
- [ ] Splash screen (optional)

---

## 🐛 Known Issues

### Critical
- None currently

### High
- [ ] Memory leak check needed (run with valgrind)
- [ ] Large trees may cause UI lag (need virtual scrolling)

### Medium
- [ ] Deprecation warning with Qt signal (using errorOccurred now)
- [ ] Console scrolling doesn't auto-follow new messages

### Low
- [ ] Window doesn't save position between sessions
- [ ] No way to clear console

---

## 🚀 Future Major Features

### Version 1.2 - Editing
- [ ] Parameter value editing
- [ ] Enum parameter dropdowns
- [ ] Boolean parameter checkboxes
- [ ] String parameter text input
- [ ] Numeric parameter spin boxes with validation
- [ ] Apply/Cancel buttons
- [ ] Subscription management

### Version 1.3 - Matrix Support
- [ ] Matrix grid widget
- [ ] Drag-and-drop routing
- [ ] Source/target visualization
- [ ] Take/Clear commands
- [ ] Matrix labels

### Version 1.4 - Functions
- [ ] Function parameter input
- [ ] Invoke button
- [ ] Result display
- [ ] Async function handling

### Version 2.0 - Pro Features
- [ ] Multiple provider tabs
- [ ] Service discovery (mDNS)
- [ ] Export/import tree
- [ ] Parameter value graphing
- [ ] Dashboard mode
- [ ] Scripting support

---

## 📊 Technical Debt

### Code Quality
- [ ] Add unit tests
- [ ] Add integration tests
- [ ] Set up CI/CD pipeline
- [ ] Add code coverage reporting
- [ ] Static analysis integration (clang-tidy)
- [ ] Memory leak detection in CI

### Documentation
- [ ] Add Doxygen comments to all public APIs
- [ ] Generate API documentation
- [ ] Create user manual
- [ ] Add video tutorials
- [ ] Write architecture decision records (ADRs)

### Build System
- [ ] Add CPack configuration
- [ ] Create installers (NSIS for Windows, DMG for Mac)
- [ ] Add codesigning
- [ ] Set up auto-update mechanism

---

## 🔍 Research Tasks

### Protocol Deep Dive
- [ ] Study all Glow message types in detail
- [ ] Understand formula evaluation
- [ ] Matrix routing patterns
- [ ] Stream subscription lifecycle
- [ ] Error handling best practices

### Performance
- [ ] Profile tree rendering with 10k+ nodes
- [ ] Benchmark S101 decoding performance
- [ ] Investigate Qt virtual tree models
- [ ] Research background threading for parsing

### Platform-Specific
- [ ] Test on Windows 10, Windows 11
- [ ] Test on macOS 12, 13, 14
- [ ] Test on various Linux distros
- [ ] Mobile platforms (Qt for iOS/Android)

---

## 📝 Notes & Ideas

### Ideas Under Consideration
- **Dark Mode**: Qt stylesheet or native support?
- **Plugins**: Allow custom parameter renderers?
- **Remote Access**: Web interface via WebSocket?
- **Multi-user**: Collaborative editing?
- **Audit Log**: Track all parameter changes?

### Decisions Needed
- Tree widget vs Tree model/view (current: widget, may need model for large trees)
- SQLite for caching? (probably not needed yet)
- Separate thread for network I/O? (Qt async is fine for now)

### Performance Targets
- Startup time: < 1 second
- Connection time: < 2 seconds
- Tree load (1000 nodes): < 500ms
- UI responsiveness: 60 FPS
- Memory usage: < 200 MB for typical use

---

## 🎓 Learning Resources

For contributors working on specific areas:

### Ember+ Protocol
- Read: `documentation/Ember+ Documentation.pdf`
- Study: `libember/Headers/ember/glow/`
- Reference: TinyEmberPlus source code

### Qt Development
- Qt Documentation: https://doc.qt.io/
- Qt Examples: Check Qt installation examples
- Book: "C++ GUI Programming with Qt 5"

### CMake
- CMake Documentation: https://cmake.org/documentation/
- Modern CMake: https://cliutils.gitlab.io/modern-cmake/

---

## 📅 Milestones

### v1.1 - Core Protocol (Target: Q1 2026)
- Full Glow encoding/decoding
- Tree population from provider
- Complete parameter display
- Basic error handling

### v1.2 - Editing (Target: Q2 2026)
- Parameter value editing
- Subscription management
- Enhanced property panel

### v1.3 - Matrix (Target: Q3 2026)
- Matrix grid widget
- Routing control
- Matrix-specific features

### v2.0 - Pro (Target: 2027)
- Multiple providers
- Service discovery
- Advanced features
- Production ready

---

## ✅ Completed Tasks

### v1.0 Foundation (October 2025)
- [x] Project structure
- [x] CMake build system
- [x] Cross-platform support (Linux/Windows/macOS)
- [x] Main window UI
- [x] Connection management
- [x] TCP socket communication
- [x] S101 frame decoding (basic)
- [x] Tree view widget
- [x] Property panel placeholder
- [x] Console logging
- [x] Documentation (README, ARCHITECTURE, ROADMAP, etc.)
- [x] Git repository setup
- [x] C23 compatibility fix for libember_slim

---

## 📧 Contact

Questions about TODO items? 
- Open a GitHub Issue
- Check CONTRIBUTING.md
- Reach out to maintainers

---

*This TODO list is actively maintained. Check back often for updates!*

