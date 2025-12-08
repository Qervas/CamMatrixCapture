/**
 * CaptureStateMachine.cpp
 *
 * Implementation of capture-rotation state machine.
 */

#include "CaptureStateMachine.hpp"

namespace SaperaCapturePro {

CaptureStateMachine::CaptureStateMachine() {
    phase_start_time_ = std::chrono::steady_clock::now();
}

const char* CaptureStateMachine::GetStateName(CaptureState state) {
    switch (state) {
        case CaptureState::Idle: return "Idle";
        case CaptureState::Capturing: return "Capturing";
        case CaptureState::Rotating: return "Rotating";
        case CaptureState::Settling: return "Settling";
        case CaptureState::Error: return "Error";
        default: return "Unknown";
    }
}

const char* CaptureStateMachine::GetEventName(CaptureEvent event) {
    switch (event) {
        case CaptureEvent::StartCapture: return "StartCapture";
        case CaptureEvent::CaptureComplete: return "CaptureComplete";
        case CaptureEvent::CaptureFailed: return "CaptureFailed";
        case CaptureEvent::StartRotation: return "StartRotation";
        case CaptureEvent::RotationComplete: return "RotationComplete";
        case CaptureEvent::RotationFailed: return "RotationFailed";
        case CaptureEvent::SettlingComplete: return "SettlingComplete";
        case CaptureEvent::Reset: return "Reset";
        case CaptureEvent::Stop: return "Stop";
        default: return "Unknown";
    }
}

void CaptureStateMachine::Log(const std::string& message) {
    if (log_callback_) {
        log_callback_("[SM] " + message);
    }
}

bool CaptureStateMachine::IsValidTransition(CaptureState from, CaptureEvent event, CaptureState& to) {
    // State transition table
    // Each state has specific valid transitions

    switch (from) {
        case CaptureState::Idle:
            switch (event) {
                case CaptureEvent::StartCapture:
                    to = CaptureState::Capturing;
                    return true;
                case CaptureEvent::StartRotation:
                    to = CaptureState::Rotating;  // Allow rotation from Idle
                    return true;
                case CaptureEvent::Reset:
                    to = CaptureState::Idle;
                    return true;
                default:
                    return false;
            }

        case CaptureState::Capturing:
            switch (event) {
                case CaptureEvent::CaptureComplete:
                    to = CaptureState::Idle;  // Will transition to Rotating if needed
                    return true;
                case CaptureEvent::CaptureFailed:
                    to = CaptureState::Error;
                    return true;
                case CaptureEvent::StartRotation:
                    to = CaptureState::Rotating;  // Direct transition from Capturing to Rotating
                    return true;
                case CaptureEvent::Stop:
                    to = CaptureState::Idle;
                    return true;
                case CaptureEvent::Reset:
                    to = CaptureState::Idle;
                    return true;
                default:
                    return false;
            }

        case CaptureState::Rotating:
            switch (event) {
                case CaptureEvent::RotationComplete:
                    to = CaptureState::Settling;
                    return true;
                case CaptureEvent::RotationFailed:
                    to = CaptureState::Settling;  // Still settle even if rotation had issues
                    return true;
                case CaptureEvent::Stop:
                    to = CaptureState::Idle;
                    return true;
                case CaptureEvent::Reset:
                    to = CaptureState::Idle;
                    return true;
                default:
                    return false;
            }

        case CaptureState::Settling:
            switch (event) {
                case CaptureEvent::SettlingComplete:
                    to = CaptureState::Idle;  // Ready for next capture
                    return true;
                case CaptureEvent::StartCapture:
                    to = CaptureState::Capturing;  // Can start capture directly from settling
                    return true;
                case CaptureEvent::Stop:
                    to = CaptureState::Idle;
                    return true;
                case CaptureEvent::Reset:
                    to = CaptureState::Idle;
                    return true;
                default:
                    return false;
            }

        case CaptureState::Error:
            switch (event) {
                case CaptureEvent::Reset:
                    to = CaptureState::Idle;
                    return true;
                case CaptureEvent::Stop:
                    to = CaptureState::Idle;
                    return true;
                default:
                    return false;
            }

        default:
            return false;
    }
}

bool CaptureStateMachine::TransitionTo(CaptureState newState, const std::string& reason) {
    CaptureState oldState = current_state_.load();

    if (oldState == newState) {
        return true;  // Already in this state
    }

    current_state_ = newState;

    Log("State: " + std::string(GetStateName(oldState)) + " -> " +
        std::string(GetStateName(newState)) + " (" + reason + ")");

    // Notify callback
    if (state_change_callback_) {
        state_change_callback_(oldState, newState);
    }

    return true;
}

bool CaptureStateMachine::ProcessEvent(CaptureEvent event) {
    std::lock_guard<std::mutex> lock(state_mutex_);

    CaptureState currentState = current_state_.load();
    CaptureState newState;

    if (!IsValidTransition(currentState, event, newState)) {
        Log("REJECTED: Event " + std::string(GetEventName(event)) +
            " invalid in state " + std::string(GetStateName(currentState)));
        return false;
    }

    return TransitionTo(newState, GetEventName(event));
}

void CaptureStateMachine::StartPhaseTimer() {
    phase_start_time_ = std::chrono::steady_clock::now();
}

int CaptureStateMachine::GetPhaseElapsedMs() const {
    auto now = std::chrono::steady_clock::now();
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - phase_start_time_).count()
    );
}

void CaptureStateMachine::RecordCaptureTime(int ms) {
    current_capture_elapsed_ms_ = ms;
    total_capture_time_ms_ = total_capture_time_ms_.load() + ms;
}

void CaptureStateMachine::RecordRotationTime(int ms) {
    current_rotation_elapsed_ms_ = ms;
    total_rotation_time_ms_ = total_rotation_time_ms_.load() + ms;
}

void CaptureStateMachine::ResetTimers() {
    total_capture_time_ms_ = 0;
    total_rotation_time_ms_ = 0;
    current_capture_elapsed_ms_ = 0;
    current_rotation_elapsed_ms_ = 0;
    phase_start_time_ = std::chrono::steady_clock::now();
}

void CaptureStateMachine::Reset() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    CaptureState oldState = current_state_.load();
    current_state_ = CaptureState::Idle;
    current_position_ = 0;

    Log("RESET: " + std::string(GetStateName(oldState)) + " -> Idle");

    if (state_change_callback_) {
        state_change_callback_(oldState, CaptureState::Idle);
    }
}

} // namespace SaperaCapturePro
