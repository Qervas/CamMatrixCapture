# Capture Module

Automated multi-angle capture orchestration.

## Responsibilities

- Coordinate turntable movement and camera capture
- Execute capture sequences (rotation + tilt patterns)
- Track capture progress
- Handle timing and settling delays
- Error recovery and retry logic

## Key Classes

### AutomatedCaptureController

Orchestrates automated capture sequences.

**Interface:**
```cpp
AutomatedCaptureController();

// Control
void StartSequence(positions, bluetooth, camera, session);
void PauseSequence();
void ResumeSequence();
void StopSequence();

// Status
bool IsActive();
bool IsPaused();
ControllerState GetState();
int GetCurrentPositionIndex();
float GetProgress();

// Configuration
void SetSettleTime(seconds);           // Turntable settling delay
void SetCaptureDelay(seconds);         // Pre-capture delay
void SetRotationSpeed(speed);          // Turntable rotation speed
void SetTiltSpeed(speed);              // Turntable tilt speed
void SetMaxCaptureWaitTime(seconds);   // Camera timeout

// Callbacks
void SetProgressCallback(callback);    // Progress updates
void SetLogCallback(callback);         // Log messages
```

**ControllerState enum:**
```cpp
enum class ControllerState {
    Idle,               // Not running
    MovingTurntable,    // Rotating/tilting
    WaitingForSettle,   // Waiting for motion to stabilize
    Capturing,          // Taking photos
    Processing,         // Saving images
    Completed,          // Sequence finished
    Error               // Failure occurred
};
```

**CapturePosition struct:**
```cpp
struct CapturePosition {
    float azimuth;      // Rotation angle (0-360°)
    float elevation;    // Tilt angle (-30 to +30°)
    std::string label;  // Position identifier
};
```

## Automated Capture Flow

### Sequence Execution

```cpp
void StartSequence(positions, bluetooth, camera, session) {
    // 1. Initialize
    current_position_index_ = 0;
    is_active_ = true;
    should_stop_ = false;

    // 2. Spawn worker thread
    worker_thread_ = std::thread([this]() {
        for (auto& pos : positions_) {
            if (should_stop_) break;

            // 3. Move turntable
            UpdateState(ControllerState::MovingTurntable);
            MoveTurntableToPosition(pos);

            // 4. Wait for settling
            UpdateState(ControllerState::WaitingForSettle);
            std::this_thread::sleep_for(std::chrono::milliseconds(settle_time_ms_));

            // 5. Additional capture delay
            std::this_thread::sleep_for(std::chrono::milliseconds(capture_delay_ms_));

            // 6. Capture images
            UpdateState(ControllerState::Capturing);
            CaptureAtCurrentPosition();

            // 7. Update progress
            current_position_index_++;
            NotifyProgress(current_position_index_, "Captured position " + pos.label);
        }

        UpdateState(ControllerState::Completed);
        NotificationSound::Play(NotificationSound::Type::Success);
    });
}
```

### Position Movement

```cpp
bool MoveTurntableToPosition(const CapturePosition& pos) {
    // 1. Calculate relative movement
    float rotation_delta = CalculateRotationAngle(pos.azimuth);
    float tilt_delta = CalculateTiltAngle(pos.elevation);

    // 2. Set speeds
    bluetooth_manager_->SetRotationSpeed(device_id, rotation_speed_);
    bluetooth_manager_->SetTiltSpeed(device_id, tilt_speed_);

    // 3. Move rotation
    if (abs(rotation_delta) > 0.1f) {
        bluetooth_manager_->RotateTurntable(device_id, rotation_delta);
        current_azimuth_ = pos.azimuth;
    }

    // 4. Move tilt
    if (abs(tilt_delta) > 0.1f) {
        bluetooth_manager_->TiltTurntable(device_id, tilt_delta);
        current_elevation_ = pos.elevation;
    }

    return true;
}

float CalculateRotationAngle(float target_azimuth) const {
    float delta = target_azimuth - current_azimuth_;

    // Normalize to [-180, 180]
    while (delta > 180.0f) delta -= 360.0f;
    while (delta < -180.0f) delta += 360.0f;

    return delta;
}
```

### Camera Capture

```cpp
bool CaptureAtCurrentPosition() {
    auto session_path = session_manager_->GetNextCapturePath();

    // Capture from all connected cameras
    bool success = camera_manager_->CaptureAllCameras(
        session_path,
        sequential = true,
        delay_ms = 750
    );

    if (!success) {
        HandleError("Camera capture failed");
        return false;
    }

    session_manager_->RecordCapture(session_path);
    return true;
}
```

### Error Handling

```cpp
bool RetryOperation(std::function<bool()> operation, int max_retries = 3) {
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        if (operation()) return true;

        LogMessage("Retry " + std::to_string(attempt + 1) + "/" + std::to_string(max_retries));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return false;
}

void HandleError(const std::string& error_message) {
    LogMessage("[ERROR] " + error_message);
    UpdateState(ControllerState::Error);
    NotificationSound::Play(NotificationSound::Type::Error);

    // Attempt recovery
    if (error_message.contains("camera")) {
        camera_manager_->DisconnectAllCameras();
        camera_manager_->ConnectAllCameras();
    } else if (error_message.contains("bluetooth")) {
        bluetooth_manager_->DisconnectDevice(device_id);
        bluetooth_manager_->ConnectToDevice(device_id);
    }
}
```

## Capture Patterns

### Single Ring (360°)

```cpp
std::vector<CapturePosition> GenerateSingleRing(int steps, float elevation = 0.0f) {
    std::vector<CapturePosition> positions;
    float angle_step = 360.0f / steps;

    for (int i = 0; i < steps; ++i) {
        positions.push_back({
            .azimuth = i * angle_step,
            .elevation = elevation,
            .label = "azim_" + std::to_string(i * angle_step) + "_elev_" + std::to_string(elevation)
        });
    }

    return positions;
}

// Example: 12 views around equator
auto positions = GenerateSingleRing(12, 0.0f);
```

### Multi-Layer Hemisphere

```cpp
std::vector<CapturePosition> GenerateHemisphere(int azimuth_steps, std::vector<float> elevations) {
    std::vector<CapturePosition> positions;
    float azimuth_step = 360.0f / azimuth_steps;

    for (float elev : elevations) {
        for (int i = 0; i < azimuth_steps; ++i) {
            positions.push_back({
                .azimuth = i * azimuth_step,
                .elevation = elev,
                .label = "azim_" + std::to_string(i * azimuth_step) + "_elev_" + std::to_string(elev)
            });
        }
    }

    return positions;
}

// Example: 3 rings at different elevations
auto positions = GenerateHemisphere(12, {-20.0f, 0.0f, 20.0f});
// Total: 36 positions (12 × 3)
```

### Custom Pattern

```cpp
std::vector<CapturePosition> GenerateCustomPattern() {
    return {
        {0.0f, 0.0f, "front"},
        {90.0f, 0.0f, "right"},
        {180.0f, 0.0f, "back"},
        {270.0f, 0.0f, "left"},
        {0.0f, 30.0f, "front_top"},
        {180.0f, 30.0f, "back_top"}
    };
}
```

## Timing Configuration

**Settle time** (default: 2000ms):
- Time to wait after turntable motion
- Allows mechanical vibrations to dampen
- Longer for heavy objects

**Capture delay** (default: 500ms):
- Additional delay before camera trigger
- Ensures complete mechanical settling
- Can be shorter for rigid objects

**Max capture wait** (default: 30s):
- Timeout for camera operations
- Prevents infinite hangs
- Increase for slow cameras

**Example configuration:**
```cpp
controller.SetSettleTime(2.5f);          // 2.5 seconds
controller.SetCaptureDelay(0.3f);        // 300ms
controller.SetRotationSpeed(60.0f);      // Slower rotation
controller.SetTiltSpeed(15.0f);          // Slower tilt
controller.SetMaxCaptureWaitTime(45.0f); // 45 second timeout
```

## Progress Tracking

**Progress callback:**
```cpp
controller.SetProgressCallback([](int position, const std::string& status) {
    std::cout << "Position " << position << ": " << status << std::endl;
});

// Called at each step:
// Position 1: Moving to azim_30_elev_0
// Position 1: Capturing...
// Position 1: Captured
// Position 2: Moving to azim_60_elev_0
// ...
```

**Progress percentage:**
```cpp
float GetProgress() const {
    if (positions_.empty()) return 0.0f;
    return (float)current_position_index_ / positions_.size();
}

// In GUI:
ImGui::ProgressBar(controller.GetProgress());
```

## Integration with Other Modules

### Session Management

```cpp
// Before starting sequence
session_manager_->StartNewSession("object_name");

// During capture
session_manager_->RecordCapture(image_path);

// After completion
session_manager_->EndCurrentSession();
```

### Camera Manager

```cpp
// Sequential capture with delay
camera_manager_->CaptureAllCameras(
    session_path,
    sequential = true,
    delay_ms = 750
);

// Images saved as:
// session_path/camera1_capture_001.tiff
// session_path/camera2_capture_001.tiff
// ...
```

### Bluetooth Manager

```cpp
// Set speeds before sequence
bluetooth_manager_->SetRotationSpeed(device_id, 70.0f);
bluetooth_manager_->SetTiltSpeed(device_id, 20.0f);

// Move to position
bluetooth_manager_->RotateTurntable(device_id, angle);
bluetooth_manager_->TiltTurntable(device_id, tilt);

// Emergency stop
bluetooth_manager_->StopRotation(device_id);
```

## Extension Points

### Custom Capture Logic

Override capture behavior:

```cpp
class CustomCaptureController : public AutomatedCaptureController {
protected:
    bool CaptureAtCurrentPosition() override {
        // Pre-capture custom logic (e.g., lighting adjustment)
        SetLighting(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Standard capture
        bool success = AutomatedCaptureController::CaptureAtCurrentPosition();

        // Post-capture custom logic
        SetLighting(false);

        return success;
    }
};
```

### Position Optimization

Optimize movement path to minimize turntable motion:

```cpp
std::vector<CapturePosition> OptimizePositions(std::vector<CapturePosition> positions) {
    // Sort by elevation first (minimize tilt changes)
    std::sort(positions.begin(), positions.end(), [](const auto& a, const auto& b) {
        if (abs(a.elevation - b.elevation) > 0.1f) {
            return a.elevation < b.elevation;
        }
        return a.azimuth < b.azimuth;
    });

    return positions;
}
```

### Multi-Object Capture

Capture multiple objects in sequence:

```cpp
for (auto& object_name : object_list) {
    // Start new session
    session_manager_->StartNewSession(object_name);

    // Generate positions
    auto positions = GenerateSingleRing(24);

    // Capture
    controller.StartSequence(positions, bluetooth, camera, session);

    // Wait for completion
    while (controller.IsActive()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // End session
    session_manager_->EndCurrentSession();

    // Prompt to change object
    ShowMessage("Replace object and press Continue");
    WaitForUserInput();
}
```

## Common Issues

**Problem:** Blurry images
**Solution:** Increase settle time, reduce rotation speed, ensure stable mounting

**Problem:** Sequence stops mid-capture
**Solution:** Check camera/bluetooth timeouts, verify network stability, increase max wait time

**Problem:** Turntable position drift
**Solution:** Use `ReturnToZero()` between sequences, calibrate turntable, check command accuracy

**Problem:** Uneven image spacing
**Solution:** Verify angle calculations, ensure positions are sorted correctly, check for rounding errors
