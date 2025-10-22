# Contributing to EmberViewer

Thank you for your interest in contributing to EmberViewer! This document provides guidelines and information for contributors.

## Table of Contents
- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Submitting Changes](#submitting-changes)
- [Bug Reports](#bug-reports)
- [Feature Requests](#feature-requests)

---

## Code of Conduct

### Our Pledge
- Be respectful and inclusive
- Accept constructive criticism gracefully
- Focus on what's best for the community
- Show empathy towards other community members

### Unacceptable Behavior
- Harassment, trolling, or derogatory comments
- Personal or political attacks
- Publishing others' private information
- Other conduct inappropriate in a professional setting

---

## Getting Started

### First Time Contributors

Welcome! Here's how to get started:

1. **Fork the repository**
   ```bash
   # Click "Fork" on GitHub
   git clone https://github.com/YOUR_USERNAME/ember-plus.git
   cd ember-plus
   ```

2. **Set up upstream**
   ```bash
   git remote add upstream https://github.com/magnusoverli/ember-plus.git
   ```

3. **Create a branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

4. **Make your changes**
   - See [Development Setup](#development-setup) below

5. **Submit a Pull Request**
   - See [Submitting Changes](#submitting-changes) below

### Good First Issues

Look for issues labeled:
- `good first issue` - Perfect for newcomers
- `help wanted` - We'd love community help
- `documentation` - Improve docs
- `bug` - Fix a reported bug

---

## Development Setup

### Prerequisites

See [BUILDING.md](BUILDING.md) for detailed build instructions.

Quick summary:
```bash
# Install dependencies (Ubuntu/Debian example)
sudo apt-get install build-essential cmake qtbase5-dev git

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make EmberViewer -j$(nproc)
```

### Project Structure

```
EmberViewer/
├── CMakeLists.txt       # Build configuration
├── docs/                # Documentation
│   ├── ARCHITECTURE.md
│   ├── BUILDING.md
│   ├── CONTRIBUTING.md (this file)
│   └── ROADMAP.md
├── include/             # Public headers
│   ├── MainWindow.h
│   └── EmberConnection.h
├── src/                 # Implementation files
│   ├── main.cpp
│   ├── MainWindow.cpp
│   └── EmberConnection.cpp
├── ui/                  # Qt UI files (future)
└── resources/           # Icons, images, etc.
```

### Development Workflow

1. **Keep your fork updated**
   ```bash
   git fetch upstream
   git checkout master
   git merge upstream/master
   git push origin master
   ```

2. **Create feature branch**
   ```bash
   git checkout -b feature/my-new-feature
   ```

3. **Make changes and test**
   ```bash
   # Edit files
   cd build
   make EmberViewer
   ./EmberViewer/EmberViewer  # Test your changes
   ```

4. **Commit with good messages**
   ```bash
   git add -p  # Stage changes interactively
   git commit -m "Add feature X: Brief description
   
   More detailed explanation of what and why.
   Fixes #123"
   ```

5. **Push and create PR**
   ```bash
   git push origin feature/my-new-feature
   # Then create PR on GitHub
   ```

---

## Coding Standards

### C++ Style

We follow modern C++ practices with Qt conventions.

#### General Guidelines

- **C++ Standard**: C++11 minimum, prefer modern features
- **Indentation**: 4 spaces (no tabs)
- **Line Length**: 120 characters max
- **Naming Conventions**:
  ```cpp
  class MyClass;              // PascalCase for classes
  void myFunction();          // camelCase for functions
  int m_memberVariable;       // m_ prefix for member variables
  const int MAX_SIZE = 100;   // UPPER_CASE for constants
  ```

#### Example Code Style

```cpp
// Good
class EmberConnection : public QObject
{
    Q_OBJECT

public:
    explicit EmberConnection(QObject *parent = nullptr);
    ~EmberConnection();

    void connectToHost(const QString &host, int port);
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void logMessage(const QString &message);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();

private:
    void processMessage(const QByteArray &data);

    QTcpSocket *m_socket;
    QString m_host;
    int m_port;
    bool m_isConnected;
};
```

#### Qt-Specific

- Use Qt types: `QString`, `QByteArray`, `QList`, etc.
- Use signals/slots instead of callbacks
- Use `Q_OBJECT` macro for signal/slot classes
- Use `nullptr` instead of `NULL` or `0`
- Use `auto` for complex iterator types
- Use `emit` keyword for clarity

#### Comments

```cpp
/**
 * Brief description of function.
 * 
 * Detailed description if needed, explaining:
 * - What the function does
 * - Important parameters
 * - Return value meaning
 * - Any side effects
 * 
 * @param host The hostname or IP address
 * @param port The port number (1-65535)
 * @return true if connection initiated successfully
 */
bool connectToHost(const QString &host, int port);

// Single-line comment for brief explanations
void simpleFunction();
```

### CMake Style

- Use lowercase for commands: `cmake_minimum_required`, not `CMAKE_MINIMUM_REQUIRED`
- Use `${VAR}` for variable expansion
- Add comments for non-obvious logic
- Keep lines under 100 characters

### Git Commit Messages

Follow these conventions:

```
<type>: <subject>

<body>

<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Build process, dependencies, etc.

**Example:**
```
feat: Add parameter value editing support

- Implement editable widgets for integer/float/string parameters
- Add validation for min/max ranges
- Send parameter set commands via Glow protocol
- Update property panel UI

Closes #42
```

---

## Submitting Changes

### Pull Request Process

1. **Update documentation**
   - Update README.md if adding features
   - Add/update code comments
   - Update CHANGELOG.md (if exists)

2. **Test thoroughly**
   - Build on your platform
   - Test the specific feature/fix
   - Check for regressions
   - Ideally test on multiple platforms

3. **Create Pull Request**
   - Use a clear, descriptive title
   - Reference related issues
   - Describe what and why
   - Add screenshots/videos if UI changes

4. **PR Template**
   ```markdown
   ## Description
   Brief description of changes
   
   ## Motivation
   Why is this change needed?
   
   ## Changes
   - Bullet list of changes
   - Be specific
   
   ## Testing
   How did you test this?
   
   ## Screenshots
   (if applicable)
   
   ## Checklist
   - [ ] Code follows style guidelines
   - [ ] Comments added for complex logic
   - [ ] Documentation updated
   - [ ] Tested on target platform(s)
   - [ ] No compiler warnings
   ```

5. **Code Review**
   - Respond to feedback professionally
   - Make requested changes
   - Push updates to same branch
   - Resolve conversations when addressed

6. **Merge**
   - Maintainer will merge when approved
   - Squash commits if requested
   - Delete branch after merge

### What Makes a Good PR?

✅ **Good:**
- Focused on one feature/fix
- Small, reviewable size
- Well-tested
- Documented
- Follows coding standards

❌ **Avoid:**
- Mixing multiple unrelated changes
- Massive PRs (> 500 lines)
- Breaking existing functionality
- Style-only changes to unrelated code
- Unnecessary reformatting

---

## Bug Reports

### Before Submitting

1. **Search existing issues** - It might already be reported
2. **Use latest version** - Bug might be fixed
3. **Isolate the problem** - Minimal reproduction steps

### Bug Report Template

```markdown
**Describe the bug**
Clear description of what the bug is.

**To Reproduce**
Steps to reproduce:
1. Launch EmberViewer
2. Connect to '...'
3. Click on '...'
4. See error

**Expected behavior**
What you expected to happen.

**Actual behavior**
What actually happened.

**Screenshots**
If applicable, add screenshots.

**Environment:**
- OS: [e.g. Ubuntu 22.04, Windows 11, macOS 13]
- EmberViewer Version: [e.g. 1.0.0]
- Qt Version: [e.g. 5.15.2]
- Compiler: [e.g. GCC 11.2]

**Additional context**
Any other relevant information.

**Logs**
```
Paste relevant log output from console
```
```

---

## Feature Requests

### Before Requesting

1. **Check roadmap** - See [ROADMAP.md](ROADMAP.md)
2. **Search existing** - Might already be planned
3. **Consider scope** - Is it core functionality?

### Feature Request Template

```markdown
**Is your feature related to a problem?**
Clear description of the problem.

**Describe the solution you'd like**
What you want to happen.

**Describe alternatives considered**
Other approaches you've thought about.

**Use cases**
Who would use this and how?

**Additional context**
Mockups, examples, references, etc.
```

---

## Development Guidelines

### Adding New Features

1. **Discuss first** - Open an issue to discuss approach
2. **Start small** - MVP first, iterate later
3. **Keep it modular** - Don't tightly couple components
4. **Think cross-platform** - Test on multiple OS
5. **Document** - Add comments and user docs

### Refactoring

1. **Justify** - Explain why refactoring is needed
2. **Don't mix** - Don't add features while refactoring
3. **Test** - Ensure no behavior changes
4. **Incremental** - Small, focused refactorings

### Performance

- Profile before optimizing
- Measure improvements
- Document performance-critical code
- Consider memory usage on large trees

### Security

- Validate all input
- Handle errors gracefully
- No hard-coded credentials
- Consider network security (future: TLS)

---

## Testing

### Manual Testing

Current state: Manual testing only

**Test checklist:**
- [ ] Application launches
- [ ] Connection succeeds/fails appropriately
- [ ] Tree displays correctly
- [ ] Console logs messages
- [ ] No crashes or hangs
- [ ] Memory leaks (use valgrind/sanitizers)

### Automated Testing (Future)

Planned:
- Unit tests for core logic
- Integration tests for protocol
- UI tests (Qt Test framework)
- CI/CD pipeline

---

## Documentation

### When to Update Docs

- Adding features → Update README.md
- Changing build → Update BUILDING.md
- Architectural changes → Update ARCHITECTURE.md
- Planning features → Update ROADMAP.md
- API changes → Update code comments

### Documentation Style

- Use clear, simple language
- Include examples
- Use code blocks with syntax highlighting
- Add screenshots for UI features
- Keep it up-to-date

---

## Community

### Getting Help

- **Questions**: Open a GitHub Discussion
- **Bugs**: Open an Issue
- **Chat**: [Link to Discord/Slack if available]
- **Email**: [maintainer email if public]

### Recognition

Contributors are recognized in:
- Git history
- Release notes
- CONTRIBUTORS file (when created)

---

## License

By contributing, you agree that your contributions will be licensed under the Boost Software License 1.0, the same license as the project.

---

## Questions?

Don't hesitate to ask! We're here to help:
- Open a GitHub Discussion
- Comment on relevant issues
- Reach out to maintainers

Thank you for contributing to EmberViewer! 🎉

