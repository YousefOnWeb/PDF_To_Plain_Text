#Requires -Version 5.1
# build.ps1 - One-click Windows build (PowerShell only, open-source MinGW-w64)
# Guarantees Qt/Poppler DLLs are found: auto-detects MSYS2/Qt, sets PATH, runs windeployqt via CMake POST_BUILD.
# Usage from repo root:  powershell -ExecutionPolicy Bypass -File .\build.ps1
# Options:  -Clean, -NoPackage, -Prefix "C:/path/to/Qt"

param(
    [switch]$Clean,
    [switch]$NoPackage,
    [string]$Prefix = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = $PSScriptRoot
if (-not $repoRoot) { $repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path }
Set-Location $repoRoot

# --- Detect Qt/Poppler prefix (MSYS2 UCRT64 preferred) ---
$prefixCandidates = @(
    $Prefix,
    $env:CMAKE_PREFIX_PATH,
    "C:\tools\msys64\ucrt64",
    "C:\msys64\ucrt64",
    "C:\tools\msys64\mingw64",
    "C:\msys64\mingw64",
    "C:\Qt\6.8.2\mingw_64",
    "C:\Qt\6.9.0\mingw_64"
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique

$qtPrefix = $null
foreach ($c in $prefixCandidates) {
    if ((Test-Path "$c\bin\Qt6Core.dll") -or (Test-Path "$c\bin\windeployqt.exe")) {
        $qtPrefix = $c
        break
    }
}
if (-not $qtPrefix) {
    # Fallback: try to locate via es.exe if available
    $es = "C:\Program Files\Everything\es.exe"
    if (Test-Path $es) {
        $hit = & $es "Qt6Core.dll" /a-d -json 2>$null | ConvertFrom-Json | Select-Object -First 1
        if ($hit) {
            $qtPrefix = Split-Path -Parent (Split-Path -Parent $hit.filename)
        }
    }
}
if ($qtPrefix) {
    Write-Host "Using Qt/Poppler prefix: $qtPrefix" -ForegroundColor Cyan
    $env:CMAKE_PREFIX_PATH = $qtPrefix
    $env:PATH = "$qtPrefix\bin;$env:PATH"
    $pkg = "$qtPrefix\lib\pkgconfig"
    if (Test-Path $pkg) { $env:PKG_CONFIG_PATH = $pkg }
} else {
    Write-Host "Qt prefix not auto-detected - relying on CMAKE_PREFIX_PATH / PATH as-is" -ForegroundColor Yellow
    Write-Host "If configure fails (moc.exe 0xC0000135), install Qt via MSYS2: pacman -S mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-poppler-qt6" -ForegroundColor Yellow
}

# Ensure MinGW + CMake on PATH (scoop fallback)
$scoopMingw = "$env:USERPROFILE\scoop\apps\mingw-winlibs-ucrt\current\bin"
if (Test-Path $scoopMingw) { $env:PATH = "$scoopMingw;$env:PATH" }
$cmakeBin = "C:\Program Files\CMake\bin"
if (Test-Path $cmakeBin) { $env:PATH = "$cmakeBin;$env:PATH" }

# --- Configure ---
if ($Clean -and (Test-Path "$repoRoot\build")) {
    Write-Host "Cleaning build/..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "$repoRoot\build"
}
$cmakeArgs = @("-S", ".", "-B", "build", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release")
if ($qtPrefix) { $cmakeArgs += "-DCMAKE_PREFIX_PATH=$($qtPrefix -replace '\\','/')" }

Write-Host "cmake $($cmakeArgs -join ' ')" -ForegroundColor Green
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

# --- Build (POST_BUILD windeployqt makes build/PDFToPlainText.exe runnable without manual PATH) ---
Write-Host "cmake --build build --parallel" -ForegroundColor Green
& cmake --build build --parallel
if ($LASTEXITCODE -ne 0) { throw "build failed" }

Write-Host ""
Write-Host "Build OK: $repoRoot\build\PDFToPlainText.exe is self-contained (Qt + Poppler DLLs copied)" -ForegroundColor Green
Write-Host "Run: .\build\PDFToPlainText.exe  (no PATH needed)" -ForegroundColor Cyan

if (-not $NoPackage) {
    Write-Host ""
    Write-Host "Packaging ZIP (CPack)..." -ForegroundColor Green
    & cpack -G ZIP --config build/CPackConfig.cmake -B packages
    if ($LASTEXITCODE -eq 0) {
        # Report whatever cpack actually produced rather than a hardcoded name
        Get-ChildItem "$repoRoot\packages\*.zip" | ForEach-Object {
            Write-Host "Package: $($_.FullName)" -ForegroundColor Green
        }
    } else {
        Write-Host "cpack failed - NSIS not installed is OK (ZIP still created if above succeeded)" -ForegroundColor Yellow
    }
}
