#pragma once

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include "BluetoothDevice.hpp"

namespace SaperaCapturePro {

class TurntableController {
public:
    TurntableController();
    ~TurntableController();

    // Connection management
    bool ConnectToTurntable(std::shared_ptr<BluetoothDevice> device);
    void Disconnect();
    bool IsConnected() const { return connected_ && bluetooth_device_; }

    // Rotation control with completion detection
    bool RotateAndWait(float degrees, int timeout_ms = 30000);
    bool RotateStepsAndWait(int steps_per_360, int current_step, int timeout_ms = 30000);
    
    // Status queries
    float GetCurrentAngle();
    bool IsRotating();
    void StopRotation();
    
    // Settings
    void SetRotationSpeed(float speed_rpm);
    void SetTiltSpeed(float speed);
    
    // Callbacks
    void SetOnRotationComplete(std::function<void()> callback) {
        on_rotation_complete_ = callback;
    }
    
    void SetOnAngleChanged(std::function<void(float)> callback) {
        on_angle_changed_ = callback;
    }
    
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }

private:
    std::shared_ptr<BluetoothDevice> bluetooth_device_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> is_rotating_{false};
    std::atomic<bool> stop_monitoring_{false};
    
    std::thread monitoring_thread_;
    
    // Callbacks
    std::function<void()> on_rotation_complete_;
    std::function<void(float)> on_angle_changed_;
    std::function<void(const std::string&)> log_callback_;
    
    // Helper methods
    bool SendCommand(const std::string& command);
    std::string SendCommandAndRead(const std::string& command);
    float ParseAngleResponse(const std::string& response);
    void StartAngleMonitoring();
    void StopAngleMonitoring();
    void LogMessage(const std::string& message);
    
    // Wait for angle to reach target with timeout
    bool WaitForAngle(float target_angle, float tolerance, int timeout_ms);
    bool WaitForRotationComplete(int timeout_ms);
};

} // namespace SaperaCapturePro
