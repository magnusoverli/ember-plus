# Build Performance Analysis & Optimization Recommendations

## Current Build Times Comparison

### Run 1 (v0.4.9-8): 748 seconds (~12.5 minutes)
- Checkout: 31 sec
- Qt Install: 304 sec (5 min)
- CMake Setup: 211 sec (3.5 min) ⚠️
- MSVC Setup: 3 sec
- Build: 149 sec (2.5 min) ⚠️
- Deploy: 2 sec
- NSIS: 15 sec

### Run 2 (v0.4.9-10): 726 seconds (~12 minutes)
- Checkout: 34 sec
- Qt Install: 255 sec (4.2 min) - **49 sec improvement from caching!**
- CMake Setup: 220 sec (3.7 min)
- MSVC Setup: 3 sec
- Build: 171 sec (2.8 min)
- Deploy: 3 sec
- NSIS: 13 sec

### Linux Build: ~120 seconds (~2 minutes)
Much faster due to parallel compilation!

---

## Critical Performance Issues Identified

### 1. ❌ **NO PARALLEL COMPILATION ON WINDOWS** (Biggest Issue!)
**Impact**: ~100-120 seconds potential savings

**Problem**: 
- Windows build uses MSBuild without specifying parallel jobs
- Line 61 in workflow: `cmake --build . --config Release`
- Should be: `cmake --build . --config Release -- /maxcpucount`

**Linux comparison**: Uses `-j$(nproc)` which enables all CPU cores
**Expected improvement**: 50-70% faster builds (149s → ~60-75s)

### 2. ⚠️ **CMAKE DOWNLOAD & EXTRACTION TAKES 3.5 MINUTES**
**Impact**: ~210 seconds

**Problems**:
- CMake ZIP extraction is extremely slow (3.5 minutes to extract!)
- Cloud cache miss on every run
- Downloads CMake and Ninja every time

**Solutions**:
- **Option A**: Install CMake permanently on the self-hosted runner (RECOMMENDED)
  - One-time installation
  - Always available, no download/extraction needed
  - Saves 3.5 minutes every build
  
- **Option B**: Keep using the action but optimize cache
  - Cache is not working reliably on self-hosted runner
  - Still requires download/extraction time on cache miss

### 3. ⚠️ **QT DOWNLOAD STILL TAKES 4+ MINUTES**
**Impact**: ~255 seconds

**Progress**: Cache enabled showed 49 second improvement (304s → 255s)

**Further optimization**:
- Qt cache should eventually reduce this to ~10-20 seconds
- First run with cache: still downloads (current state)
- Subsequent runs: should be cached
- Monitor next build to see if cache hits

### 4. ⚠️ **SLOW ARCHIVE DOWNLOAD (CHECKOUT)**
**Impact**: ~34 seconds

**Problem**: Using GitHub REST API to download archive instead of Git
**Log evidence**: "The repository will be downloaded using the GitHub REST API"

**Solution**: Ensure Git is properly installed and in PATH before checkout

---

## Optimization Recommendations (Priority Order)

### Priority 1: Enable Parallel Compilation ⭐⭐⭐
**Expected time savings**: 70-100 seconds
**Difficulty**: Very Easy (1-line change)
**Change**: Add `/maxcpucount` flag to Windows build command

```yaml
# Current (slow):
cmake --build . --config Release

# Optimized (fast):
cmake --build . --config Release -- /maxcpucount
```

### Priority 2: Install CMake Permanently on Self-Hosted Runner ⭐⭐⭐
**Expected time savings**: 210 seconds  
**Difficulty**: Easy (one-time manual installation)
**Steps**:
1. On the Windows runner machine, install CMake using Chocolatey:
   ```powershell
   choco install cmake -y
   ```
2. Remove the `lukka/get-cmake` action from workflow
3. Add a simple check to verify CMake is available

### Priority 3: Fix Git PATH Issue ⭐⭐
**Expected time savings**: 10-15 seconds
**Difficulty**: Easy
**Current**: Downloads archive via API, then tries to add Git to PATH
**Better**: Install Git permanently or ensure it's in PATH before checkout

### Priority 4: Monitor Qt Caching ⭐
**Expected time savings**: 235+ seconds (on cache hit)
**Difficulty**: Already implemented, just monitor
**Status**: Cache enabled but first runs will miss. Should improve on next build.

---

## Projected Build Time After All Optimizations

| Phase | Current | After Optimization | Savings |
|-------|---------|-------------------|---------|
| Checkout | 34s | 20s | 14s |
| Qt Install | 255s | 20s (cached) | 235s |
| CMake Setup | 220s | 5s (local install) | 215s |
| MSVC Setup | 3s | 3s | 0s |
| **Build** | **171s** | **70s** | **101s** |
| Deploy | 3s | 3s | 0s |
| NSIS | 13s | 13s | 0s |
| **TOTAL** | **726s (12 min)** | **134s (2.2 min)** | **592s (~10 min saved!)** |

## Comparison with Linux Build
- **Current Windows**: 12 minutes
- **Optimized Windows**: 2.2 minutes
- **Current Linux**: 2 minutes
- **Result**: Windows and Linux builds will be comparable!

---

## Implementation Plan

### Immediate (Can do now):
1. ✅ **Add parallel compilation flag** - 1 line change in workflow
2. ✅ **Install CMake on runner** - Run one command on the runner machine
3. ✅ **Remove get-cmake action** - Simplify workflow

### Monitor:
- Qt caching (should improve on next run automatically)
- Git checkout performance (may need Git installed permanently)

### Notes:
- Self-hosted runners benefit MORE from permanent installations vs actions
- Actions are great for GitHub-hosted runners (ephemeral)
- For self-hosted runners, permanent installation + checks is more efficient

