#pragma once

#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <imgui.h>
#include "../../utils/SessionManager.hpp"
#include "../../hardware/CameraManager.hpp"
#include "../../bluetooth/BluetoothManager.hpp"
#include "SessionWidget.hpp"
#include "FileExplorerWidget.hpp"

namespace SaperaCapturePro {

enum class CaptureMode {
    Quick,
    Automated,
    Advanced
};

enum class SequenceStep {
    Idle,           // Not running
    Initializing,   // Setting up sequence
    Rotating,       // Rotating turntable
    WaitingToSettle,// Waiting for turntable to settle
    Capturing,      // Taking photos
    Processing,     // Processing/saving images
    WaitingForNext, // Delay before next capture
    Completing,     // Finalizing sequence
    Paused          // User paused the sequence
};

class CaptureStudioPanel {
public:
    CaptureStudioPanel();
    ~CaptureStudioPanel();

    void Initialize(CameraManager* camera_manager, BluetoothManager* bluetooth_manager, SessionManager* session_manager);
    void Render();
    void Shutdown();

    // State queries
    bool IsCapturing() const { return is_capturing_; }
    CaptureMode GetCurrentMode() const { return current_mode_; }
    
    // Callbacks
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }

private:
    // System references
    CameraManager* camera_manager_ = nullptr;
    BluetoothManager* bluetooth_manager_ = nullptr;
    SessionManager* session_manager_ = nullptr;
    
    // Shared UI components
    std::unique_ptr<SessionWidget> session_widget_;
    std::unique_ptr<FileExplorerWidget> file_explorer_widget_;
    
    // UI state
    CaptureMode current_mode_ = CaptureMode::Quick;
    bool is_capturing_ = false;
    float widget_height_ = 250.0f;
    
    // Quick capture state
    int quick_capture_count_ = 1;
    char quick_capture_name_[256] = "";
    
    // Automated capture state
    int auto_capture_count_ = 36;
    float rotation_angle_ = 10.0f;
    float capture_delay_ = 2.0f;
    int current_capture_index_ = 0;
    std::chrono::steady_clock::time_point last_capture_time_;
    bool auto_sequence_active_ = false;
    
    // Pauseable automation state
    SequenceStep current_step_ = SequenceStep::Idle;
    bool sequence_paused_ = false;
    std::chrono::steady_clock::time_point step_start_time_;
    float step_duration_seconds_ = 0.0f;
    std::string current_step_description_;
    float step_progress_ = 0.0f;
    
    // Advanced capture state
    struct AdvancedSettings {
        bool enable_exposure_bracketing = false;
        std::vector<float> exposure_stops = {-1.0f, 0.0f, 1.0f};
        bool enable_focus_stacking = false;
        int focus_steps = 5;
        float focus_step_size = 0.1f;
        bool enable_lighting_variation = false;
    } advanced_settings_;
    
    // Callbacks
    std::function<void(const std::string&)> log_callback_;
    
    // UI rendering methods
    void RenderModeSelector();
    void RenderQuickCapture();
    void RenderAutomatedCapture();
    void RenderAdvancedCapture();
    void RenderCaptureControls();
    void RenderSharedComponents();
    
    // Capture logic
    void StartQuickCapture();
    void StartAutomatedSequence();
    void StopAutomatedSequence();
    void UpdateAutomatedSequence();
    void PerformSingleCapture(const std::string& capture_name = "");
    
    // Pauseable automation
    void PauseSequence();
    void ResumeSequence();
    void AdvanceToNextStep();
    void SetCurrentStep(SequenceStep step, const std::string& description, float duration_seconds = 0.0f);
    void UpdateStepProgress();
    std::string GetStepName(SequenceStep step) const;
    void RenderStepIndicator();
    
    // Turntable control
    void RotateTurntable(float degrees);
    bool IsTurntableConnected() const;
    
    // Utility methods
    void LogMessage(const std::string& message);
    std::string GenerateCaptureFilename() const;
    bool ValidateSystemState() const;
};

} // namespace SaperaCapturePro