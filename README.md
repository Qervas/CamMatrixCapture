# CameraMatrixCapture

Multi-camera dataset capture system based on Sapera SDK and Revopoint dual-axis turntable.

Build your own multi-view capture rig with off-the-shelf industrial cameras and turntable equipment.

## Build

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Requirements**: Windows 10/11, Sapera SDK, CMake 3.16+, Visual Studio 2019+

## Usage

1. Launch `build/Release/neural_capture.exe`
2. Hardware tab: Discover and connect cameras
3. Capture tab: Take single shots or run automated sequences

## Features

- Parallel capture (bandwidth-optimized for 1Gbps network)
- Automated 360° turntable sequences (Bluetooth)
- Custom camera naming and ordering
- TIFF/RAW output formats
- Real-time preview

## Output

Images saved to `neural_dataset/images/[timestamp]/`
Named as: `cam_01.tiff`, `cam_02.tiff`, etc.

## Architecture

```
src/
├── main.cpp                    # WinRT initialization
├── gui/                        # ImGui interface
│   ├── Application.cpp         # Main application loop
│   ├── GuiManager.cpp          # UI state management
│   ├── core/
│   │   └── NavigationRail.cpp  # Tab navigation
│   ├── views/
│   │   ├── HardwareView.cpp    # Camera discovery/connection
│   │   ├── CaptureView.cpp     # Capture controls
│   │   ├── FilesView.cpp       # File browser
│   │   └── SettingsView.cpp    # Preferences
│   └── widgets/
│       ├── CaptureStudioPanel.cpp  # Automated sequences
│       ├── CameraPreviewWidget.cpp # Live preview
│       ├── FileExplorerWidget.cpp  # Directory tree
│       └── SessionWidget.cpp       # Session management
├── hardware/
│   ├── CameraManager.cpp       # Sapera SDK integration
│   └── CameraTypes.cpp         # Camera data structures
├── bluetooth/
│   ├── BluetoothManager.cpp    # Device management
│   ├── BluetoothDevice.cpp     # BLE communication
│   ├── BluetoothScanner.cpp    # Device discovery
│   └── TurntableController.cpp # Rotation control
├── capture/
│   └── AutomatedCaptureController.cpp  # Sequence automation
├── rendering/
│   └── HemisphereRenderer.cpp  # 3D visualization
└── utils/
    ├── SettingsManager.cpp     # INI persistence
    ├── SessionManager.cpp      # File organization
    └── NotificationSounds.cpp  # Windows alerts
```

## Troubleshooting

- **Cameras not found**: Check Sapera SDK installed, cameras powered and connected
- **Blank images**: Increase stagger delay (Settings) or reduce parallel groups
- **Crashes during capture**: Use sequential mode (parallel groups = 1)

## License

MIT License - Copyright (c) 2025 Shaoxuan Yin

See [LICENSE](LICENSE) for full text.