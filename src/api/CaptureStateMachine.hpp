/**
 * CaptureStateMachine.hpp
 *
 * State machine for managing capture-rotation cycle.
 * Ensures clean state transitions and prevents misalignment between operations.
 */

#pragma once

#include <atomic>
#include <mutex>
#include <chrono>
#include <string>
#include <functional>

namespace SaperaCapturePro {

/**
 * Capture cycle states
 */
enum class CaptureState : int {
    Idle = 0,           // Ready for next operation
    Capturing = 1,      // Cameras are actively capturing
    Rotating = 2,       // Turntable is rotating
    Settling = 3,       // Waiting for turntable to settle
    Error = 4           // Error occurred, needs reset
};

/**
 * State transition events
 */
enum class CaptureEvent {
    StartCapture,       // Begin camera capture
    CaptureComplete,    // All cameras finished capturing
    CaptureFailed,      // Camera capture failed
    StartRotation,      // Begin turntable rotation
    RotationComplete,   // Turntable reached target
    RotationFailed,     // Rotation failed or timed out
    SettlingComplete,   // Turntable has settled
    Reset,              // Reset to idle state
    Stop                // User requested stop
};

/**
 * State machine for capture-rotation cycle
 * Thread-safe implementation using atomic operations and mutex
 */
class CaptureStateMachine {
public:
    using LogCallback = std::function<void(const std::string&)>;
    using StateChangeCallback = std::function<void(CaptureState, CaptureState)>;

    CaptureStateMachine();
    ~CaptureStateMachine() = default;

    // Delete copy/move
    CaptureStateMachine(const CaptureStateMachine&) = delete;
    CaptureStateMachine& operator=(const CaptureStateMachine&) = delete;

    /**
     * Process an event and transition to the appropriate state
     * Returns true if transition was valid, false if rejected
     */
    bool ProcessEvent(CaptureEvent event);

    /**
     * Get current state (thread-safe)
     */
    CaptureState GetState() const { return current_state_.load(); }

    /**
     * Get state as integer for API compatibility
     */
    int GetStateInt() const { return static_cast<int>(current_state_.load()); }

    /**
     * Check if in a specific state
     */
    bool IsInState(CaptureState state) const { return current_state_.load() == state; }

    /**
     * Check if capture cycle is active (not idle or error)
     */
    bool IsActive() const {
        CaptureState s = current_state_.load();
        return s != CaptureState::Idle && s != CaptureState::Error;
    }

    /**
     * Get current position in capture sequence
     */
    int GetCurrentPosition() const { return current_position_.load(); }

    /**
     * Set current position (called by capture loop)
     */
    void SetCurrentPosition(int pos) { current_position_ = pos; }

    /**
     * Get total positions in capture sequence
     */
    int GetTotalPositions() const { return total_positions_.load(); }

    /**
     * Set total positions (called when starting capture)
     */
    void SetTotalPositions(int total) { total_positions_ = total; }

    // Timing methods
    void StartPhaseTimer();
    int GetPhaseElapsedMs() const;
    void RecordCaptureTime(int ms);
    void RecordRotationTime(int ms);
    int GetTotalCaptureTimeMs() const { return total_capture_time_ms_.load(); }
    int GetTotalRotationTimeMs() const { return total_rotation_time_ms_.load(); }
    int GetCurrentCaptureElapsedMs() const { return current_capture_elapsed_ms_.load(); }
    int GetCurrentRotationElapsedMs() const { return current_rotation_elapsed_ms_.load(); }

    /**
     * Reset all timing counters (called at start of new session)
     */
    void ResetTimers();

    /**
     * Reset state machine to idle
     */
    void Reset();

    /**
     * Set callbacks
     */
    void SetLogCallback(LogCallback cb) { log_callback_ = cb; }
    void SetStateChangeCallback(StateChangeCallback cb) { state_change_callback_ = cb; }

    /**
     * Get state name for logging
     */
    static const char* GetStateName(CaptureState state);
    static const char* GetEventName(CaptureEvent event);

private:
    std::atomic<CaptureState> current_state_{CaptureState::Idle};
    std::atomic<int> current_position_{0};
    std::atomic<int> total_positions_{0};

    // Timing
    std::chrono::steady_clock::time_point phase_start_time_;
    std::atomic<int> total_capture_time_ms_{0};
    std::atomic<int> total_rotation_time_ms_{0};
    std::atomic<int> current_capture_elapsed_ms_{0};
    std::atomic<int> current_rotation_elapsed_ms_{0};

    mutable std::mutex state_mutex_;
    LogCallback log_callback_;
    StateChangeCallback state_change_callback_;

    void Log(const std::string& message);
    bool TransitionTo(CaptureState newState, const std::string& reason);
    bool IsValidTransition(CaptureState from, CaptureEvent event, CaptureState& to);
};

} // namespace SaperaCapturePro
