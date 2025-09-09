#include "TurntableController.hpp"
#include <regex>
#include <sstream>
#include <cmath>
#include <thread>
#include <future>

namespace SaperaCapturePro {

TurntableController::TurntableController() = default;

TurntableController::~TurntableController() {
    Disconnect();
}

bool TurntableController::ConnectToTurntable(std::shared_ptr<BluetoothDevice> device) {
    if (!device) {
        LogMessage("[TURNTABLE] Invalid device provided");
        return false;
    }
    
    bluetooth_device_ = device;
    connected_ = true;
    
    LogMessage("[TURNTABLE] Connected to turntable device");
    return true;
}

void TurntableController::Disconnect() {
    if (connected_) {
        StopAngleMonitoring();
        StopRotation();
        connected_ = false;
        bluetooth_device_.reset();
        LogMessage("[TURNTABLE] Disconnected from turntable");
    }
}

bool TurntableController::RotateAndWait(float degrees, int timeout_ms) {
    if (!IsConnected()) {
        LogMessage("[TURNTABLE] ERROR: Not connected to turntable");
        return false;
    }
    
    LogMessage("[TURNTABLE] Starting rotation: " + std::to_string(degrees) + " degrees");
    
    // Get current angle before rotation
    float start_angle = GetCurrentAngle();
    if (start_angle < 0) {
        LogMessage("[TURNTABLE] ERROR: Could not get current angle");
        return false;
    }
    
    // Calculate target angle (handle wraparound at 360)
    float target_angle = start_angle + degrees;
    while (target_angle >= 360.0f) target_angle -= 360.0f;
    while (target_angle < 0.0f) target_angle += 360.0f;
    
    LogMessage("[TURNTABLE] Target angle: " + std::to_string(target_angle) + " degrees");
    
    // Start rotation
    std::string command = "+CT,TURNANGLE=" + std::to_string(degrees) + ";";
    if (!SendCommand(command)) {
        LogMessage("[TURNTABLE] ERROR: Failed to send rotation command");
        return false;
    }
    
    is_rotating_ = true;
    
    // Wait for rotation to complete
    bool success = WaitForAngle(target_angle, 2.0f, timeout_ms); // 2 degree tolerance
    
    is_rotating_ = false;
    
    if (success) {
        LogMessage("[TURNTABLE] Rotation completed successfully");
        if (on_rotation_complete_) {
            on_rotation_complete_();
        }
    } else {
        LogMessage("[TURNTABLE] ERROR: Rotation timed out or failed");
    }
    
    return success;
}

bool TurntableController::RotateStepsAndWait(int steps_per_360, int current_step, int timeout_ms) {
    float degrees_per_step = 360.0f / static_cast<float>(steps_per_360);
    return RotateAndWait(degrees_per_step, timeout_ms);
}

float TurntableController::GetCurrentAngle() {
    if (!IsConnected()) {
        return -1.0f;
    }
    
    std::string response = SendCommandAndRead("+QT,CHANGEANGLE;");
    if (response.empty()) {
        LogMessage("[TURNTABLE] ERROR: No response to angle query");
        return -1.0f;
    }
    
    float angle = ParseAngleResponse(response);
    if (angle >= 0 && on_angle_changed_) {
        on_angle_changed_(angle);
    }
    
    return angle;
}

bool TurntableController::IsRotating() {
    return is_rotating_;
}

void TurntableController::StopRotation() {
    if (IsConnected()) {
        SendCommand("+CT,STOP;");
        is_rotating_ = false;
        LogMessage("[TURNTABLE] Rotation stopped");
    }
}

void TurntableController::SetRotationSpeed(float speed_rpm) {
    if (IsConnected()) {
        // Convert RPM to turntable speed units (based on DAT.html range 35.64-131)
        float turntable_speed = 35.64f + (speed_rpm / 10.0f) * (131.0f - 35.64f);
        turntable_speed = std::max(35.64f, std::min(131.0f, turntable_speed));
        
        std::string command = "+CT,TURNSPEED=" + std::to_string(turntable_speed) + ";";
        SendCommand(command);
        LogMessage("[TURNTABLE] Rotation speed set to " + std::to_string(turntable_speed));
    }
}

void TurntableController::SetTiltSpeed(float speed) {
    if (IsConnected()) {
        // Clamp to turntable tilt speed range (9-35 from DAT.html)
        float tilt_speed = std::max(9.0f, std::min(35.0f, speed));
        
        std::string command = "+CR,TILTSPEED=" + std::to_string(tilt_speed) + ";";
        SendCommand(command);
        LogMessage("[TURNTABLE] Tilt speed set to " + std::to_string(tilt_speed));
    }
}

bool TurntableController::SendCommand(const std::string& command) {
    if (!bluetooth_device_) {
        return false;
    }
    
    try {
        return bluetooth_device_->SendCommand(command);
    } catch (const std::exception& e) {
        LogMessage("[TURNTABLE] ERROR sending command: " + std::string(e.what()));
        return false;
    }
}

std::string TurntableController::SendCommandAndRead(const std::string& command) {
    if (!bluetooth_device_) {
        return "";
    }
    
    try {
        // Use a promise to make the async callback synchronous
        std::promise<std::string> response_promise;
        auto response_future = response_promise.get_future();
        
        bool sent = bluetooth_device_->SendCommandWithResponse(command, 
            [&response_promise](const std::string& response) {
                response_promise.set_value(response);
            });
        
        if (sent) {
            // Wait for response with timeout
            auto status = response_future.wait_for(std::chrono::milliseconds(1000));
            if (status == std::future_status::ready) {
                return response_future.get();
            } else {
                LogMessage("[TURNTABLE] WARNING: Command response timeout");
                return "";
            }
        }
    } catch (const std::exception& e) {
        LogMessage("[TURNTABLE] ERROR in command/read: " + std::string(e.what()));
    }
    
    return "";
}

float TurntableController::ParseAngleResponse(const std::string& response) {
    // Parse response like "+DATA=123.45;" 
    std::regex angle_regex(R"(\+DATA=([0-9.-]+);)");
    std::smatch match;
    
    if (std::regex_search(response, match, angle_regex)) {
        try {
            float angle = std::stof(match[1].str());
            // Ensure angle is in 0-360 range
            while (angle < 0) angle += 360.0f;
            while (angle >= 360.0f) angle -= 360.0f;
            return angle;
        } catch (const std::exception& e) {
            LogMessage("[TURNTABLE] ERROR parsing angle: " + std::string(e.what()));
        }
    }
    
    LogMessage("[TURNTABLE] ERROR: Invalid angle response format: " + response);
    return -1.0f;
}

bool TurntableController::WaitForAngle(float target_angle, float tolerance, int timeout_ms) {
    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout_ms);
    
    LogMessage("[TURNTABLE] Waiting for angle " + std::to_string(target_angle) + 
               " ±" + std::to_string(tolerance) + " degrees");
    
    int poll_count = 0;
    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        if (current_time - start_time > timeout_duration) {
            LogMessage("[TURNTABLE] ERROR: Timeout waiting for rotation completion");
            return false;
        }
        
        float current_angle = GetCurrentAngle();
        if (current_angle < 0) {
            LogMessage("[TURNTABLE] ERROR: Could not read current angle during wait");
            return false;
        }
        
        // Calculate angular difference (handle wraparound)
        float angle_diff = std::abs(current_angle - target_angle);
        if (angle_diff > 180.0f) {
            angle_diff = 360.0f - angle_diff; // Shorter path around circle
        }
        
        if (angle_diff <= tolerance) {
            LogMessage("[TURNTABLE] Target angle reached: " + std::to_string(current_angle) + 
                       " degrees (polls: " + std::to_string(poll_count) + ")");
            return true;
        }
        
        // Log progress every 20 polls to avoid spam
        if (++poll_count % 20 == 0) {
            LogMessage("[TURNTABLE] Current angle: " + std::to_string(current_angle) + 
                       ", diff: " + std::to_string(angle_diff) + " degrees");
        }
        
        // Poll every 40ms (similar to DAT.html pattern)
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
}

bool TurntableController::WaitForRotationComplete(int timeout_ms) {
    // This is a simpler version that just waits for the rotation to stabilize
    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout_ms);
    
    float previous_angle = GetCurrentAngle();
    if (previous_angle < 0) return false;
    
    int stable_count = 0;
    const int required_stable_readings = 5; // Need 5 consecutive stable readings
    const float stability_tolerance = 0.5f; // Within 0.5 degrees
    
    while (true) {
        auto current_time = std::chrono::steady_clock::now();
        if (current_time - start_time > timeout_duration) {
            return false;
        }
        
        float current_angle = GetCurrentAngle();
        if (current_angle < 0) return false;
        
        float angle_diff = std::abs(current_angle - previous_angle);
        if (angle_diff > 180.0f) {
            angle_diff = 360.0f - angle_diff;
        }
        
        if (angle_diff <= stability_tolerance) {
            stable_count++;
            if (stable_count >= required_stable_readings) {
                LogMessage("[TURNTABLE] Rotation stabilized at " + std::to_string(current_angle) + " degrees");
                return true;
            }
        } else {
            stable_count = 0; // Reset if movement detected
        }
        
        previous_angle = current_angle;
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }
}

void TurntableController::StartAngleMonitoring() {
    if (monitoring_thread_.joinable()) {
        StopAngleMonitoring();
    }
    
    stop_monitoring_ = false;
    monitoring_thread_ = std::thread([this]() {
        while (!stop_monitoring_) {
            if (IsConnected()) {
                GetCurrentAngle(); // This will trigger the callback
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });
}

void TurntableController::StopAngleMonitoring() {
    stop_monitoring_ = true;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
}

void TurntableController::LogMessage(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
}

} // namespace SaperaCapturePro
