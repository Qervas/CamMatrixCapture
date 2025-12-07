# System Architecture

## Overview

Camera Matrix Capture is a multi-camera acquisition system designed for generating training datasets for neural rendering (NeRF, 3D Gaussian Splatting, etc.). The system coordinates multiple industrial cameras with automated turntable rotation.

## Design Principles

1. **Modular**: Each subsystem (cameras, bluetooth, capture) is independent
2. **Session-based**: All captures organized by timestamped sessions
3. **Hardware-agnostic interfaces**: Easy to swap camera SDKs or BLE protocols
4. **Settings persistence**: User preferences survive across sessions
5. **Separation of concerns**: C++ backend for hardware, C# frontend for UI

## System Diagram

```
┌─────────────────────────────────────────────────────────────┐
│              WinUI 3 Frontend (C#)                          │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐          │
│  │ Connect │ │  Setup  │ │ Capture │ │  Done   │          │
│  │  Page   │ │  Page   │ │  Page   │ │  Page   │          │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘          │
│                      │                                      │
│              ┌───────┴───────┐                             │
│              │CaptureService │  (P/Invoke)                 │
│              └───────┬───────┘                             │
└──────────────────────┼──────────────────────────────────────┘
                       │
┌──────────────────────┼──────────────────────────────────────┐
│              CaptureCore.dll (C++)                          │
│              ┌───────┴───────┐                             │
│              │  C API Layer  │  (CaptureAPI.h)             │
│              └───────┬───────┘                             │
│  ┌──────────────┬────┴────┬──────────────┐                │
│  │CameraManager │ BTMgr   │ SessionMgr   │                │
│  │(Sapera SDK)  │(WinRT)  │              │                │
│  └──────────────┴─────────┴──────────────┘                │
└─────────────────────────────────────────────────────────────┘
```

## Data Flow

### Camera Discovery
1. User clicks "Discover" on Connect page
2. CaptureService calls `CamMatrix_DiscoverCameras()` via P/Invoke
3. C++ backend uses Sapera SDK to enumerate GigE devices
4. Camera count updated in UI via polling

### Bluetooth Connection
1. User clicks "Scan" on Connect page
2. CaptureService calls `CamMatrix_StartBluetoothScan()` via P/Invoke
3. C++ backend uses WinRT BLE to discover devices
4. User selects turntable from list and connects

### Automated Capture (360° Sequence)
1. User configures preset on Setup page (36/72/custom positions)
2. User clicks "Start" on Capture page
3. CaptureService calls `CamMatrix_StartCapture()` with session name and position count
4. For each position:
   - C++ backend rotates turntable via BluetoothManager
   - Waits for motion completion
   - Triggers all cameras via CameraManager
   - Saves images to session directory
5. Progress reported via callbacks to UI
6. Sound notification on completion

### Settings Persistence
1. User modifies settings via Settings dialog
2. CaptureService calls setter functions (e.g., `CamMatrix_SetExposureTime()`)
3. `CamMatrix_SaveSettings()` persists to settings.ini
4. On next launch, `CamMatrix_LoadSettings()` restores values

## Threading Model

- **UI Thread**: WinUI 3 dispatcher thread for all UI updates
- **Camera Operations**: Async via `Task.Run()` in C#, synchronous in C++ backend
- **Bluetooth Operations**: Async via WinRT coroutines (`co_await`) in C++
- **Status Polling**: UI polls backend state every 200-500ms via DispatcherTimer
- **Callbacks**: Log and progress callbacks marshaled to UI thread

## File Organization

```
neural_dataset/
└── [session_name]/
    ├── pos_001/
    │   ├── cam_01.tiff
    │   ├── cam_02.tiff
    │   └── ...
    ├── pos_002/
    │   └── ...
    └── ...

settings.ini                   # Camera parameters, capture settings (INI format)
```

## Dependencies

### C++ Backend (CaptureCore.dll)
- **Sapera SDK**: Camera hardware abstraction (Teledyne DALSA)
- **WinRT/C++**: Windows Bluetooth LE API
- **SimpleBLE**: Cross-platform BLE library (via vcpkg)
- **CMake**: Build system

### C# Frontend (CameraMatrixCapture.exe)
- **WinUI 3**: Windows App SDK for modern UI
- **.NET 8.0**: Runtime and SDK
- **P/Invoke**: Interop with C++ backend

## Module Boundaries

### C++ Backend Modules

| Module | Responsibilities | Depends On |
|--------|-----------------|------------|
| API | C-style DLL exports, callback management | All modules |
| Hardware | Camera discovery, capture, parameters | Sapera SDK |
| Bluetooth | Device discovery, connection, turntable control | WinRT BLE, SimpleBLE |
| Utils | Settings, sessions, file organization | Filesystem |

### C# Frontend Modules

| Module | Responsibilities | Depends On |
|--------|-----------------|------------|
| Pages | Wizard UI (Connect, Setup, Capture, Done) | CaptureService |
| Dialogs | Settings flyout | CaptureService |
| Services | P/Invoke wrapper for C++ backend | CaptureCore.dll |

See [modules/](modules/README.md) for detailed documentation of each module.

## Build System

```bash
# Build both components
.\build.ps1       # PowerShell
build.bat         # Command Prompt

# Manual build
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
cd ../App
dotnet build -c Release -p:Platform=x64
```

Output: `App\bin\x64\Release\net8.0-windows10.0.19041.0\CameraMatrixCapture.exe`
