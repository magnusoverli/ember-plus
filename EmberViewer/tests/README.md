# EmberViewer Unit Tests

## Overview

This directory contains unit tests for the EmberViewer application using Qt Test framework.

## Test Coverage

### 1. `test_constants.cpp`
Tests all named constants defined in the application:
- MainWindow constants (timeouts, ports, magic numbers)
- EmberConnection constants (connection timeout)
- Validates relationships between constants
- Ensures all values are in valid ranges

### 2. `test_path_parsing.cpp`
Tests Ember+ path parsing and manipulation:
- Basic path splitting (e.g., "1.2.3")
- Empty and single-level paths
- Path construction from parts
- Matrix label path pattern detection
- Path depth calculation
- Path prefix matching

### 3. `test_matrix_widget.cpp`
Tests MatrixWidget functionality:
- Widget creation and initialization
- Setting/getting matrix info (type, dimensions)
- Target and source label management
- Connection state management
- Multiple simultaneous connections
- Connection clearing
- Different connection dispositions (Tally, Modified, Pending, Locked)
- Crosspoints enable/disable

### 4. `test_parameter_delegate.cpp`
Tests ParameterDelegate functionality:
- Parameter metadata storage (Qt::UserRole data)
- Integer, Real, String, Boolean, Enum parameter types
- Read-only vs read-write parameters
- Parameter type constants validation
- Min/max constraint handling

## Building and Running Tests

### Build Tests
```bash
cd /path/to/ember-plus
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make
```

### Run All Tests
```bash
cd build/EmberViewer
ctest --output-on-failure
```

### Run Individual Tests
```bash
cd build/EmberViewer/tests
./test_constants
./test_path_parsing
./test_matrix_widget
./test_parameter_delegate
```

### Verbose Output
```bash
ctest -V
# or
./test_constants -v2  # Qt Test verbose output
```

## Test Structure

Each test follows the Qt Test framework pattern:
```cpp
class TestClassName : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()    // Run once before all tests
    void init()            // Run before each test
    void testMethod()      // Individual test
    void cleanup()         // Run after each test
    void cleanupTestCase() // Run once after all tests
};

QTEST_MAIN(TestClassName)
#include "test_file.moc"
```

## Adding New Tests

1. Create new test file in `tests/` directory:
   ```cpp
   #include <QtTest/QtTest>
   // Your test class
   ```

2. Add to `tests/CMakeLists.txt`:
   ```cmake
   add_emberviewer_test(test_new_feature)
   ```

3. Rebuild and run:
   ```bash
   make
   ctest
   ```

## Test Assertions

Qt Test provides various assertion macros:
- `QVERIFY(condition)` - Verify condition is true
- `QCOMPARE(actual, expected)` - Compare two values
- `QVERIFY2(condition, message)` - Verify with custom message
- `QTEST(actual, expected)` - Test data-driven tests
- `QBENCHMARK { code }` - Benchmark code performance

## Coverage (Optional)

To generate code coverage:
```bash
cmake .. -DBUILD_TESTS=ON -DENABLE_COVERAGE=ON
make
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

## CI Integration

Tests can be integrated into CI/CD pipelines:
```yaml
# .github/workflows/test.yml
- name: Run tests
  run: |
    cd build
    ctest --output-on-failure
```

## Test Philosophy

These tests focus on:
- **Unit testing** - Testing individual components in isolation
- **Behavioral testing** - Testing public APIs and contracts
- **Regression prevention** - Catching bugs before they reach production
- **Documentation** - Tests serve as usage examples

What these tests DON'T cover (by design):
- **Integration testing** - Network protocol testing requires mock Ember+ provider
- **UI testing** - Widget interaction testing is limited without QTest::mouseClick
- **Performance testing** - Benchmarking requires real Ember+ workloads

## Troubleshooting

### Tests don't build
- Ensure Qt5Test is installed: `apt-get install qtbase5-dev`
- Check CMake finds Qt5: Look for "Found Qt5Test" in cmake output

### Tests fail
- Check test output: `ctest --output-on-failure`
- Run individual test: `./test_name -v2`
- Verify constants match source code values

### Tests timeout
- Increase timeout: `ctest --timeout 60`
- Check for infinite loops or deadlocks

## Future Test Ideas

- [ ] EmberConnection signal emissions
- [ ] Parameter caching logic
- [ ] Settings save/load
- [ ] Tree item creation and lookup
- [ ] Function invocation dialog
- [ ] Mock Ember+ protocol messages
- [ ] Performance benchmarks

---

**Test Count:** 4 test suites, ~40 individual test cases  
**Maintained By:** EmberViewer development team
