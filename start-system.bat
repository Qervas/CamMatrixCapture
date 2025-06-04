@echo off
title Clean Sapera Camera System

echo.
echo ===============================================
echo  🚀 Clean Sapera Camera System - Launcher
echo ===============================================
echo.

echo 📁 Current directory: %CD%
echo.

echo 🔍 Checking system components...

if not exist "backend\refactored_capture.exe" (
    echo ❌ Backend executable not found!
    echo    Please build first: cmake --build build --config Release
    pause
    exit /b 1
)

if not exist "web-bridge\web_bridge.py" (
    echo ❌ Web bridge not found!
    pause
    exit /b 1
)

if not exist "frontend\index.html" (
    echo ❌ Frontend not found!
    pause
    exit /b 1
)

echo ✅ All components found!
echo.

echo 🎯 Choose startup option:
echo   1. Start Web Bridge Server (recommended)
echo   2. Test Backend Only
echo   3. Open Frontend Only
echo   4. Build System First
echo.

set /p choice="Enter choice (1-4): "

if "%choice%"=="1" (
    echo.
    echo 🌐 Starting Web Bridge Server...
    echo 💡 Backend will be launched automatically via API
    echo 🖥️  Frontend available at: http://localhost:8080
    echo.
    cd web-bridge
    python web_bridge.py
) else if "%choice%"=="2" (
    echo.
    echo 📸 Testing backend directly...
    cd backend
    refactored_capture.exe --config ..\config\camera_config.json --list-cameras
    pause
) else if "%choice%"=="3" (
    echo.
    echo 🖥️  Opening frontend...
    start frontend\index.html
) else if "%choice%"=="4" (
    echo.
    echo 🔨 Building system...
    if not exist "build" mkdir build
    cd build
    cmake .. -A x64
    cmake --build . --config Release
    echo.
    echo ✅ Build complete! Run this script again.
    pause
) else (
    echo Invalid choice!
    pause
) 