#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "../utils/SessionManager.hpp"

namespace SaperaCapturePro {

// Forward declarations  
class BluetoothManager;
class CameraManager;

// Forward declaration - definition is in AutomatedCapturePanel.hpp
struct CapturePosition;

enum class ControllerState {
    Idle,
    MovingTurntable,
    WaitingForSettle,
    Capturing,
    Processing,
    Completed,
    Error
};

class AutomatedCaptureController {
public:
    AutomatedCaptureController();
    ~AutomatedCaptureController();
    
    // Control methods
    void StartSequence(const std::vector<CapturePosition>& positions,
                      BluetoothManager* bluetooth,
                      CameraManager* camera,
                      SessionManager* session);
    
    void PauseSequence();
    void ResumeSequence();
    void StopSequence();
    
    // Status queries
    bool IsActive() const { return is_active_.load(); }
    bool IsPaused() const { return is_paused_.load(); }
    ControllerState GetState() const { return current_state_.load(); }
    int GetCurrentPositionIndex() const { return current_position_index_.load(); }
    float GetProgress() const;
    
    // Configuration
    void SetSettleTime(float seconds) { settle_time_ms_ = static_cast<int>(seconds * 1000); }
    void SetCaptureDelay(float seconds) { capture_delay_ms_ = static_cast<int>(seconds * 1000); }
    void SetRotationSpeed(float speed) { rotation_speed_ = speed; }
    void SetTiltSpeed(float speed) { tilt_speed_ = speed; }
    
    // Callbacks
    void SetProgressCallback(std::function<void(int, const std::string&)> callback) {
        progress_callback_ = callback;
    }
    
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }

private:
    // System references
    BluetoothManager* bluetooth_manager_ = nullptr;
    CameraManager* camera_manager_ = nullptr;
    SessionManager* session_manager_ = nullptr;
    
    // Sequence data
    std::vector<CapturePosition> positions_;
    std::atomic<int> current_position_index_{0};
    std::atomic<bool> is_active_{false};
    std::atomic<bool> is_paused_{false};
    std::atomic<bool> should_stop_{false};
    std::atomic<ControllerState> current_state_{ControllerState::Idle};
    
    // Timing configuration
    int settle_time_ms_ = 2000;    // Time to wait after turntable movement
    int capture_delay_ms_ = 500;   // Additional delay before capture
    float rotation_speed_ = 70.0f; // Turntable rotation speed
    float tilt_speed_ = 20.0f;     // Turntable tilt speed
    
    // Threading
    std::thread worker_thread_;
    std::mutex state_mutex_;
    
    // Current position tracking
    float current_azimuth_ = 0.0f;
    float current_elevation_ = 0.0f;
    
    // Callbacks
    std::function<void(int, const std::string&)> progress_callback_;
    std::function<void(const std::string&)> log_callback_;
    
    // Worker thread methods
    void WorkerThreadFunction();
    void ProcessNextPosition();
    bool MoveTurntableToPosition(const CapturePosition& position);
    bool CaptureAtCurrentPosition();
    void UpdateState(ControllerState new_state);
    void LogMessage(const std::string& message);
    void NotifyProgress(int position_index, const std::string& status);
    
    // Utility methods
    float CalculateRotationAngle(float target_azimuth) const;
    float CalculateTiltAngle(float target_elevation) const;
    void SleepMs(int milliseconds);
    bool WaitForTurntableSettle();
    
    // Error handling
    void HandleError(const std::string& error_message);
    bool RetryOperation(std::function<bool()> operation, int max_retries = 3);
};

} // namespace SaperaCapturePro