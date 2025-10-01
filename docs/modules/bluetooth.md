# Bluetooth Module

Turntable control via Bluetooth Low Energy (BLE).

## Responsibilities

- Discover BLE devices (turntables)
- Connect/disconnect to turntable controllers
- Send rotation and tilt commands
- Coordinate automated capture sequences
- Persist connection settings

## Key Classes

### BluetoothManager

Singleton managing all Bluetooth operations.

**Interface:**
```cpp
static BluetoothManager& GetInstance();

// Discovery
void StartScanning();
void StopScanning();
bool IsScanning();
std::vector<std::pair<string, string>> GetDiscoveredDevices();

// Connection
bool ConnectToDevice(deviceId);
void DisconnectDevice(deviceId);
void DisconnectAllDevices();
bool IsDeviceConnected(deviceId);
std::vector<string> GetConnectedDevices();

// Commands
bool SendCommand(deviceId, command);
bool SendCommandWithResponse(deviceId, command, callback);

// Turntable-specific
bool RotateTurntable(deviceId, angle);
bool SetRotationSpeed(deviceId, speed);
bool StopRotation(deviceId);
bool TiltTurntable(deviceId, angle);
bool SetTiltSpeed(deviceId, speed);
bool ReturnToZero(deviceId);

// Configuration
void SetServiceUUID(uuid);
void SetCharacteristicUUID(uuid);
```

**Default GATT UUIDs:**
```cpp
Service:        0000ffe0-0000-1000-8000-00805f9b34fb
Characteristic: 0000ffe1-0000-1000-8000-00805f9b34fb
```

**DeviceInfo struct:**
```cpp
struct DeviceInfo {
    std::string id;                           // Device ID (BLE address)
    std::string name;                         // Human-readable name
    bool connected;
    std::chrono::steady_clock::time_point lastSeen;
    int signalStrength;                       // RSSI
};
```

### BluetoothDevice

Represents a single BLE device connection.

**Interface:**
```cpp
BluetoothDevice(deviceId);

// Connection
bool Connect();
void Disconnect();
bool IsConnected();

// Commands
bool SendCommand(command);
bool SendCommandWithResponse(command, callback);

// Async operations (WinRT coroutines)
IAsyncOperation<bool> ConnectAsync();
IAsyncOperation<bool> SendCommandAsync(command);
IAsyncOperation<hstring> ReadResponseAsync();
```

**Internal command queue:**
- Commands are queued and processed sequentially
- Prevents BLE stack overload
- Automatic retries on failure

### BluetoothScanner

Device discovery using Windows DeviceWatcher API.

**Functionality:**
- Enumerates BLE devices matching service UUID
- Filters out non-turntable devices
- Provides real-time discovery callbacks
- Manual control (no automatic timeout)

### BluetoothCommands

Command helpers for turntable protocol.

**Rotation commands:**
```cpp
RotateByAngle(angle)        // "+CT,TURNANGLE=<angle>;"
RotateLeft360()             // "+CT,TURNANGLE=-360;"
RotateRight360()            // "+CT,TURNANGLE=360;"
RotateContinuousLeft()      // "+CT,TURNCONTINUE=-1;"
RotateContinuousRight()     // "+CT,TURNCONTINUE=1;"
StopRotation()              // "+CT,STOP;"
ReturnToZero()              // "+CT,TOZERO;"
SetRotationSpeed(speed)     // "+CT,TURNSPEED=<speed>;" (35.64-131)
```

**Tilt commands:**
```cpp
TiltByAngle(angle)          // "+CR,TILTVALUE=<angle>;"
TiltLeft30()                // "+CR,TILTVALUE=-30;"
TiltRight30()               // "+CR,TILTVALUE=30;"
StopTilt()                  // "+CR,STOP;"
TiltToZero()                // "+CR,TILTVALUE=0;"
SetTiltSpeed(speed)         // "+CR,TILTSPEED=<speed>;" (9-35)
```

**Query commands:**
```cpp
QueryStatus()               // "+QT,CHANGEANGLE;"
QueryVersion()              // "+QT,VERSION;"
```

**Presets:**
```cpp
namespace Presets {
    // Rotation angles
    ROTATION_STEP_15, ROTATION_STEP_30, ROTATION_STEP_45,
    ROTATION_STEP_60, ROTATION_STEP_90

    // Tilt angles
    TILT_UP_15, TILT_UP_30, TILT_DOWN_15, TILT_DOWN_30

    // Speeds
    ROTATION_SPEED_SLOW (40), ROTATION_SPEED_MEDIUM (70), ROTATION_SPEED_FAST (100)
    TILT_SPEED_SLOW (10), TILT_SPEED_MEDIUM (20), TILT_SPEED_FAST (30)
}
```

**CaptureSequence struct:**
```cpp
struct CaptureSequence {
    int steps;              // Number of rotation steps
    float anglePerStep;     // Degrees per step
    float tiltAngle;        // Tilt angle for this sequence
    float rotationSpeed;
    float tiltSpeed;
    int delayMs;            // Delay between steps
};

// Common sequences
Sequences::Basic360()       // 12 steps × 30°
Sequences::Detailed360()    // 24 steps × 15°
Sequences::Quick360()       // 8 steps × 45°
```

## WinRT Integration

Uses **Windows Runtime (WinRT) C++** for BLE:

```cpp
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>

// Must call in main() before any BLE operations
winrt::init_apartment();

// Async operation example
IAsyncOperation<bool> BluetoothDevice::ConnectAsync() {
    // Get BLE device from ID
    auto device = co_await BluetoothLEDevice::FromIdAsync(deviceId);

    // Get GATT service
    auto servicesResult = co_await device.GetGattServicesForUuidAsync(serviceGuid);
    m_service = servicesResult.Services().GetAt(0);

    // Get characteristic
    auto charsResult = co_await m_service.GetCharacteristicsForUuidAsync(charGuid);
    m_characteristic = charsResult.Characteristics().GetAt(0);

    co_return true;
}
```

**Compiler requirements:**
- `/await` flag (C++ coroutines)
- Link against `windowsapp.lib`

## Protocol Details

### Device ID Format
```
BluetoothLE#BluetoothLE00:0f:f0:02:d5:b9-f8:01:29:2b:3d:5b
                         └─────┬──────┘  └─────┬──────┘
                          MAC address    Additional identifier
```

### GATT Service Structure
```
Service: 0000ffe0-0000-1000-8000-00805f9b34fb
└── Characteristic: 0000ffe1-0000-1000-8000-00805f9b34fb
    ├── Write (command transmission)
    └── Notify (status updates)
```

### Command Format
```
+CT,TURNANGLE=30;
│ │  │         │
│ │  │         └─ Terminator
│ │  └─────────── Parameter
│ └────────────── Command type
└──────────────── Prefix
```

**Command types:**
- `CT` - Turntable control (rotation)
- `CR` - Cradle control (tilt)
- `QT` - Query

## Automated Capture Flow

1. **Setup phase:**
   - Connect to turntable
   - Set rotation/tilt speed
   - Return to zero position

2. **Capture loop:**
   ```cpp
   for (int step = 0; step < steps; ++step) {
       // Rotate to next angle
       RotateTurntable(deviceId, anglePerStep);

       // Wait for motion to complete
       std::this_thread::sleep_for(motionDelay);

       // Capture images from all cameras
       CameraManager::CaptureAllCameras(sessionPath);

       // Wait before next rotation
       std::this_thread::sleep_for(captureDelay);
   }
   ```

3. **Cleanup phase:**
   - Return to zero
   - Disconnect (optional)

See [Capture Module](capture.md) for full automation details.

## Extension Points

### Replacing Turntable Protocol

To support different turntable hardware:

1. Keep `BluetoothManager` interface unchanged
2. Update `BluetoothCommands` with new protocol:
```cpp
inline std::string RotateByAngle(float angle) {
    // Your protocol here
    return "ROT:" + std::to_string(angle) + "\n";
}
```
3. Adjust GATT UUIDs if needed:
```cpp
manager.SetServiceUUID("your-service-uuid");
manager.SetCharacteristicUUID("your-characteristic-uuid");
```

### Using Serial Instead of BLE

Replace WinRT BLE with serial port:

1. Replace `BluetoothDevice.cpp` implementation
2. Use Windows Serial API or third-party library
3. Update `ConnectToDevice()` to use COM port
4. Keep command format identical

**Example:**
```cpp
bool BluetoothDevice::SendCommand(const std::string& command) {
    HANDLE hSerial = CreateFile("COM3", GENERIC_WRITE, ...);
    DWORD written;
    WriteFile(hSerial, command.c_str(), command.size(), &written, nullptr);
    CloseHandle(hSerial);
    return written == command.size();
}
```

### Adding New Turntable Features

1. Add command builder to `BluetoothCommands.hpp`:
```cpp
inline std::string SetLighting(bool on) {
    return on ? "+CL,ON;" : "+CL,OFF;";
}
```

2. Add method to `BluetoothManager`:
```cpp
bool SetLighting(const std::string& deviceId, bool on) {
    return SendCommand(deviceId, BluetoothCommands::SetLighting(on));
}
```

3. Add UI control in `BluetoothGui.cpp`

## Common Issues

**Problem:** Device not discovered
**Solution:** Check Bluetooth is enabled, device is in pairing mode, UUID filter is correct

**Problem:** Connection fails
**Solution:** Unpair device in Windows Settings, restart application, verify GATT service UUID

**Problem:** Commands not responding
**Solution:** Check characteristic UUID, ensure write permissions, verify command format

**Problem:** Turntable moves erratically
**Solution:** Reduce rotation speed, increase delay between commands, send stop command first

## Hardware Specifications

**Compatible Turntables:**
- Generic BLE turntables with UART service (0xFFE0/0xFFE1)
- Command-based rotation control
- Rotation range: 360° continuous
- Tilt range: Typically ±30° (hardware-dependent)
- Speed range: 35.64-131 (rotation), 9-35 (tilt)

**Bluetooth Requirements:**
- Bluetooth 4.0+ (BLE)
- Windows 10 version 1803+ (for WinRT APIs)
- GATT profile support
