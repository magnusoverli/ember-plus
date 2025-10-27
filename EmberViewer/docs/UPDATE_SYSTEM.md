# EmberViewer Update System - Implementation Plan

## Overview

Cross-platform automatic update system using GitHub Releases as the distribution mechanism. The system provides a unified user experience across Linux, Windows, and macOS with platform-specific backend implementations.

## Design Decisions

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| **Installation Behavior** | Auto-restart with prompt | Standard UX, applies updates immediately |
| **Update Channels** | Stable releases only | Simplicity, users get official releases |
| **Check Frequency** | Every launch | Maximum freshness, simple implementation |
| **Notifications** | Status bar → dialog on click | Non-intrusive but discoverable |
| **Skip Versions** | Yes | User control over unwanted updates |
| **First Launch Check** | Yes | Immediate update discovery |
| **Checksum Verification** | No | Trust GitHub infrastructure |
| **Rollback/Backup** | No | Users re-download if needed |

## Architecture

```
┌─────────────────────────────────────┐
│      MainWindow (Qt UI)             │
│  - Menu: Help → Check for Updates   │
│  - Status bar notifications         │
│  - Update dialogs                   │
└──────────────┬──────────────────────┘
               │
┌──────────────▼──────────────────────┐
│    UpdateManager (Abstract)         │
│  - checkForUpdates()                │
│  - startUpdate()                    │
│  - Version comparison               │
│  - Settings persistence             │
└──────────────┬──────────────────────┘
               │
       ┌───────┴────────┬──────────────┐
       │                │              │
┌──────▼─────┐  ┌──────▼─────┐  ┌────▼─────┐
│ Linux      │  │ Windows    │  │ macOS    │
│ GitHub API │  │ WinSparkle │  │ Sparkle  │
│ or         │  │ or         │  │ or       │
│ AppImage   │  │ GitHub API │  │ GitHub   │
│ Update     │  │            │  │ API      │
└────────────┘  └────────────┘  └──────────┘
```

## File Structure

```
EmberViewer/
├── include/
│   ├── UpdateManager.h         # Abstract base class
│   ├── UpdateDialog.h          # Shared update notification UI
│   ├── LinuxUpdateManager.h    # Linux implementation
│   ├── WindowsUpdateManager.h  # Windows implementation
│   └── MacUpdateManager.h      # macOS implementation
├── src/
│   ├── UpdateManager.cpp
│   ├── UpdateDialog.cpp
│   ├── LinuxUpdateManager.cpp
│   ├── WindowsUpdateManager.cpp
│   └── MacUpdateManager.cpp
└── CMakeLists.txt              # Build configuration updates
```

## Component Specifications

### 1. UpdateManager (Base Class)

**Responsibility:** Platform-agnostic update logic, settings management, version comparison.

**Key Data Structures:**
- `UpdateInfo`: Contains version, release notes, download URL, file size, availability flag
- `m_settings`: QSettings instance for persistence

**Required Methods:**
- `checkForUpdates()`: Pure virtual - trigger update check
- `startUpdate()`: Pure virtual - begin update installation
- `isUpdateSupported()`: Pure virtual - returns true if updates work on this platform
- `shouldCheckOnLaunch()`: Check if update check should run (handles skipped versions, first launch)
- `compareVersions(v1, v2)`: Semantic version comparison (0.4.0 > 0.3.2)
- `setAutoCheckEnabled(bool)`: Enable/disable auto-checking
- `isAutoCheckEnabled()`: Get auto-check state
- `create(parent)`: Static factory method that returns platform-specific instance

**Signals:**
- `updateAvailable(UpdateInfo)`: New version found
- `updateNotAvailable()`: Already on latest version
- `checkingForUpdates()`: Check started (optional for UI feedback)
- `updateProgress(int, QString)`: Download/install progress
- `updateError(QString)`: Something went wrong
- `updateComplete()`: Update finished successfully

**Settings Keys:**
- `Updates/CheckOnLaunch`: bool (always true in this implementation)
- `Updates/LastCheckTime`: QDateTime (for future rate limiting if needed)
- `Updates/SkippedVersion`: QString (version user chose to skip)

**Version Comparison Algorithm:**
Split versions by '.', compare each segment as integers left-to-right. First difference determines result.

---

### 2. UpdateDialog (Shared UI)

**Responsibility:** Display update information and prompt user for action.

**UI Elements:**
- Title label: "Update Available"
- Version label: "Version X.Y.Z is now available (you have A.B.C)"
- Release notes: QTextEdit (read-only, scrollable, supports Markdown/HTML from GitHub)
- Progress bar: Shows download progress (hidden initially)
- Status label: Text feedback during download
- Buttons:
  - "Update Now": Triggers update
  - "Remind Me Later": Close dialog
  - "Skip This Version": Save version to skip list, close dialog

**Methods:**
- `setUpdateInfo(UpdateInfo)`: Populate dialog with version details
- `setProgress(int, QString)`: Update progress bar and status
- `setError(QString)`: Display error state

**Signals:**
- `updateRequested()`: User clicked "Update Now"
- `updateCanceled()`: User closed dialog
- `versionSkipped(QString)`: User clicked "Skip This Version"

**Behavior:**
- Dialog is modal
- Release notes rendered as rich text (HTML from GitHub API)
- Progress bar only visible during download
- Buttons disabled during download

---

### 3. LinuxUpdateManager

**Responsibility:** Handle updates on Linux systems.

**Strategy:** Use GitHub Releases API to check versions, then either:
- Option A: Download full AppImage and replace current file
- Option B: Use AppImageUpdate library (libappimageupdate) for delta updates

**Implementation Details:**

**Update Check Flow:**
1. GET `https://api.github.com/repos/magnusoverli/ember-plus/releases/latest`
2. Parse JSON response for `tag_name` (e.g., "v0.4.0")
3. Compare with `QApplication::applicationVersion()`
4. Extract release notes from `body` field
5. Find AppImage asset by matching pattern: `*x86_64.AppImage` (not `.zsync`)
6. Emit `updateAvailable()` with download URL from `browser_download_url`

**Update Installation Flow:**
1. Download AppImage to temp directory
2. Make executable (`chmod +x`)
3. Determine current AppImage path via `$APPIMAGE` env var or `QCoreApplication::applicationFilePath()`
4. Atomically replace: `mv new.AppImage current.AppImage`
5. Exec new AppImage and exit current process

**Platform Detection:**
- Check if running from AppImage: `qgetenv("APPIMAGE")` is set
- If not AppImage: Disable updates (`isUpdateSupported()` returns false)

**Network:**
- Use `QNetworkAccessManager` for HTTP requests
- Set User-Agent header: `EmberViewer/X.Y.Z`
- Handle network errors gracefully

**Alternative: AppImageUpdate Integration:**
If libappimageupdate is available, use its API directly for delta updates. This requires:
- Linking against libappimageupdate
- Calling updater check/start methods
- The AppImage must have update information embedded (see GitHub Actions section)

---

### 4. WindowsUpdateManager

**Responsibility:** Handle updates on Windows systems.

**Strategy:** Option A (Simple): Use GitHub API like Linux. Option B (Advanced): Integrate WinSparkle library.

**Recommended: GitHub API Approach (Simpler)**

**Update Check Flow:**
1. Same as Linux: Query GitHub releases API
2. Find Windows installer asset: `*-Setup.exe` or `*-win64.exe`
3. Parse version from tag

**Update Installation Flow:**
1. Download installer to `%TEMP%\EmberViewer-Update.exe`
2. Show final confirmation: "EmberViewer will close to install the update"
3. Execute installer with elevated privileges if needed
4. Exit application
5. Installer runs, replaces program files
6. Installer optionally relaunches EmberViewer

**Platform-Specific Considerations:**
- May require UAC elevation (installer handles this)
- Use `QProcess::startDetached()` to launch installer
- Installer should have `/SILENT` or `/VERYSILENT` flag support (NSIS)

**WinSparkle Alternative:**
If using WinSparkle, initialize it in the manager constructor with:
- App name
- App version
- Appcast URL (custom XML feed, see Infrastructure section)

WinSparkle handles everything automatically, but requires hosting an appcast.xml file.

---

### 5. MacUpdateManager

**Responsibility:** Handle updates on macOS systems.

**Strategy:** Option A: GitHub API (like Linux/Windows). Option B: Sparkle framework.

**Update Check Flow:**
Same as Linux/Windows, but find macOS asset: `*.dmg` or `*.pkg`

**Update Installation Flow:**
1. Download DMG to temp directory
2. Mount DMG programmatically
3. Copy .app bundle to /Applications (may need authorization)
4. Unmount DMG
5. Relaunch application from new location

**Sparkle Alternative:**
Use Sparkle framework (native macOS solution). Requires:
- Appcast XML feed
- Code signing for automatic updates
- Sparkle framework bundled in app

---

### 6. MainWindow Integration

**Responsibility:** UI entry points for update system.

**Changes Required:**

**Constructor:**
- Create UpdateManager instance via factory method
- Connect signals (updateAvailable, updateNotAvailable, updateError)
- Trigger immediate check on first launch
- Schedule check on every subsequent launch (2-second delay to allow UI to settle)

**Menu Setup:**
- Add "Help" menu if not present
- Add "Check for Updates..." action
- Connect to `onCheckForUpdates()` slot
- Only show if `updateManager->isUpdateSupported()` returns true

**Status Bar Integration:**
- On `updateAvailable()` signal:
  - Add clickable label to status bar: "⬇ Update to vX.Y.Z available"
  - Use QLabel with rich text and link
  - Connect link click to show UpdateDialog

**Slots:**
- `onCheckForUpdates()`: Manual check triggered from menu
- `onUpdateAvailable(UpdateInfo)`: Show status bar notification
- `onUpdateNotAvailable()`: Show message box "You're up to date"
- `onUpdateError(QString)`: Show warning dialog

**UpdateDialog Handling:**
- Create dialog on demand (don't keep in memory)
- Connect `updateRequested()` to `updateManager->startUpdate()`
- Connect `versionSkipped()` to save skipped version in settings

---

## Version Comparison Algorithm

**Input:** Two version strings (e.g., "0.4.0" and "0.3.2")

**Process:**
1. Remove "v" prefix if present
2. Split by "." delimiter
3. Pad shorter version with zeros
4. Compare each segment left-to-right as integers
5. Return: 1 if v1 > v2, -1 if v1 < v2, 0 if equal

**Example:**
```
compareVersions("0.4.0", "0.3.2"):
  - Split: [0,4,0] vs [0,3,2]
  - Compare: 0==0, 4>3 → return 1 (v1 is newer)
  
compareVersions("1.0.0", "0.9.9"):
  - Split: [1,0,0] vs [0,9,9]
  - Compare: 1>0 → return 1
```

---

## Settings Persistence

**Storage:** QSettings (platform-specific locations)
- Linux: `~/.config/Magnus Overli/EmberViewer.conf`
- Windows: Registry `HKEY_CURRENT_USER\Software\Magnus Overli\EmberViewer`
- macOS: `~/Library/Preferences/com.magnusoverli.emberviewer.plist`

**Keys:**
- `Updates/CheckOnLaunch`: Always true (hardcoded in this design)
- `Updates/LastCheckTime`: QDateTime of last check (for future enhancements)
- `Updates/SkippedVersion`: QString of version user wants to skip
- `App/FirstLaunch`: bool to track if this is first run (for future use)

**Settings Initialization:**
Set application metadata in main():
```
QApplication::setApplicationName("EmberViewer");
QApplication::setOrganizationName("Magnus Overli");
QApplication::setOrganizationDomain("github.com/magnusoverli");
```

This is already done in the current codebase.

---

## GitHub API Integration

### Endpoint

`GET https://api.github.com/repos/magnusoverli/ember-plus/releases/latest`

### Request Headers

- `User-Agent: EmberViewer/X.Y.Z` (required by GitHub)
- `Accept: application/vnd.github+json` (optional, but recommended)

### Response Format

Relevant JSON fields:
```json
{
  "tag_name": "v0.4.0",
  "name": "Version 0.4.0",
  "body": "# Release Notes\n\n- Fixed bugs\n- Added features",
  "draft": false,
  "prerelease": false,
  "assets": [
    {
      "name": "EmberViewer-v0.4.0-x86_64.AppImage",
      "size": 15728640,
      "browser_download_url": "https://github.com/.../EmberViewer-v0.4.0-x86_64.AppImage"
    },
    {
      "name": "EmberViewer-v0.4.0-Setup.exe",
      "size": 20971520,
      "browser_download_url": "https://github.com/.../EmberViewer-v0.4.0-Setup.exe"
    }
  ]
}
```

### Filtering Assets

**Linux:** Find asset matching `*x86_64.AppImage` (exclude `*.zsync`)

**Windows:** Find asset matching `*Setup.exe` or `*win64.exe`

**macOS:** Find asset matching `*.dmg` or `*.app.zip`

Use `QSysInfo::currentCpuArchitecture()` if supporting ARM64 (e.g., Apple Silicon).

### Rate Limiting

GitHub allows 60 requests/hour for unauthenticated requests. This is sufficient for:
- Check on every launch
- Typical user launches app 1-5 times per day
- No rate limiting mitigation needed

If rate limits become an issue (unlikely):
- Cache last check result for 1 hour
- Include GitHub token (not recommended for security)

### Error Handling

Handle these scenarios:
- Network unreachable: Silent failure, try next launch
- HTTP 404: Repository or release not found (should never happen)
- HTTP 403: Rate limited (extremely rare with current frequency)
- Invalid JSON: Malformed response
- No matching asset: Platform not supported for this release

Always fail gracefully without blocking app startup.

---

## Build System Changes (CMakeLists.txt)

### Add Update Source Files

Create conditional compilation based on platform:

```cmake
# Update system sources (always compiled)
set(UPDATE_SOURCES
    src/UpdateManager.cpp
    src/UpdateDialog.cpp
    include/UpdateManager.h
    include/UpdateDialog.h
)

# Platform-specific implementations
if(UNIX AND NOT APPLE)
    list(APPEND UPDATE_SOURCES
        src/LinuxUpdateManager.cpp
        include/LinuxUpdateManager.h
    )
endif()

if(WIN32)
    list(APPEND UPDATE_SOURCES
        src/WindowsUpdateManager.cpp
        include/WindowsUpdateManager.h
    )
endif()

if(APPLE)
    list(APPEND UPDATE_SOURCES
        src/MacUpdateManager.mm  # Note: .mm for Objective-C++
        include/MacUpdateManager.h
    )
endif()

target_sources(${PROJECT_NAME} PRIVATE ${UPDATE_SOURCES})
```

### Qt Network Module

Ensure Qt Network is linked:
```cmake
find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Network)
```

This is already present in the current CMakeLists.txt.

### Optional: External Libraries

**For AppImageUpdate (Linux):**
```cmake
if(UNIX AND NOT APPLE)
    find_package(libappimageupdate QUIET)
    if(libappimageupdate_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE libappimageupdate)
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_LIBAPPIMAGEUPDATE)
    endif()
endif()
```

**For WinSparkle (Windows):**
```cmake
if(WIN32)
    # Add WinSparkle library path
    # target_link_libraries(${PROJECT_NAME} PRIVATE winsparkle)
endif()
```

**For Sparkle (macOS):**
```cmake
if(APPLE)
    find_library(SPARKLE_FRAMEWORK Sparkle)
    if(SPARKLE_FRAMEWORK)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${SPARKLE_FRAMEWORK})
    endif()
endif()
```

**Note:** External libraries are optional. The GitHub API approach works without them.

---

## Infrastructure: GitHub Actions

### Current State

The repository already has:
- `.github/workflows/release.yml`: Builds installers on git tags
- `.github/workflows/ci.yml`: CI builds

### Required Changes

**For Linux AppImage:**

If using AppImageUpdate with embedded update info, add to the AppImage build step:

```yaml
- name: Embed update information
  run: |
    # Update information embedded in AppImage
    # Format: gh-releases-zsync|user|repo|latest|pattern
    echo "UPDATE_INFORMATION=gh-releases-zsync|magnusoverli|ember-plus|latest|EmberViewer-*-x86_64.AppImage.zsync" > update-info.txt
```

This is passed to linuxdeployqt or appimagetool during build.

**Generate .zsync files:**

```yaml
- name: Generate zsync file
  run: |
    sudo apt-get install -y zsync
    zsyncmake -u "EmberViewer-${{ github.ref_name }}-x86_64.AppImage" \
              EmberViewer-${{ github.ref_name }}-x86_64.AppImage
```

Upload .zsync files alongside AppImages in release assets.

**For Windows:**

No changes needed if using GitHub API approach. The existing NSIS installer works as-is.

**For macOS:**

Future implementation - similar to Linux/Windows.

### Release Checklist

When creating a new release:
1. Tag commit: `git tag v0.4.0 && git push origin v0.4.0`
2. GitHub Actions builds all platforms
3. Release is created automatically with assets:
   - `EmberViewer-v0.4.0-x86_64.AppImage`
   - `EmberViewer-v0.4.0-x86_64.AppImage.zsync` (if using AppImageUpdate)
   - `EmberViewer-v0.4.0-Setup.exe`
4. Release notes added (manually or automated)
5. Users receive update notifications on next launch

---

## User Experience Flow

### First Launch
1. User downloads and runs EmberViewer v0.3.2
2. App starts normally
3. After 2 seconds: Background update check begins
4. If update found (v0.4.0): Status bar shows "⬇ Update to v0.4.0 available"
5. User clicks status bar link → UpdateDialog opens
6. User reviews release notes

### User Actions in Dialog

**Option 1: Update Now**
1. "Update Now" button clicked
2. Progress bar appears
3. Download happens (with progress updates)
4. Dialog shows: "Update ready! EmberViewer will restart."
5. App closes, update installs, app relaunches

**Option 2: Remind Me Later**
1. Dialog closes
2. Status bar notification persists
3. On next launch: Update check runs again, notification reappears

**Option 3: Skip This Version**
1. "Skip This Version" button clicked
2. v0.4.0 saved to skipped versions list
3. Dialog closes, status bar clears
4. v0.4.0 never shown again
5. v0.4.1 (when released) will trigger new notification

### Manual Check

1. User opens Help → Check for Updates
2. Immediate check runs (even if recently checked)
3. If up-to-date: Message box "You are running the latest version (v0.3.2)"
4. If outdated: Same UpdateDialog flow as above

### Subsequent Launches

1. Every time app starts: Update check runs (2-second delay)
2. If skipped version: No notification
3. If new version (different from skipped): Show notification
4. Status bar notification is non-modal, doesn't interrupt work

---

## Error Scenarios

### Network Unavailable
- Silent failure
- No user notification
- Try again on next launch
- Menu item still works (shows error if manually triggered)

### GitHub API Error (403, 404, 500)
- Log warning to console
- If manual check: Show error dialog
- If auto-check: Silent failure

### Download Failure
- Show error dialog: "Failed to download update. Please try again later."
- Offer manual download link to GitHub releases page

### Installation Failure
- Show error dialog: "Failed to install update."
- Suggest manual download
- Log error details for debugging

### Corrupted Download
- Not applicable (no checksum verification in this design)
- Trust GitHub CDN integrity

---

## Testing Strategy

### Unit Tests

**UpdateManager:**
- Version comparison: Test all cases (greater, less, equal, edge cases)
- Settings persistence: Read/write skipped versions
- Factory method: Returns correct platform instance

**Platform Managers:**
- Mock GitHub API responses
- Test JSON parsing
- Test asset filtering (find correct platform file)

### Integration Tests

**Update Check:**
1. Mock GitHub API with test response
2. Trigger check
3. Verify `updateAvailable()` signal emitted
4. Verify UpdateInfo populated correctly

**Update Flow (Linux):**
1. Create fake AppImage in temp directory
2. Mock download to return test file
3. Trigger update
4. Verify file replacement (without actually restarting)

### Manual Testing

**Happy Path:**
1. Run old version
2. Create new release on GitHub
3. Launch app
4. Verify status bar notification
5. Click → verify dialog content
6. Click "Update Now" → verify download → verify restart

**Skip Version:**
1. Get update notification
2. Click "Skip This Version"
3. Restart app
4. Verify no notification for same version
5. Create newer release
6. Verify notification appears for new version

**Error Cases:**
1. Disconnect network → verify silent failure
2. Delete GitHub release → verify graceful handling
3. Manually trigger check with no network → verify error message

---

## Implementation Phases

### Phase 1: Core Foundation (1-2 days)
- Create UpdateManager base class
- Implement version comparison
- Implement settings persistence
- Create UpdateDialog UI
- Add factory method with platform detection

### Phase 2: Linux Implementation (2-3 days)
- Implement LinuxUpdateManager
- GitHub API integration
- Download logic
- AppImage replacement logic
- Test with mock releases

### Phase 3: MainWindow Integration (1 day)
- Add menu item
- Add status bar notification
- Connect signals
- Add launch check timer
- Test full flow

### Phase 4: Windows Implementation (2-3 days)
- Implement WindowsUpdateManager
- Adapt Linux logic for Windows
- Test installer execution
- Handle UAC/elevation

### Phase 5: macOS Implementation (2-3 days)
- Implement MacUpdateManager
- DMG handling
- Test on macOS

### Phase 6: GitHub Actions (1 day)
- Add zsync generation (Linux)
- Verify release asset naming
- Test end-to-end with real release

### Phase 7: Polish & Testing (1-2 days)
- Error handling improvements
- UI polish
- Comprehensive testing
- Documentation updates

**Total Estimate:** 10-16 days for complete implementation across all platforms.

---

## Security Considerations

### Trust Model
This design trusts:
- GitHub's infrastructure (HTTPS, CDN)
- Release assets are not tampered with
- No checksum verification (accepted trade-off)

### Mitigation
- Always use HTTPS for API and downloads (Qt handles this)
- Verify SSL certificates (QNetworkAccessManager default behavior)
- Only download from `github.com` domain

### Future Enhancements
If security becomes a concern:
- Add SHA256 checksum verification
- Add GPG signature verification (AppImages)
- Code signing (Windows/macOS installers)

---

## Future Enhancements

### Not in Initial Implementation

**Pre-release Channel:**
- Settings option: "Include pre-release versions"
- Query `/releases` instead of `/releases/latest`
- Filter `prerelease: false` or allow both

**Configurable Check Frequency:**
- Settings: Daily / Weekly / Manual only
- Implement rate limiting logic in `shouldCheckOnLaunch()`

**Background Download:**
- Download updates in background without user interaction
- Prompt only when ready to install

**Delta Updates (Linux):**
- Full AppImageUpdate integration
- Requires libappimageupdate dependency
- Significantly reduces bandwidth

**Update Statistics:**
- Track update success/failure rates
- Anonymous telemetry for update reliability

**Release Channels:**
- Stable / Beta / Nightly
- Different GitHub repos or branches

---

## Troubleshooting

### "Check for Updates" Menu Not Visible
- Verify platform detection: `updateManager->isUpdateSupported()` returns true
- Linux: Ensure running from AppImage (`$APPIMAGE` env var set)
- Check compile flags: Platform-specific code compiled correctly

### Update Check Fails Silently
- Check network connectivity
- Verify GitHub API is reachable: `curl https://api.github.com/repos/magnusoverli/ember-plus/releases/latest`
- Check Qt Network module linked correctly
- Review console logs for error messages

### Update Downloads But Doesn't Install
- Linux: Check file permissions on AppImage and target directory
- Windows: Check UAC settings, installer may need elevation
- Verify download completed (check file size)

### Version Comparison Wrong
- Ensure version format is consistent: `X.Y.Z` (no "v" prefix in app version)
- Check `QApplication::applicationVersion()` returns expected value
- Test `compareVersions()` with unit tests

---

## References

### External Documentation
- GitHub Releases API: https://docs.github.com/en/rest/releases/releases
- AppImageUpdate: https://github.com/AppImageCommunity/AppImageUpdate
- WinSparkle: https://winsparkle.org/
- Sparkle (macOS): https://sparkle-project.org/
- Qt Network Module: https://doc.qt.io/qt-5/qtnetwork-index.html
- Semantic Versioning: https://semver.org/

### Repository Files
- Current release workflow: `.github/workflows/release.yml`
- Current CI workflow: `.github/workflows/ci.yml`
- Version header template: `EmberViewer/include/version.h.in`
- CMake configuration: `EmberViewer/CMakeLists.txt`

---

## Appendix: Code Patterns

While implementation details are left to the developer, here are recommended patterns:

### Factory Pattern for Platform Detection
```
UpdateManager* UpdateManager::create(QObject *parent)
{
    #if defined(Q_OS_LINUX)
        return new LinuxUpdateManager(parent);
    #elif defined(Q_OS_WIN)
        return new WindowsUpdateManager(parent);
    #elif defined(Q_OS_MAC)
        return new MacUpdateManager(parent);
    #else
        return nullptr; // Unsupported platform
    #endif
}
```

### Signal-Based Async Operations
All network operations and update processes should be asynchronous using Qt signals/slots. Never block the main thread.

### Error Handling Strategy
- Fail gracefully: Update system should never crash the application
- Log errors for debugging but don't spam user with technical details
- Provide actionable solutions: "Check your internet connection" or "Download manually from..."

### Resource Cleanup
Use RAII and Qt parent-child ownership for automatic cleanup:
- QNetworkAccessManager as member variable
- QNetworkReply deleted in slots with `reply->deleteLater()`
- Dialogs parented to MainWindow

---

## Questions for Developer

Before implementation, consider:

1. **Desktop file integration (Linux):** Should update mechanism integrate with desktop environments (e.g., show in system notifications)?

2. **Automatic vs Manual Download:** Should download start automatically when user clicks "Update Now", or show confirmation first?

3. **Logging verbosity:** How detailed should update process logging be? (qDebug, qInfo, qWarning levels)

4. **Network timeouts:** What timeout for GitHub API requests? (Recommended: 10 seconds)

5. **UI customization:** Should UpdateDialog match EmberViewer's theme/style, or use default Qt styling?

6. **Installer flags (Windows):** Should installer run silently (`/S` flag) or show installation UI?

These can be decided during implementation based on testing and user feedback.

---

**End of Implementation Plan**
