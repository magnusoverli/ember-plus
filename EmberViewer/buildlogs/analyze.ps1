function Get-TimeDiff {
    param([string]$start, [string]$end)
    $s = [DateTime]::ParseExact($start.Substring(0,19), 'yyyy-MM-ddTHH:mm:ss', $null)
    $e = [DateTime]::ParseExact($end.Substring(0,19), 'yyyy-MM-ddTHH:mm:ss', $null)
    return [math]::Round(($e - $s).TotalSeconds, 1)
}

Write-Host '=== BUILD TIMING COMPARISON ===' -ForegroundColor Cyan
Write-Host ''

# Run 1 Analysis
$r1 = Get-Content run1.log
$r1_start = ($r1 | Select-String '^2025-' | Select-Object -First 1).ToString()
$r1_checkout = ($r1 | Select-String 'Run actions/checkout' | Select-Object -First 1).ToString()
$r1_git = ($r1 | Select-String 'Run # Add Git to PATH' | Select-Object -First 1).ToString()
$r1_qt = ($r1 | Select-String 'Run jurplel/install-qt-action@v4' | Select-Object -First 1).ToString()
$r1_cmake = ($r1 | Select-String 'Run lukka/get-cmake' | Select-Object -First 1).ToString()
$r1_msvc = ($r1 | Select-String 'Run ilammy/msvc-dev-cmd' | Select-Object -First 1).ToString()
$r1_build = ($r1 | Select-String 'Run mkdir build' | Select-Object -First 1).ToString()
$r1_deploy = ($r1 | Select-String 'Run mkdir deploy' | Select-Object -First 1).ToString()
$r1_nsis_check = ($r1 | Select-String 'Run if.*NSIS' | Select-Object -First 1).ToString()
$r1_makensis = ($r1 | Select-String 'Run cd EmberViewer' | Select-Object -First 2 | Select-Object -Last 1).ToString()
$r1_end = ($r1 | Select-String '^2025-' | Select-Object -Last 1).ToString()

$r1_checkout_time = Get-TimeDiff $r1_checkout $r1_git
$r1_qt_time = Get-TimeDiff $r1_qt $r1_cmake
$r1_cmake_time = Get-TimeDiff $r1_cmake $r1_msvc
$r1_msvc_time = Get-TimeDiff $r1_msvc $r1_build
$r1_build_time = Get-TimeDiff $r1_build $r1_deploy
$r1_deploy_time = Get-TimeDiff $r1_deploy $r1_nsis_check
$r1_nsis_time = Get-TimeDiff $r1_nsis_check $r1_makensis
$r1_total_time = Get-TimeDiff $r1_start $r1_end

Write-Host 'RUN 1 (v0.4.9-8):' -ForegroundColor Yellow
Write-Host "  Checkout:    $r1_checkout_time sec"
Write-Host "  Qt Install:  $r1_qt_time sec"
Write-Host "  CMake Setup: $r1_cmake_time sec"
Write-Host "  MSVC Setup:  $r1_msvc_time sec"
Write-Host "  Build:       $r1_build_time sec"
Write-Host "  Deploy:      $r1_deploy_time sec"
Write-Host "  NSIS:        $r1_nsis_time sec"
Write-Host "  Total:       $r1_total_time sec" -ForegroundColor Yellow
Write-Host ''

# Run 2 Analysis
$r2 = Get-Content run2.log
$r2_start = ($r2 | Select-String '^2025-' | Select-Object -First 1).ToString()
$r2_checkout = ($r2 | Select-String 'Run actions/checkout' | Select-Object -First 1).ToString()
$r2_git = ($r2 | Select-String 'Run # Add Git to PATH' | Select-Object -First 1).ToString()
$r2_qt = ($r2 | Select-String 'Run jurplel/install-qt-action@v4' | Select-Object -First 1).ToString()
$r2_cmake = ($r2 | Select-String 'Run lukka/get-cmake' | Select-Object -First 1).ToString()
$r2_msvc = ($r2 | Select-String 'Run ilammy/msvc-dev-cmd' | Select-Object -First 1).ToString()
$r2_build = ($r2 | Select-String 'Run mkdir build' | Select-Object -First 1).ToString()
$r2_deploy = ($r2 | Select-String 'Run mkdir deploy' | Select-Object -First 1).ToString()
$r2_nsis_check = ($r2 | Select-String 'Run if.*NSIS' | Select-Object -First 1).ToString()
$r2_makensis = ($r2 | Select-String 'Run cd EmberViewer' | Select-Object -First 2 | Select-Object -Last 1).ToString()
$r2_end = ($r2 | Select-String '^2025-' | Select-Object -Last 1).ToString()

$r2_checkout_time = Get-TimeDiff $r2_checkout $r2_git
$r2_qt_time = Get-TimeDiff $r2_qt $r2_cmake
$r2_cmake_time = Get-TimeDiff $r2_cmake $r2_msvc
$r2_msvc_time = Get-TimeDiff $r2_msvc $r2_build
$r2_build_time = Get-TimeDiff $r2_build $r2_deploy
$r2_deploy_time = Get-TimeDiff $r2_deploy $r2_nsis_check
$r2_nsis_time = Get-TimeDiff $r2_nsis_check $r2_makensis
$r2_total_time = Get-TimeDiff $r2_start $r2_end

Write-Host 'RUN 2 (v0.4.9-10):' -ForegroundColor Green
Write-Host "  Checkout:    $r2_checkout_time sec"
Write-Host "  Qt Install:  $r2_qt_time sec"
Write-Host "  CMake Setup: $r2_cmake_time sec"
Write-Host "  MSVC Setup:  $r2_msvc_time sec"
Write-Host "  Build:       $r2_build_time sec"
Write-Host "  Deploy:      $r2_deploy_time sec"
Write-Host "  NSIS:        $r2_nsis_time sec"
Write-Host "  Total:       $r2_total_time sec" -ForegroundColor Green
Write-Host ''

# Comparison
Write-Host 'DIFFERENCE (Run2 - Run1):' -ForegroundColor Magenta
Write-Host "  Checkout:    $([math]::Round($r2_checkout_time - $r1_checkout_time, 1)) sec"
Write-Host "  Qt Install:  $([math]::Round($r2_qt_time - $r1_qt_time, 1)) sec"
Write-Host "  CMake Setup: $([math]::Round($r2_cmake_time - $r1_cmake_time, 1)) sec"
Write-Host "  MSVC Setup:  $([math]::Round($r2_msvc_time - $r1_msvc_time, 1)) sec"
Write-Host "  Build:       $([math]::Round($r2_build_time - $r1_build_time, 1)) sec"
Write-Host "  Deploy:      $([math]::Round($r2_deploy_time - $r1_deploy_time, 1)) sec"
Write-Host "  NSIS:        $([math]::Round($r2_nsis_time - $r1_nsis_time, 1)) sec"
Write-Host "  Total:       $([math]::Round($r2_total_time - $r1_total_time, 1)) sec" -ForegroundColor Magenta

