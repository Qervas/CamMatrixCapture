#pragma once

#include <string>
#include <sstream>

namespace SaperaCapturePro {
namespace BluetoothCommands {

// Turntable rotation commands
inline std::string RotateByAngle(float angle) {
    std::stringstream cmd;
    cmd << "+CT,TURNANGLE=" << angle << ";";
    return cmd.str();
}

inline std::string RotateLeft360() {
    return "+CT,TURNANGLE=-360;";
}

inline std::string RotateRight360() {
    return "+CT,TURNANGLE=360;";
}

inline std::string RotateContinuousLeft() {
    return "+CT,TURNCONTINUE=-1;";
}

inline std::string RotateContinuousRight() {
    return "+CT,TURNCONTINUE=1;";
}

inline std::string StopRotation() {
    return "+CT,STOP;";
}

inline std::string ReturnToZero() {
    return "+CT,TOZERO;";
}

inline std::string SetRotationSpeed(float speed) {
    // Speed range: 35.64 - 131
    if (speed < 35.64f) speed = 35.64f;
    if (speed > 131.0f) speed = 131.0f;
    
    std::stringstream cmd;
    cmd << "+CT,TURNSPEED=" << speed << ";";
    return cmd.str();
}

// Turntable tilt commands
inline std::string TiltByAngle(float angle) {
    std::stringstream cmd;
    cmd << "+CR,TILTVALUE=" << angle << ";";
    return cmd.str();
}

inline std::string TiltLeft30() {
    return "+CR,TILTVALUE=-30;";
}

inline std::string TiltRight30() {
    return "+CR,TILTVALUE=30;";
}

inline std::string StopTilt() {
    return "+CR,STOP;";
}

inline std::string TiltToZero() {
    return "+CR,TILTVALUE=0;";
}

inline std::string SetTiltSpeed(float speed) {
    // Speed range: 9 - 35
    if (speed < 9.0f) speed = 9.0f;
    if (speed > 35.0f) speed = 35.0f;
    
    std::stringstream cmd;
    cmd << "+CR,TILTSPEED=" << speed << ";";
    return cmd.str();
}

// Query commands
inline std::string QueryStatus() {
    return "+QT,CHANGEANGLE;";
}

inline std::string QueryVersion() {
    return "+QT,VERSION;";
}

// Preset positions for camera capture
namespace Presets {
    // Common rotation angles for 3D scanning
    constexpr float ROTATION_STEP_15 = 15.0f;
    constexpr float ROTATION_STEP_30 = 30.0f;
    constexpr float ROTATION_STEP_45 = 45.0f;
    constexpr float ROTATION_STEP_60 = 60.0f;
    constexpr float ROTATION_STEP_90 = 90.0f;
    
    // Common tilt angles
    constexpr float TILT_UP_15 = 15.0f;
    constexpr float TILT_UP_30 = 30.0f;
    constexpr float TILT_DOWN_15 = -15.0f;
    constexpr float TILT_DOWN_30 = -30.0f;
    
    // Speed presets
    constexpr float ROTATION_SPEED_SLOW = 40.0f;
    constexpr float ROTATION_SPEED_MEDIUM = 70.0f;
    constexpr float ROTATION_SPEED_FAST = 100.0f;
    
    constexpr float TILT_SPEED_SLOW = 10.0f;
    constexpr float TILT_SPEED_MEDIUM = 20.0f;
    constexpr float TILT_SPEED_FAST = 30.0f;
}

// Helper functions for automated capture sequences
struct CaptureSequence {
    int steps;           // Number of rotation steps
    float anglePerStep;  // Degrees per step
    float tiltAngle;     // Tilt angle for this sequence
    float rotationSpeed; // Rotation speed
    float tiltSpeed;     // Tilt speed
    int delayMs;         // Delay between steps in milliseconds
    
    CaptureSequence() 
        : steps(12)
        , anglePerStep(30.0f)
        , tiltAngle(0.0f)
        , rotationSpeed(Presets::ROTATION_SPEED_MEDIUM)
        , tiltSpeed(Presets::TILT_SPEED_MEDIUM)
        , delayMs(1000) {}
    
    CaptureSequence(int s, float a, float t = 0.0f) 
        : steps(s)
        , anglePerStep(a)
        , tiltAngle(t)
        , rotationSpeed(Presets::ROTATION_SPEED_MEDIUM)
        , tiltSpeed(Presets::TILT_SPEED_MEDIUM)
        , delayMs(1000) {}
};

// Common capture sequences
namespace Sequences {
    inline CaptureSequence Basic360() {
        return CaptureSequence(12, 30.0f, 0.0f);
    }
    
    inline CaptureSequence Detailed360() {
        return CaptureSequence(24, 15.0f, 0.0f);
    }
    
    inline CaptureSequence Quick360() {
        return CaptureSequence(8, 45.0f, 0.0f);
    }
    
    inline CaptureSequence MultiAngle360() {
        // This would be used multiple times with different tilt angles
        return CaptureSequence(12, 30.0f, 0.0f);
    }
}

} // namespace BluetoothCommands
} // namespace SaperaCapturePro