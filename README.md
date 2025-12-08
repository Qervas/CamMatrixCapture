# Camera Matrix Capture

Multi-camera dataset capture system for neural rendering, using Teledyne DALSA industrial cameras and Revopoint dual-axis turntable.

Build your own multi-view capture rig with off-the-shelf industrial cameras and turntable equipment.

![Connect Page](image/README/connect.png)

## Features

- **Wizard-style UI** - Step-by-step workflow for beginners
- **Multi-camera sync** - Parallel capture with staggered timing (bandwidth-optimized)
- **Bluetooth turntable control** - Automated 360° capture sequences
- **Real-time status** - Live capture/rotation state machine with timing display
- **Flexible presets** - Quick 36, Detailed 72, or custom positions
- **Drag-and-drop camera ordering** - Reorder cameras in the UI
- **TIFF output** - High-quality ~35MB images per capture
- **Sound notifications** - Audio feedback on completion

## Requirements

- Windows 10/11
- [Sapera SDK](https://www.teledynedalsa.com/en/products/imaging/vision-software/sapera-lt/) (Teledyne DALSA)
- Visual Studio 2022
- .NET 8.0 SDK
- CMake 3.16+

## Build

### Quick Build

```powershell
.\build.ps1
```

Or using Command Prompt:

```cmd
build.bat
```

### Manual Build

```bash
# 1. Build C++ backend DLL
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
cd ..

# 2. Build WinUI 3 frontend
cd App
dotnet build -c Release -p:Platform=x64
```

### Output

```
App\bin\x64\Release\net8.0-windows10.0.19041.0\CameraMatrixCapture.exe
```

## Usage

1. Launch `CameraMatrixCapture.exe`
2. **Connect**: Discover cameras and connect Bluetooth turntable
3. **Setup**: Choose capture preset (36/72 positions or custom)
4. **Capture**: Start automated 360° capture sequence
5. **Done**: View results and open output folder

## Output

Images saved to `neural_dataset/[session_name]/`

```
neural_dataset/
└── session_001/
    ├── pos_001/
    │   ├── cam_01.tiff
    │   ├── cam_02.tiff
    │   └── ...
    ├── pos_002/
    │   └── ...
    └── ...
```

## Architecture

```
CamMatrixCapture/
├── App/                           # WinUI 3 Frontend (C#)
│   ├── Pages/
│   │   ├── ConnectPage.xaml       # Step 1: Device connection
│   │   ├── SetupPage.xaml         # Step 2: Capture settings
│   │   ├── CapturePage.xaml       # Step 3: Capture execution
│   │   └── DonePage.xaml          # Step 4: Completion
│   ├── Dialogs/
│   │   ├── SettingsDialog.xaml    # Settings flyout
│   │   └── DebugConsoleDialog.xaml # Debug log viewer
│   └── Services/
│       └── CaptureService.cs      # P/Invoke wrapper
│
├── src/                           # C++ Backend (CaptureCore.dll)
│   ├── api/
│   │   ├── CaptureAPI.cpp         # C-style DLL exports
│   │   └── CaptureStateMachine.cpp # Capture/rotation state machine
│   ├── hardware/
│   │   └── CameraManager.cpp      # Sapera SDK integration
│   ├── bluetooth/
│   │   ├── BluetoothManager.cpp   # BLE device management
│   │   ├── BluetoothDevice.cpp    # BLE communication
│   │   └── TurntableController.cpp
│   └── utils/
│       ├── SettingsManager.cpp    # INI persistence
│       └── SessionManager.cpp     # File organization
│
├── build.bat                      # Build script (CMD)
├── build.ps1                      # Build script (PowerShell)
└── CMakeLists.txt
```

### State Machine

The capture-rotation cycle is managed by a state machine to ensure correct synchronization:

```
Idle → Capturing → Rotating → Settling → Idle (repeat)
```

States:
- **Idle**: Ready for next operation
- **Capturing**: Cameras actively capturing images
- **Rotating**: Turntable moving to next position
- **Settling**: Waiting for turntable vibration to stop

## Settings

Settings are stored in `settings.ini` next to the executable:

- **Exposure Time**: Camera exposure in microseconds
- **Gain**: Camera gain in dB
- **Parallel Groups**: Number of cameras capturing simultaneously
- **Stagger Delay**: Delay between camera triggers (ms)
- **Output Directory**: Where to save captured images

## Troubleshooting

- **Cameras not found**: Check Sapera SDK is installed, cameras are powered and connected via GigE
- **Blank images**: Increase stagger delay or reduce parallel groups in Settings
- **Turntable not found**: Enable Bluetooth, ensure turntable is powered on
- **Build fails**: Ensure .NET 8.0 SDK and Visual Studio 2022 are installed

## License

MIT License - Copyright (c) 2025 Shaoxuan Yin

See [LICENSE](LICENSE) for full text.
