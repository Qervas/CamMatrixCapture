#include "AutomatedCaptureController.hpp"
#include "../gui/AutomatedCapturePanel.hpp" // For CapturePosition definition
#include "../bluetooth/BluetoothManager.hpp"
#include "../bluetooth/BluetoothCommands.hpp"
#include "../hardware/CameraManager.hpp"
#include "../utils/SessionManager.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <future>
#include <chrono>

namespace SaperaCapturePro {

AutomatedCaptureController::AutomatedCaptureController() = default;

AutomatedCaptureController::~AutomatedCaptureController() {
    StopSequence();
}

void AutomatedCaptureController::StartSequence(const std::vector<CapturePosition>& positions,
                                             BluetoothManager* bluetooth,
                                             CameraManager* camera,
                                             SessionManager* session) {
    if (is_active_.load()) {
        LogMessage("[AUTOMATED] Sequence already active, stopping current sequence first");
        StopSequence();
    }
    
    // Validate inputs
    if (positions.empty()) {
        LogMessage("[AUTOMATED] ERROR: No capture positions provided");
        return;
    }
    
    if (!bluetooth || !camera || !session) {
        LogMessage("[AUTOMATED] ERROR: Missing required system references");
        return;
    }
    
    if (!bluetooth->IsConnected()) {
        LogMessage("[AUTOMATED] ERROR: Bluetooth turntable not connected");
        return;
    }
    
    if (camera->GetConnectedCount() == 0) {
        LogMessage("[AUTOMATED] ERROR: No cameras connected");
        return;
    }
    
    // Initialize sequence
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        positions_ = positions;
        bluetooth_manager_ = bluetooth;
        camera_manager_ = camera;
        session_manager_ = session;
        current_position_index_.store(0);
        is_active_.store(true);
        is_paused_.store(false);
        should_stop_.store(false);
        current_azimuth_ = 0.0f;
        current_elevation_ = 0.0f;
    }
    
    UpdateState(ControllerState::MovingTurntable);
    
    // Start worker thread
    worker_thread_ = std::thread(&AutomatedCaptureController::WorkerThreadFunction, this);
    
    LogMessage("[AUTOMATED] Started capture sequence with " + std::to_string(positions.size()) + " positions");
    NotifyProgress(0, "Starting sequence...");
}

void AutomatedCaptureController::PauseSequence() {
    if (is_active_.load()) {
        is_paused_.store(!is_paused_.load());
        LogMessage(is_paused_.load() ? "[AUTOMATED] Sequence paused" : "[AUTOMATED] Sequence resumed");
    }
}

void AutomatedCaptureController::ResumeSequence() {
    if (is_active_.load() && is_paused_.load()) {
        is_paused_.store(false);
        LogMessage("[AUTOMATED] Sequence resumed");
    }
}

void AutomatedCaptureController::StopSequence() {
    if (is_active_.load()) {
        should_stop_.store(true);
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        
        is_active_.store(false);
        is_paused_.store(false);
        UpdateState(ControllerState::Idle);
        
        LogMessage("[AUTOMATED] Sequence stopped");
        NotifyProgress(current_position_index_.load(), "Stopped");
    }
}

float AutomatedCaptureController::GetProgress() const {
    if (positions_.empty()) return 0.0f;
    return static_cast<float>(current_position_index_.load()) / static_cast<float>(positions_.size());
}

void AutomatedCaptureController::WorkerThreadFunction() {
    try {
        // Set initial turntable speeds
        if (bluetooth_manager_) {
            auto connected_devices = bluetooth_manager_->GetConnectedDevices();
            if (!connected_devices.empty()) {
                const std::string& device_id = connected_devices[0];
                bluetooth_manager_->SendCommand(device_id, BluetoothCommands::SetRotationSpeed(rotation_speed_));
                SleepMs(100);
                bluetooth_manager_->SendCommand(device_id, BluetoothCommands::SetTiltSpeed(tilt_speed_));
                SleepMs(100);
                
                // Return to zero position
                LogMessage("[AUTOMATED] Returning turntable to zero position");
                bluetooth_manager_->SendCommand(device_id, BluetoothCommands::ReturnToZero());
                bluetooth_manager_->SendCommand(device_id, BluetoothCommands::TiltToZero());
                SleepMs(3000); // Wait for movement to complete
            }
        }
        
        // Process each position
        while (current_position_index_.load() < positions_.size() && 
               !should_stop_.load() && 
               is_active_.load()) {
            
            // Handle pause
            while (is_paused_.load() && !should_stop_.load()) {
                SleepMs(100);
            }
            
            if (should_stop_.load()) break;
            
            ProcessNextPosition();
            
            // Move to next position
            current_position_index_.fetch_add(1);
        }
        
        // Sequence completed
        if (!should_stop_.load() && is_active_.load()) {
            UpdateState(ControllerState::Completed);
            LogMessage("[AUTOMATED] Capture sequence completed successfully!");
            NotifyProgress(current_position_index_.load(), "Completed");
            
            // Return to zero position
            if (bluetooth_manager_) {
                auto connected_devices = bluetooth_manager_->GetConnectedDevices();
                if (!connected_devices.empty()) {
                    const std::string& device_id = connected_devices[0];
                    bluetooth_manager_->SendCommand(device_id, BluetoothCommands::ReturnToZero());
                    bluetooth_manager_->SendCommand(device_id, BluetoothCommands::TiltToZero());
                }
            }
        }
        
    } catch (const std::exception& e) {
        HandleError("Worker thread exception: " + std::string(e.what()));
    } catch (...) {
        HandleError("Unknown worker thread exception");
    }
    
    is_active_.store(false);
    UpdateState(ControllerState::Idle);
}

void AutomatedCaptureController::ProcessNextPosition() {
    int position_index = current_position_index_.load();
    if (position_index >= positions_.size()) return;
    
    const auto& position = positions_[position_index];
    
    LogMessage("[AUTOMATED] Processing position " + std::to_string(position_index + 1) + 
               "/" + std::to_string(positions_.size()) + 
               " (Az: " + std::to_string(position.azimuth) + "°, El: " + std::to_string(position.elevation) + "°)");
    
    NotifyProgress(position_index, "Moving to position...");
    
    // Move turntable to position
    UpdateState(ControllerState::MovingTurntable);
    if (!MoveTurntableToPosition(position)) {
        HandleError("Failed to move turntable to position");
        return;
    }
    
    // Wait for settling
    UpdateState(ControllerState::WaitingForSettle);
    NotifyProgress(position_index, "Waiting for turntable to settle...");
    if (!WaitForTurntableSettle()) {
        if (should_stop_.load()) return;
        LogMessage("[AUTOMATED] WARNING: Settle time expired, proceeding with capture");
    }
    
    // Capture images
    UpdateState(ControllerState::Capturing);
    NotifyProgress(position_index, "Capturing images...");
    if (!CaptureAtCurrentPosition()) {
        HandleError("Failed to capture images at position");
        return;
    }
    
    // Images captured successfully - ready for next position
    UpdateState(ControllerState::Processing);
    NotifyProgress(position_index, "Images captured successfully");
    
    // No need for additional delay - camera completion signal ensures images are saved
    LogMessage("[AUTOMATED] Position " + std::to_string(position_index + 1) + " completed");
}

bool AutomatedCaptureController::MoveTurntableToPosition(const CapturePosition& position) {
    if (!bluetooth_manager_) return false;
    
    // Calculate required movements
    float rotation_delta = CalculateRotationAngle(position.azimuth);
    float tilt_delta = CalculateTiltAngle(position.elevation);
    
    bool success = true;
    
    // Apply rotation if needed
    if (std::abs(rotation_delta) > 1.0f) { // 1 degree threshold
        std::string rotation_cmd = BluetoothCommands::RotateByAngle(rotation_delta);
        success &= RetryOperation([this, rotation_cmd]() {
            auto connected_devices = bluetooth_manager_->GetConnectedDevices();
            if (!connected_devices.empty()) {
                return bluetooth_manager_->SendCommand(connected_devices[0], rotation_cmd);
            }
            return false;
        });
        
        if (success) {
            current_azimuth_ = position.azimuth;
            LogMessage("[AUTOMATED] Rotated by " + std::to_string(rotation_delta) + "° to azimuth " + std::to_string(current_azimuth_) + "°");
        }
    }
    
    // Apply tilt if needed
    if (std::abs(tilt_delta) > 1.0f) { // 1 degree threshold
        std::string tilt_cmd = BluetoothCommands::TiltByAngle(tilt_delta);
        success &= RetryOperation([this, tilt_cmd]() {
            auto connected_devices = bluetooth_manager_->GetConnectedDevices();
            if (!connected_devices.empty()) {
                return bluetooth_manager_->SendCommand(connected_devices[0], tilt_cmd);
            }
            return false;
        });
        
        if (success) {
            current_elevation_ = position.elevation;
            LogMessage("[AUTOMATED] Tilted by " + std::to_string(tilt_delta) + "° to elevation " + std::to_string(current_elevation_) + "°");
        }
    }
    
    return success;
}

bool AutomatedCaptureController::CaptureAtCurrentPosition() {
    if (!camera_manager_ || !session_manager_) return false;
    
    // Get session path for capture
    auto* session = session_manager_->GetCurrentSession();
    if (!session) {
        LogMessage("[AUTOMATED] ERROR: No active session for capture");
        return false;
    }
    
    std::string capture_path = session->GetNextCapturePath();
    
    // Capture images from all cameras
    bool capture_success = false;
    
    return RetryOperation([this, capture_path, &capture_success]() {
        // Use async capture to prevent blocking
        std::promise<bool> capture_promise;
        std::future<bool> capture_future = capture_promise.get_future();
        
        camera_manager_->CaptureAllCamerasAsync(capture_path, true, 750, 
            [&capture_promise, this](const std::string& msg) {
                LogMessage(msg);
                if (msg.find("Capture completed") != std::string::npos) {
                    capture_promise.set_value(true);
                } else if (msg.find("ERROR") != std::string::npos || msg.find("Failed") != std::string::npos) {
                    capture_promise.set_value(false);
                }
            });
        
        // Wait for capture completion with intelligent timeout
        int timeout_seconds = static_cast<int>(max_capture_wait_seconds_);
        auto status = capture_future.wait_for(std::chrono::seconds(timeout_seconds));
        if (status == std::future_status::timeout) {
            LogMessage("[AUTOMATED] WARNING: Capture timeout after " + std::to_string(timeout_seconds) + " seconds");
            return false;
        }
        
        capture_success = capture_future.get();
        return capture_success;
    });
}

float AutomatedCaptureController::CalculateRotationAngle(float target_azimuth) const {
    float delta = target_azimuth - current_azimuth_;
    
    // Normalize to [-180, 180] range for shortest rotation
    while (delta > 180.0f) delta -= 360.0f;
    while (delta < -180.0f) delta += 360.0f;
    
    return delta;
}

float AutomatedCaptureController::CalculateTiltAngle(float target_elevation) const {
    return target_elevation - current_elevation_;
}

bool AutomatedCaptureController::WaitForTurntableSettle() {
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::milliseconds(settle_time_ms_);
    
    while (std::chrono::steady_clock::now() < end_time) {
        if (should_stop_.load()) return false;
        
        // Handle pause during settle
        while (is_paused_.load() && !should_stop_.load()) {
            SleepMs(100);
            // Extend settle time while paused
            end_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(settle_time_ms_);
        }
        
        SleepMs(100);
    }
    
    return true;
}

bool AutomatedCaptureController::RetryOperation(std::function<bool()> operation, int max_retries) {
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        if (should_stop_.load()) return false;
        
        try {
            if (operation()) {
                return true;
            }
        } catch (const std::exception& e) {
            LogMessage("[AUTOMATED] Operation attempt " + std::to_string(attempt + 1) + 
                      " failed: " + std::string(e.what()));
        }
        
        if (attempt < max_retries - 1) {
            LogMessage("[AUTOMATED] Retrying operation (attempt " + std::to_string(attempt + 2) + 
                      "/" + std::to_string(max_retries) + ")");
            SleepMs(1000); // Wait 1 second between retries
        }
    }
    
    return false;
}

void AutomatedCaptureController::SleepMs(int milliseconds) {
    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::milliseconds(milliseconds);
    
    while (std::chrono::steady_clock::now() < end) {
        if (should_stop_.load()) return;
        
        // Handle pause
        while (is_paused_.load() && !should_stop_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // Extend sleep time while paused
            end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(std::min(100, milliseconds)));
        milliseconds -= 100;
        if (milliseconds <= 0) break;
    }
}

void AutomatedCaptureController::UpdateState(ControllerState new_state) {
    current_state_.store(new_state);
    
    // Log state changes
    std::string state_name;
    switch (new_state) {
        case ControllerState::Idle: state_name = "Idle"; break;
        case ControllerState::MovingTurntable: state_name = "Moving Turntable"; break;
        case ControllerState::WaitingForSettle: state_name = "Waiting for Settle"; break;
        case ControllerState::Capturing: state_name = "Capturing"; break;
        case ControllerState::Processing: state_name = "Processing"; break;
        case ControllerState::Completed: state_name = "Completed"; break;
        case ControllerState::Error: state_name = "Error"; break;
    }
    
    LogMessage("[AUTOMATED] State: " + state_name);
}

void AutomatedCaptureController::LogMessage(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
}

void AutomatedCaptureController::NotifyProgress(int position_index, const std::string& status) {
    if (progress_callback_) {
        progress_callback_(position_index, status);
    }
}

void AutomatedCaptureController::HandleError(const std::string& error_message) {
    UpdateState(ControllerState::Error);
    LogMessage("[AUTOMATED] ERROR: " + error_message);
    
    // Stop the sequence on error
    should_stop_.store(true);
    is_active_.store(false);
}

} // namespace SaperaCapturePro