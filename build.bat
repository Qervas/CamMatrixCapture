@echo off
setlocal enabledelayedexpansion

echo ============================================
echo   Camera Matrix Capture - Build Script
echo ============================================
echo.

:: Check for Visual Studio
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] Setting up Visual Studio environment...
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else (
        echo [ERROR] Visual Studio 2022 not found!
        exit /b 1
    )
)

:: Step 1: Build C++ Backend DLL
echo [1/3] Building C++ Backend (CaptureCore.dll)...
echo.

if not exist build mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" -A x64 >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed!
    cd ..
    exit /b 1
)

cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [ERROR] C++ build failed!
    cd ..
    exit /b 1
)

cd ..
echo.
echo [OK] CaptureCore.dll built successfully
echo.

:: Step 2: Build WinUI 3 Frontend
echo [2/3] Building WinUI 3 Frontend (CameraMatrixCapture.exe)...
echo.

:: Find dotnet SDK
set DOTNET_PATH=
if exist "C:\Program Files\dotnet\dotnet.exe" (
    set "DOTNET_PATH=C:\Program Files\dotnet\dotnet.exe"
) else if exist "%ProgramFiles%\dotnet\dotnet.exe" (
    set "DOTNET_PATH=%ProgramFiles%\dotnet\dotnet.exe"
) else if exist "%LOCALAPPDATA%\Microsoft\dotnet\dotnet.exe" (
    set "DOTNET_PATH=%LOCALAPPDATA%\Microsoft\dotnet\dotnet.exe"
) else (
    where dotnet >nul 2>&1
    if %errorlevel% equ 0 (
        for /f "delims=" %%i in ('where dotnet') do set "DOTNET_PATH=%%i"
    )
)

if "%DOTNET_PATH%"=="" (
    echo [ERROR] .NET SDK not found! Please install .NET 8.0 SDK.
    echo Download from: https://dotnet.microsoft.com/download/dotnet/8.0
    exit /b 1
)

echo [INFO] Using dotnet: %DOTNET_PATH%

cd App
"%DOTNET_PATH%" build -c Release -p:Platform=x64
if %errorlevel% neq 0 (
    echo [ERROR] WinUI build failed!
    cd ..
    exit /b 1
)

cd ..
echo.
echo [OK] CameraMatrixCapture.exe built successfully
echo.

:: Step 3: Copy DLL to output
echo [3/3] Copying CaptureCore.dll to output...
copy /Y "build\Release\CaptureCore.dll" "App\bin\x64\Release\net8.0-windows10.0.19041.0\" >nul
echo.

echo ============================================
echo   Build Complete!
echo ============================================
echo.
echo Output: App\bin\x64\Release\net8.0-windows10.0.19041.0\
echo.
echo Run: App\bin\x64\Release\net8.0-windows10.0.19041.0\CameraMatrixCapture.exe
echo.

pause
