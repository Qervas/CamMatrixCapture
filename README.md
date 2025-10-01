# Camera Matrix Capture

Multi-camera capture system for neural rendering datasets. Master's thesis implementation.

## Quick Start

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
cd build/Release
./neural_capture.exe
```

**Requirements**: Windows 10/11, Teledyne DALSA Sapera SDK, CMake 3.20+, Visual Studio 2019+

See [Getting Started Guide](docs/getting-started.md) for detailed setup instructions.

## Overview

Captures synchronized images from multiple Teledyne DALSA Nano-C4020 cameras (4112×3008) for neural rendering training. Supports Bluetooth turntable control for automated object rotation.

**Output:**
```
neural_dataset/images/20241231_143052_456/
├── camera1_capture_001.tiff
├── camera2_capture_001.tiff
└── ...
```

## Documentation

- **[Getting Started](docs/getting-started.md)** - Installation, configuration, first capture
- **[Architecture](docs/architecture.md)** - System design and data flow
- **[Modules](docs/modules/README.md)** - Component documentation
  - [Hardware](docs/modules/hardware.md) - Camera integration
  - [Bluetooth](docs/modules/bluetooth.md) - Turntable control
  - [GUI](docs/modules/gui.md) - User interface
  - [Capture](docs/modules/capture.md) - Automated capture
  - [Utils](docs/modules/utils.md) - Settings and sessions

## Structure

```
src/
├── main.cpp              # Entry point, WinRT init
├── gui/                  # ImGui interface
├── hardware/             # Sapera SDK integration
├── bluetooth/            # BLE turntable control
├── capture/              # Automated capture
├── rendering/            # Hemisphere visualization
└── utils/                # Settings, sessions
```

## License

MIT - See [LICENSE](LICENSE) for details