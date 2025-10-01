# Getting Started

Quick guide to building and using Camera Matrix Capture.

## Prerequisites

### Software Requirements

1. **Windows 10/11** (64-bit)
2. **CMake 3.20+**
   Download: https://cmake.org/download/
3. **Visual Studio 2019+** with C++ development tools
   Download: https://visualstudio.microsoft.com/
4. **Teledyne DALSA Sapera SDK**
   Download: https://www.teledynedalsa.com/en/products/imaging/vision-software/sapera-lt/
   Install to default location: `C:/Program Files/Teledyne DALSA/Sapera`

### Hardware Requirements

- **Cameras**: Teledyne DALSA Nano-C4020 (or compatible GigE Vision cameras)
- **Network**: Dedicated GigE adapter for cameras (recommended)
- **Turntable** (optional): BLE turntable with UART service (UUID: 0xFFE0/0xFFE1)

## Build Instructions

### 1. Clone Repository

```bash
git clone https://github.com/yourusername/CamMatrixCapture.git
cd CamMatrixCapture
```

### 2. Configure with CMake

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
```

**Note:** Adjust Visual Studio version as needed (e.g., `"Visual Studio 16 2019"`)

### 3. Build

```bash
cmake --build . --config Release
```

**Build output:**
```
build/
└── Release/
    ├── neural_capture.exe
    ├── *.dll (Sapera, ImGui, etc.)
    └── assets/
```

### 4. Run

```bash
cd Release
./neural_capture.exe
```

## First Launch Setup

### Camera Configuration

1. **Connect cameras** to GigE network adapter
2. **Launch application**
3. Open **Hardware Panel** (left sidebar)
4. Click **"Discover Cameras"**
   - Wait 5-10 seconds for discovery
   - Connected cameras appear in list
5. Click **"Connect All"**
6. Adjust camera parameters:
   - **Exposure time**: 40000μs (default)
   - **Gain**: 1.0-3.0 (adjust for brightness)
   - **Pixel format**: RGB8 (color) or BayerRG8 (raw)
7. Test capture: Click **"Capture All"**
   - Images saved to `neural_dataset/images/[timestamp]/`

### Bluetooth Turntable (Optional)

1. **Power on turntable** and enable pairing mode
2. Open **Bluetooth Panel** (View → Bluetooth)
3. Click **"Start Scanning"**
4. Select turntable from discovered devices
5. Click **"Connect"**
6. Test rotation:
   - Enter angle (e.g., 30°)
   - Click **"Rotate"**
   - Verify turntable movement

### Settings

1. Open **Preferences** (Edit → Preferences or Ctrl+,)
2. Configure:
   - **Output directory**: Default is `neural_dataset/images`
   - **UI scale**: Adjust for monitor DPI
   - **Sequential capture delay**: 750ms (default)
   - **Capture format**: Raw or color-corrected
3. Click **"Apply"** and **"Save"**

## Basic Capture Workflow

### Single Capture

1. **Connect cameras** (Hardware Panel)
2. **Adjust parameters** (exposure, gain)
3. Click **"Capture All"**
4. Images saved to new timestamped session folder

### Automated Multi-Angle Capture

1. **Connect turntable** (Bluetooth Panel)
2. **Connect cameras** (Hardware Panel)
3. Open **Capture Studio Panel** (center window)
4. Configure capture:
   - **Rotation steps**: 12 (30° per step)
   - **Tilt angles**: 0° (single ring)
   - **Motion delay**: 2000ms (turntable settling time)
   - **Capture delay**: 500ms (pre-capture wait)
5. Click **"Start Capture"**
6. Progress bar shows completion status
7. Notification sound plays when finished

**Output structure:**
```
neural_dataset/images/20250101_143052_456/
├── camera1_capture_001.tiff
├── camera1_capture_002.tiff
├── ...
├── camera2_capture_001.tiff
├── camera2_capture_002.tiff
└── ...
```

## Network Configuration (Cameras)

For optimal performance with multiple cameras:

### GigE Adapter Settings

1. **Dedicated network adapter** for cameras (separate from internet)
2. **Static IP configuration:**
   - Adapter IP: `192.168.1.100`
   - Subnet mask: `255.255.255.0`
   - Gateway: (leave blank)
3. **Jumbo frames:** Enable (9000 MTU) if supported
4. **Firewall:** Allow Sapera SDK through firewall

### Camera Network Settings

1. Assign static IPs to cameras:
   - Camera 1: `192.168.1.101`
   - Camera 2: `192.168.1.102`
   - Camera 3: `192.168.1.103`
   - etc.
2. Configure in Sapera CamExpert (included with Sapera SDK)

### Performance Tuning

If capture timeouts occur:

1. **Increase packet delay** (Hardware Panel → Advanced)
2. **Reduce resolution** (if possible)
3. **Use sequential capture** (enable in Hardware Panel)
4. **Increase sequential delay** (750ms → 1000ms)

## Troubleshooting

### Cameras Not Discovered

**Symptoms:** Discovery finishes with 0 cameras

**Solutions:**
- Verify Sapera SDK installation (`C:/Program Files/Teledyne DALSA/Sapera`)
- Check network cable connections
- Ensure cameras are powered on
- Run Sapera CamExpert to verify camera visibility
- Check Windows Firewall settings

### Capture Timeout

**Symptoms:** "Capture failed: timeout" error

**Solutions:**
- Increase **max capture wait time** (Preferences → Capture)
- Enable **sequential mode** (Hardware Panel)
- Increase **sequential delay** (750ms → 1500ms)
- Check network bandwidth (GigE adapter activity)
- Reduce camera resolution

### Bluetooth Connection Failed

**Symptoms:** Cannot connect to turntable

**Solutions:**
- Unpair device in Windows Settings (Bluetooth & devices)
- Restart application
- Verify turntable is in pairing mode
- Check **Service UUID** matches turntable (Bluetooth Panel → Settings)
- Ensure Windows Bluetooth is enabled

### Blurry Images

**Symptoms:** Motion blur in captured images

**Solutions:**
- Increase **settle time** (Capture Studio → Motion delay)
- Reduce **rotation/tilt speed** (Capture Studio → Speed settings)
- Ensure object is securely mounted on turntable
- Check for camera focus issues

### Application Crashes on Startup

**Symptoms:** Immediate crash or "WinRT not initialized" error

**Solutions:**
- Update Windows to latest version (required for WinRT)
- Install **Visual C++ Redistributable** (2019+)
- Run as Administrator (first launch only)
- Check antivirus isn't blocking executable

## Next Steps

- **Read [Architecture docs](architecture.md)** to understand system design
- **Explore [Module docs](modules/README.md)** to customize behavior
- **Modify capture patterns** (see [Capture Module](modules/capture.md))
- **Integrate new cameras** (see [Hardware Module](modules/hardware.md))
- **Adapt turntable protocol** (see [Bluetooth Module](modules/bluetooth.md))

## Quick Reference

**Default shortcuts:**
- `Ctrl+,` - Preferences
- `Ctrl+Q` - Quit
- `F11` - Toggle fullscreen

**Default paths:**
- Output: `neural_dataset/images/`
- Settings: `config/settings.json`
- Logs: Console window (View → Log Panel)

**Default parameters:**
- Exposure: 40000μs
- Gain: 1.0
- Sequential delay: 750ms
- Motion settle: 2000ms
- Capture delay: 500ms
