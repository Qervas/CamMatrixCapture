#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <imgui.h>
#include <glad/glad.h>
#include "../utils/SessionManager.hpp"

// Forward declare to avoid circular includes
namespace SaperaCapturePro {
    struct CapturePosition {
        float azimuth;
        float elevation;
        int cameraIndex;
        bool captured;
        std::string imagePath;
        
        CapturePosition() : azimuth(0.0f), elevation(0.0f), cameraIndex(0), captured(false) {}
        CapturePosition(float az, float el, int cam = 0) 
            : azimuth(az), elevation(el), cameraIndex(cam), captured(false) {}
    };
}

namespace SaperaCapturePro {

// Forward declarations
class BluetoothManager;
class CameraManager;
class HemisphereRenderer;
class AutomatedCaptureController;

enum class CaptureMode {
    Sequential,  // Simple latitude bands
    Uniform      // Optimized spherical distribution
};

struct AutomatedSession {
    CaptureMode mode = CaptureMode::Sequential;
    std::vector<CapturePosition> positions;
    int currentIndex = 0;
    float totalProgress = 0.0f;
    bool isActive = false;
    bool isPaused = false;
    
    // Configuration
    float minElevation = -30.0f;  // Minimum elevation angle
    float maxElevation = 45.0f;   // Maximum elevation angle
    int stepsPerRotation = 12;    // Steps for full 360 rotation
    float captureDelay = 2.0f;    // Delay between captures (seconds)
    
    void Reset() {
        positions.clear();
        currentIndex = 0;
        totalProgress = 0.0f;
        isActive = false;
        isPaused = false;
    }
};

class AutomatedCapturePanel {
public:
    AutomatedCapturePanel();
    ~AutomatedCapturePanel();

    void Initialize();
    void Render(bool* p_open);
    void Shutdown();

    // External system connections
    void SetBluetoothManager(BluetoothManager* bluetooth) { bluetooth_manager_ = bluetooth; }
    void SetCameraManager(CameraManager* camera) { camera_manager_ = camera; }
    void SetSessionManager(SessionManager* session) { session_manager_ = session; }
    
    // Automation control
    void StartAutomatedCapture();
    void PauseAutomatedCapture();
    void StopAutomatedCapture();
    
    bool IsAutomating() const { return session_.isActive; }

private:
    // System references
    BluetoothManager* bluetooth_manager_ = nullptr;
    CameraManager* camera_manager_ = nullptr;
    SessionManager* session_manager_ = nullptr;
    
    // Core components
    std::unique_ptr<HemisphereRenderer> hemisphere_renderer_;
    std::unique_ptr<AutomatedCaptureController> capture_controller_;
    
    // UI state
    AutomatedSession session_;
    bool show_advanced_settings_ = false;
    int selected_position_ = -1;
    
    // 3D viewport
    GLuint framebuffer_id_ = 0;
    GLuint color_texture_ = 0;
    GLuint depth_texture_ = 0;
    int viewport_width_ = 400;
    int viewport_height_ = 300;
    
    // Camera controls
    float view_azimuth_ = 0.0f;
    float view_elevation_ = 30.0f;
    float view_distance_ = 5.0f;
    bool mouse_captured_ = false;
    ImVec2 last_mouse_pos_;
    
    // Internal methods
    void RenderControlSection();
    void RenderVisualizationSection();
    void RenderProgressSection();
    void RenderAdvancedSettings();
    
    void InitializeFramebuffer();
    void ResizeFramebuffer(int width, int height);
    void RenderToFramebuffer();
    void HandleViewportInteraction();
    
    void GenerateCapturePositions();
    void GenerateSequentialPositions();
    void GenerateUniformPositions();
    
    void UpdateProgress();
    
    // Turntable test methods
    void TestRotation(float angle, float speed);
    void TestTilt(float angle, float speed);
    void TestReturnToZero();
    void TestTiltToZero();
    void EmergencyStopTurntable();
    
    // Callbacks
    std::function<void(const std::string&)> log_callback_;
    
public:
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }
    
private:
    void LogMessage(const std::string& message) {
        if (log_callback_) {
            log_callback_(message);
        }
    }
};

} // namespace SaperaCapturePro