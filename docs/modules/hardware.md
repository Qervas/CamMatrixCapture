# Hardware Module

Camera integration via Teledyne DALSA Sapera SDK.

## Responsibilities

- Discover GigE Vision cameras on the network
- Connect/disconnect cameras
- Configure camera parameters (exposure, gain, white balance, etc.)
- Capture images (single or sequential multi-camera)
- Handle raw/color image formats

## Key Classes

### CameraManager

Singleton that manages all camera operations.

**Interface:**
```cpp
static CameraManager& GetInstance();

// Discovery and connection
void DiscoverCameras(log_callback);
void ConnectAllCameras(log_callback);
void DisconnectAllCameras();

// Capture
bool CaptureAllCameras(session_path, sequential, delay_ms);
void CaptureAllCamerasAsync(session_path, sequential, delay_ms, log_callback);
bool CaptureCamera(camera_id, session_path);

// Parameters
void ApplyParameterToAllCameras(feature_name, value);
void ApplyParameterToCamera(camera_id, feature_name, value);

// State
const std::vector<CameraInfo>& GetDiscoveredCameras();
bool IsConnected(camera_id);
bool IsDiscovering() / IsConnecting() / IsCapturing();
```

**Parameters struct:**
```cpp
struct Parameters {
    int width, height;
    int max_width, max_height;
    std::string pixel_format;  // "RGB8", "BayerRG8", "Mono8", etc.
    int exposure_time;         // microseconds
    float gain;
    float wb_red, wb_green, wb_blue;
    bool auto_white_balance;
    int packet_size;           // GigE optimization
    int packet_delay;          // GigE optimization
};
```

**ColorConfig struct:**
```cpp
struct ColorConfig {
    int color_method;               // 1-7 (Sapera color algorithms)
    int bayer_align;                // 0-5 (Bayer alignment)
    bool use_hardware;              // HW vs SW demosaic
    std::string color_output_format; // "RGB888", "RGB8888", "RGB101010"
    float gamma;
    float wb_gain_r, wb_gain_g, wb_gain_b;
    float wb_offset_r, wb_offset_g, wb_offset_b;
};
```

### CameraTypes

Data structures for camera information.

**CameraInfo:**
```cpp
struct CameraInfo {
    std::string id;
    std::string name;
    std::string serialNumber;
    std::string modelName;
    std::string serverName;
    int resourceIndex;
    bool isConnected;
    CameraStatus status;
    CameraType type;
    CameraCapabilities capabilities;
};
```

**ImageBuffer:**
```cpp
struct ImageBuffer {
    uint32_t width, height;
    PixelFormat pixelFormat;
    std::vector<uint8_t> data;
    ImageMetadata metadata;
};
```

## Sapera SDK Integration

The system uses **direct Sapera SDK calls** (no abstraction layer):

- `SapManager::GetResourceCount()` - Enumerate cameras
- `SapAcqDevice` - Camera device handle
- `SapBuffer` - Image buffer allocation
- `SapAcqDeviceToBuf` - Transfer object (device → buffer)
- `SapColorConversion` - Bayer → RGB demosaic

**Discovery flow:**
```cpp
int serverCount = SapManager::GetServerCount();
for (int srv = 0; srv < serverCount; ++srv) {
    int resCount = SapManager::GetResourceCount(srv, SapManager::ResourceAcq);
    for (int res = 0; res < resCount; ++res) {
        char name[256];
        SapManager::GetResourceName(srv, SapManager::ResourceAcq, res, name, 256);
        // Create CameraInfo
    }
}
```

**Capture flow:**
```cpp
// 1. Create device
SapAcqDevice* device = new SapAcqDevice(serverName, resourceName);
device->Create();

// 2. Allocate buffer
SapBuffer* buffer = new SapBuffer(1, device);  // 1 frame
buffer->Create();

// 3. Create transfer
SapAcqDeviceToBuf* transfer = new SapAcqDeviceToBuf(device, buffer);
transfer->Create();

// 4. Grab
transfer->Grab();
transfer->Wait(5000);  // 5s timeout

// 5. Save image
buffer->Save("output.tif", "-format tiff");
```

## Sequential Capture Mode

For **multi-camera systems**, bandwidth limitations require sequential capture:

```cpp
bool CameraManager::CaptureAllCameras(session_path, sequential=true, delay_ms=750) {
    if (sequential) {
        for (auto& [id, device] : connected_devices_) {
            CaptureCamera(id, session_path);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    } else {
        // Parallel capture (may saturate network)
    }
}
```

**Why 750ms delay?** Teledyne DALSA Nano-C4020 cameras at 4112×3008 resolution produce ~37MB raw images. GigE bandwidth limits require staggered captures.

## Extension Points

### Replacing Sapera SDK

To use a different camera SDK (e.g., OpenCV, Spinnaker, Aravis):

1. Replace `CameraManager.cpp` implementation
2. Keep `CameraManager.hpp` interface unchanged
3. Update `CMakeLists.txt` to link new SDK
4. Map SDK-specific formats to `PixelFormat` enum

**Example: OpenCV adaptation**
```cpp
void CameraManager::DiscoverCameras(log_callback) {
    discovered_cameras_.clear();

    // OpenCV VideoCapture enumeration
    for (int i = 0; i < 10; ++i) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            CameraInfo info;
            info.id = "opencv_" + std::to_string(i);
            info.width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
            info.height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
            discovered_cameras_.push_back(info);
        }
    }
}
```

### Adding New Parameters

1. Add field to `CameraManager::Parameters` struct
2. Implement in `ApplyParameterToCamera()`:
```cpp
void CameraManager::ApplyParameterToCamera(camera_id, feature_name, value) {
    if (feature_name == "MyNewParameter") {
        device->SetFeatureValue("MyFeatureName", value);
    }
}
```
3. Add UI control in `HardwarePanel.cpp`
4. Add to `SettingsManager` for persistence

### Supporting New Image Formats

1. Add format to `PixelFormat` enum in `CameraTypes.hpp`
2. Update `toString(PixelFormat)` in `CameraTypes.cpp`
3. Handle format in color conversion logic
4. Update buffer size calculation in `ImageBuffer::getDataSize()`

## Common Issues

**Problem:** Cameras not discovered
**Solution:** Check GigE network adapter settings, firewall, Sapera installation

**Problem:** Capture timeout
**Solution:** Increase packet size, decrease packet delay, check network bandwidth

**Problem:** Color artifacts in images
**Solution:** Adjust `ColorConfig::bayer_align` (usually 2 for RGGB), verify pixel format

**Problem:** Sequential capture too slow
**Solution:** Reduce `delay_ms` if network can handle it, or use parallel mode with bandwidth monitoring

## Hardware Specifications

**Teledyne DALSA Nano-C4020:**
- Resolution: 4112×3008 (12.4 MP)
- Sensor: CMOSIS CMV4000 CMOS
- Interface: GigE Vision
- Pixel formats: Mono8, Mono10, Mono12, BayerRG8
- Max frame rate: 90 FPS (at reduced resolution)
- Exposure range: 10μs - 60s
- Gain range: 0dB - 24dB
