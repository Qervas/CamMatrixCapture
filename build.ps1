# Camera Matrix Capture - Build Script (PowerShell)

$ErrorActionPreference = "Stop"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Camera Matrix Capture - Build Script" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

$RootDir = $PSScriptRoot
$BuildDir = Join-Path $RootDir "build"
$AppDir = Join-Path $RootDir "App"
$OutputDir = Join-Path $AppDir "bin\x64\Release\net8.0-windows10.0.19041.0"

# Find dotnet SDK
$dotnetPath = $null
$dotnetLocations = @(
    "C:\Program Files\dotnet\dotnet.exe",
    "$env:ProgramFiles\dotnet\dotnet.exe",
    "$env:LOCALAPPDATA\Microsoft\dotnet\dotnet.exe"
)

foreach ($loc in $dotnetLocations) {
    if (Test-Path $loc) {
        $dotnetPath = $loc
        break
    }
}

# Also check PATH
if (-not $dotnetPath) {
    $dotnetPath = Get-Command dotnet -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}

if (-not $dotnetPath) {
    Write-Host "[ERROR] .NET SDK not found! Please install .NET 8.0 SDK." -ForegroundColor Red
    Write-Host "Download from: https://dotnet.microsoft.com/download/dotnet/8.0" -ForegroundColor Yellow
    exit 1
}

Write-Host "[INFO] Using dotnet: $dotnetPath" -ForegroundColor Gray

# Step 1: Build C++ Backend DLL
Write-Host "[1/3] Building C++ Backend (CaptureCore.dll)..." -ForegroundColor Yellow
Write-Host ""

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Push-Location $BuildDir
try {
    # Configure
    $cmakeResult = & cmake .. -G "Visual Studio 17 2022" -A x64 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] CMake configuration failed!" -ForegroundColor Red
        Write-Host $cmakeResult
        exit 1
    }

    # Build
    $buildResult = & cmake --build . --config Release 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] C++ build failed!" -ForegroundColor Red
        Write-Host $buildResult
        exit 1
    }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "[OK] CaptureCore.dll built successfully" -ForegroundColor Green
Write-Host ""

# Step 2: Build WinUI 3 Frontend
Write-Host "[2/3] Building WinUI 3 Frontend (CameraMatrixCapture.exe)..." -ForegroundColor Yellow
Write-Host ""

Push-Location $AppDir
try {
    $dotnetResult = & $dotnetPath build -c Release -p:Platform=x64 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] WinUI build failed!" -ForegroundColor Red
        Write-Host $dotnetResult
        exit 1
    }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "[OK] CameraMatrixCapture.exe built successfully" -ForegroundColor Green
Write-Host ""

# Step 3: Copy DLL to output
Write-Host "[3/3] Copying CaptureCore.dll to output..." -ForegroundColor Yellow

$DllSource = Join-Path $BuildDir "Release\CaptureCore.dll"
if (Test-Path $DllSource) {
    Copy-Item -Path $DllSource -Destination $OutputDir -Force
    Write-Host "[OK] DLL copied" -ForegroundColor Green
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Build Complete!" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Output: $OutputDir" -ForegroundColor White
Write-Host ""
Write-Host "Run:" -ForegroundColor White
Write-Host "  $OutputDir\CameraMatrixCapture.exe" -ForegroundColor Gray
Write-Host ""
