# System Architecture

## Overview

Camera Matrix Capture is a multi-camera acquisition system designed for generating training datasets for neural rendering (NeRF, 3D Gaussian Splatting, etc.). The system coordinates multiple industrial cameras with optional automated turntable rotation.

## Design Principles

1. **Modular**: Each subsystem (cameras, bluetooth, GUI, capture) is independent
2. **Session-based**: All captures organized by timestamped sessions
3. **Hardware-agnostic interfaces**: Easy to swap camera SDKs or BLE protocols
4. **Settings persistence**: User preferences survive across sessions

## System Diagram

```
┌─────────────────────────────────────────────────┐
│                  Application                     │
│              (Main UI Loop)                      │
└───────┬─────────────────────────────────┬───────┘
        │                                 │
        ├─────────────┐          ┌────────┴────────┐
        ▼             ▼          ▼                 ▼
   ┌────────┐   ┌─────────┐  ┌──────────┐   ┌──────────┐
   │  GUI   │   │Hardware │  │Bluetooth │   │ Capture  │
   │ Panels │   │Manager  │  │Manager   │   │Controller│
   └────────┘   └────┬────┘  └────┬─────┘   └─────┬────┘
                     │            │                │
                     ▼            ▼                ▼
              ┌──────────┐  ┌─────────┐    ┌──────────┐
              │  Sapera  │  │  WinRT  │    │ Session  │
              │   SDK    │  │   BLE   │    │ Manager  │
              └──────────┘  └─────────┘    └──────────┘
```

## Data Flow

### Camera Discovery
1. Application starts → CameraManager::DiscoverCameras()
2. Sapera SDK enumerates GigE devices
3. Camera list populated in GUI

### Single Capture
1. User clicks "Capture" → CameraManager::CaptureImage()
2. Sequential capture from each camera (with delay)
3. SessionManager creates timestamped directory
4. Images saved as TIFF files

### Automated Capture
1. User configures angles/shots → CaptureController::Start()
2. For each angle:
   - Send rotation command via BluetoothManager
   - Wait for turntable motion
   - Trigger camera capture
   - Save images to session directory
3. Notification on completion

### Settings Persistence
1. User modifies camera parameters → SettingsManager::SaveCameraSettings()
2. Settings stored in JSON (per-camera)
3. On next launch → SettingsManager::LoadCameraSettings()

## Threading Model

- **Main Thread**: ImGui rendering loop (60 FPS)
- **Camera Operations**: Synchronous on main thread (fast enough)
- **Bluetooth Operations**: Async via WinRT coroutines (`co_await`)
- **No explicit worker threads**: Sapera SDK and WinRT handle async internally

## File Organization

```
neural_dataset/
└── images/
    └── [session_timestamp]/
        ├── camera1_capture_001.tiff
        ├── camera1_capture_002.tiff
        ├── camera2_capture_001.tiff
        └── ...

config/
└── settings.json              # Camera parameters, UI state
```

## Dependencies

- **Sapera SDK**: Camera hardware abstraction (Teledyne DALSA)
- **WinRT/C++**: Windows Bluetooth LE API
- **ImGui**: Immediate mode GUI (docking branch)
- **stb_image_write**: TIFF export
- **nlohmann/json**: Settings serialization

## Module Boundaries

| Module | Responsibilities | Depends On |
|--------|-----------------|------------|
| Hardware | Camera discovery, capture, parameters | Sapera SDK |
| Bluetooth | Device discovery, connection, commands | WinRT BLE |
| GUI | User interface, panels, widgets | ImGui, Hardware, Bluetooth |
| Capture | Automated multi-angle capture | Hardware, Bluetooth, Session |
| Utils | Settings, sessions, notifications | Filesystem, JSON |

See [modules/](modules/README.md) for detailed documentation of each module.
