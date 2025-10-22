# EmberViewer Development Roadmap

## Version 1.0 - Foundation ✅ COMPLETED

**Status**: Released  
**Date**: October 2025

### Features
- [x] Basic Qt5 GUI application
- [x] Connection management (host/port)
- [x] TCP socket communication
- [x] S101 frame decoding
- [x] Tree view widget
- [x] Property panel
- [x] Console logging
- [x] Cross-platform build (Linux, Windows, macOS)
- [x] Basic error handling

### Known Limitations
- Glow message encoding/decoding not fully implemented
- No parameter editing
- No matrix controls
- No function invocation
- Tree is not fully populated from provider

---

## Version 1.1 - Core Protocol Implementation 🚧 NEXT

**Target**: Q1 2026  
**Focus**: Complete Ember+ protocol support

### High Priority
- [ ] **Full Glow Decoder**
  - [ ] Parse all Glow message types
  - [ ] Handle QualifiedParameter messages
  - [ ] Handle QualifiedNode messages
  - [ ] Handle QualifiedMatrix messages
  - [ ] Handle QualifiedFunction messages
  - [ ] Process StreamEntry messages

- [ ] **Tree Population**
  - [ ] Send GetDirectory requests
  - [ ] Process root elements
  - [ ] Lazy-load child nodes
  - [ ] Handle tree updates
  - [ ] Show parameter types in tree

- [ ] **Parameter Display**
  - [ ] Show value in property panel
  - [ ] Display min/max/format
  - [ ] Show description
  - [ ] Display enum values
  - [ ] Show formula (if present)

### Medium Priority
- [ ] **Glow Encoder**
  - [ ] Encode GetDirectory commands
  - [ ] Encode Subscribe commands
  - [ ] Encode Unsubscribe commands
  - [ ] Proper S101 frame generation

- [ ] **Connection Improvements**
  - [ ] Better error messages
  - [ ] Connection timeout handling
  - [ ] Automatic reconnection
  - [ ] Keep-alive support

### Low Priority
- [ ] **UI Polish**
  - [ ] Custom icons
  - [ ] Better status indicators
  - [ ] Tooltips
  - [ ] Keyboard shortcuts

**Success Criteria**: Can browse and view full tree from real Ember+ provider

---

## Version 1.2 - Parameter Editing 📝

**Target**: Q2 2026  
**Focus**: Interactive parameter control

### Features
- [ ] **Value Editing**
  - [ ] Integer parameter edit
  - [ ] Real/float parameter edit
  - [ ] String parameter edit
  - [ ] Boolean parameter toggle
  - [ ] Enum parameter dropdown
  - [ ] Value validation (min/max)
  - [ ] Send parameter set commands

- [ ] **Parameter Subscription**
  - [ ] Subscribe to parameter updates
  - [ ] Unsubscribe from parameters
  - [ ] Real-time value updates
  - [ ] Subscription management UI

- [ ] **Property Panel Enhancement**
  - [ ] Editable value widgets
  - [ ] Apply/Cancel buttons
  - [ ] Format-aware input
  - [ ] Error indication

**Success Criteria**: Can control parameters on real devices

---

## Version 1.3 - Matrix Support 🔀

**Target**: Q3 2026  
**Focus**: Matrix routing capabilities

### Features
- [ ] **Matrix Display**
  - [ ] Matrix grid visualization
  - [ ] Source/target lists
  - [ ] Connection state display
  - [ ] Labels and metadata

- [ ] **Matrix Control**
  - [ ] Click to connect/disconnect
  - [ ] Drag and drop routing
  - [ ] Multi-select operations
  - [ ] Take/clear commands

- [ ] **Matrix Types**
  - [ ] One-to-N support
  - [ ] N-to-N support
  - [ ] Linear matrix
  - [ ] Non-linear matrix

**Success Criteria**: Full matrix control for audio routers

---

## Version 1.4 - Functions & Commands 🔧

**Target**: Q4 2026  
**Focus**: Function invocation

### Features
- [ ] **Function Support**
  - [ ] Display function parameters
  - [ ] Argument input widgets
  - [ ] Invoke button
  - [ ] Result display
  - [ ] Async invocation handling

- [ ] **Command Handling**
  - [ ] Batch commands
  - [ ] Command history
  - [ ] Undo/redo (if supported by device)

**Success Criteria**: Can invoke device functions

---

## Version 2.0 - Advanced Features 🚀

**Target**: 2027  
**Focus**: Professional workflow tools

### Major Features

#### Search & Filter
- [ ] Full-text search in tree
- [ ] Filter by type (node/param/matrix/function)
- [ ] Search history
- [ ] Regular expression support
- [ ] Search results panel

#### Connection Profiles
- [ ] Save favorite connections
- [ ] Connection presets
- [ ] Recent connections list
- [ ] Quick connect dropdown
- [ ] Import/export profiles

#### Service Discovery
- [ ] mDNS/Bonjour support
- [ ] Auto-discover Ember+ providers
- [ ] Browse discovered devices
- [ ] Connection from discovery

#### Multiple Providers
- [ ] Multi-tab interface
- [ ] Connect to multiple providers
- [ ] Copy/paste between providers
- [ ] Synchronized views

#### Data Export
- [ ] Export tree to JSON
- [ ] Export tree to XML
- [ ] Export parameter values to CSV
- [ ] Import tree structure
- [ ] Snapshot/restore

#### Visualization
- [ ] Parameter value graphs
- [ ] Real-time plotting
- [ ] Historical data
- [ ] Meter displays
- [ ] Custom dashboards

#### Scripting (Stretch Goal)
- [ ] Embedded script engine
- [ ] Automate tasks
- [ ] Batch operations
- [ ] Python/JavaScript API

**Success Criteria**: Production-ready tool for professionals

---

## Version 2.1+ - Polish & Optimization

### Performance
- [ ] Lazy tree loading
- [ ] Virtual scrolling for large trees
- [ ] Background thread for parsing
- [ ] Caching layer
- [ ] Memory optimization

### UI/UX
- [ ] Dark mode
- [ ] Customizable layout
- [ ] Dockable panels
- [ ] Layout presets
- [ ] Accessibility improvements

### Platform Specific
- [ ] Windows installer
- [ ] macOS DMG
- [ ] Linux .deb/.rpm packages
- [ ] Flatpak/Snap
- [ ] Windows store

### Documentation
- [ ] User manual
- [ ] Video tutorials
- [ ] API documentation
- [ ] Example scripts
- [ ] Developer guide

---

## Community & Contributions

### Open Source Goals
- [ ] Comprehensive test suite
- [ ] CI/CD pipeline
- [ ] Automated releases
- [ ] Contributor guidelines
- [ ] Code of conduct
- [ ] Issue templates

### Integration
- [ ] Plugin API
- [ ] Custom parameter renderers
- [ ] Protocol extensions
- [ ] Third-party integrations

---

## Long-term Vision

### 3-5 Year Outlook
- **Industry Standard**: Become the go-to Ember+ viewer
- **Enterprise Features**: Multi-user, authentication, audit logs
- **Cloud Integration**: Remote access, cloud sync
- **Mobile**: iOS/Android companion apps
- **Protocol Evolution**: Stay current with Ember+ spec updates

---

## How to Contribute

Want to help implement these features?

1. Check the [Issues](https://github.com/magnusoverli/ember-plus/issues) page
2. Look for `good first issue` or `help wanted` labels
3. Comment on the issue you want to work on
4. Fork, develop, and submit a PR
5. Celebrate when merged! 🎉

---

## Feedback & Priorities

Have ideas or need a feature urgently? 
- Open an issue with the `enhancement` label
- Vote on existing feature requests
- Sponsor development for priority features
- Join the discussion in the community forum

---

*This roadmap is subject to change based on community feedback and priorities.*

