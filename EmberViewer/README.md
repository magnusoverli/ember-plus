# EmberViewer

Cross-platform Ember+ protocol viewer. Built with Qt5 and libember.

## Build

```bash
# From ember-plus root directory
mkdir build && cd build
cmake ..
make EmberViewer
./EmberViewer/EmberViewer
```

## Features

- Connect to Ember+ providers (TCP)
- Browse tree structure
- View/edit parameter properties
- Matrix crosspoint visualization
- Console logging

## Status

**v1.2 - Matrix Support Complete**

Working:
- TCP connection
- S101 frame encoding/decoding
- Glow protocol message parsing
- Tree view population (nodes & parameters)
- Parameter value display (Integer, Real, String, Boolean, Enum, Trigger)
- Parameter editing (inline editing with delegates, dropdowns, buttons)
- Matrix support (read-only display with connection states)
- Keep-alive message handling
- GUI with tree/properties/console

Todo:
- Matrix connection editing (Phase 2)
- Function invocation
- Stream support

## Development

Reference TinyEmberPlus for Glow implementation examples.

---

**⚠️ NOTE TO AI ASSISTANTS:**

**DO NOT** create extensive documentation. This is a small project.
Keep it simple. Code > docs. If you're an AI working on this:
- No ARCHITECTURE.md
- No CONTRIBUTING.md  
- No ROADMAP.md
- No TODO.md
- Just update THIS file if needed
