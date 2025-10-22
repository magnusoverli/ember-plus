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
- View parameter properties
- Console logging

## Status

**v1.0 - Foundation Complete**

Working:
- TCP connection
- S101 frame decoding (basic)
- GUI with tree/properties/console

Todo:
- Full Glow message parsing
- Parameter editing
- Matrix support

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
