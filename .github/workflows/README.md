# GitHub Workflows for ember-plus

This directory contains GitHub Actions workflows for automated building and releasing.

## Workflows

### 1. CI Build (`ci.yml`)

**Triggers:** Push to main/develop/feature branches, Pull Requests

**Purpose:** Continuous Integration - builds and tests on every commit

**What it does:**
- Builds EmberViewer in both Debug and Release modes
- Runs on Ubuntu latest
- Caches Qt to speed up builds
- Uploads Release build as artifact (kept for 7 days)

**Build matrix:**
- Debug build (for development)
- Release build (optimized)

**Future expansion:**
- Uncomment `build-macos` job for macOS builds
- Uncomment `build-windows` job for Windows builds

---

### 2. Release Build (`release.yml`)

**Triggers:** 
- Version tags (e.g., `v1.2.0`)
- Manual workflow dispatch

**Purpose:** Creates distributable AppImage for Linux

**What it does:**
1. Builds EmberViewer in Release mode
2. Uses `linuxdeploy` to bundle Qt dependencies
3. Creates self-contained AppImage
4. Uploads to GitHub Releases with auto-generated notes
5. Keeps artifact for 30 days

**Output:** `EmberViewer-v1.2.0-x86_64.AppImage`

**Future expansion:**
- Uncomment `release-macos` for .dmg creation
- Uncomment `release-windows` for installer creation

---

## Usage

### For CI Builds

Just push your code! The workflow runs automatically.

```bash
git push origin feature/my-feature
```

Check the "Actions" tab on GitHub to see build status.

---

### For Releases

Create and push a version tag:

```bash
# Create release
git tag -a v1.2.0 -m "Release v1.2.0: Matrix support"
git push origin v1.2.0
```

Or trigger manually from GitHub Actions UI.

The workflow will:
1. Build the AppImage
2. Create a GitHub Release
3. Attach the AppImage to the release
4. Generate release notes from commits

---

## Testing Locally

### Test the build steps:

```bash
# Simulate CI build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja
ninja EmberViewer
```

### Test AppImage creation:

```bash
# Download linuxdeploy
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x *.AppImage

# Create AppDir
mkdir -p AppDir/usr/bin
cp build/EmberViewer/EmberViewer AppDir/usr/bin/

# Create desktop file
cat > AppDir/usr/share/applications/emberviewer.desktop << 'EOF'
[Desktop Entry]
Type=Application
Name=EmberViewer
Exec=EmberViewer
Icon=emberviewer
Categories=Network;
EOF

# Build AppImage
export QMAKE=/usr/lib/qt5/bin/qmake
./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --output appimage
```

---

## Cross-Platform Preparation

The workflows have **commented placeholders** for macOS and Windows builds.

### To enable macOS:
1. Uncomment `build-macos` job in `ci.yml`
2. Uncomment `release-macos` job in `release.yml`
3. Add macOS-specific build steps (Qt installation, macdeployqt, DMG creation)

### To enable Windows:
1. Uncomment `build-windows` job in `ci.yml`
2. Uncomment `release-windows` job in `release.yml`
3. Add Windows-specific build steps (Qt installation, windeployqt, NSIS installer)

---

## Requirements

### For CI (automatic):
- Nothing! GitHub Actions provides everything

### For AppImage (automatic):
- `linuxdeploy` - downloaded by workflow
- `linuxdeploy-plugin-qt` - downloaded by workflow
- Qt5 - installed by workflow

### For local testing:
- CMake 3.10+
- Qt5 (qtbase5-dev)
- Ninja or Make
- GCC/Clang

---

## Troubleshooting

### Build fails on dependencies
Check that all Qt packages are installed. The workflow installs:
- qt5-default
- qtbase5-dev
- qtbase5-dev-tools

### AppImage creation fails
- Check that binary was built successfully
- Verify Qt plugin path is correct
- Check `QMAKE` environment variable

### Release not created
- Ensure you pushed a tag starting with 'v' (e.g., v1.2.0)
- Check GITHUB_TOKEN permissions
- Verify tag format matches pattern in workflow

---

## Future Improvements

- [ ] Add automated testing (when tests are added to project)
- [ ] Add code coverage reporting
- [ ] Add static analysis (clang-tidy)
- [ ] Expand to macOS (.dmg)
- [ ] Expand to Windows (NSIS installer)
- [ ] Add Flatpak/Snap packaging
- [ ] Add checksums to releases
- [ ] Code signing for macOS/Windows

