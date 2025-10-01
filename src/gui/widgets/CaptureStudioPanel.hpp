#pragma once

#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <imgui.h>
#include "../../utils/SessionManager.hpp"
#include "../../hardware/CameraManager.hpp"
#include "../../bluetooth/BluetoothManager.hpp"
#include "../../bluetooth/TurntableController.hpp"
#include "../../utils/NotificationSounds.hpp"
#include "FileExplorerWidget.hpp"

namespace SaperaCapturePro {

enum class CaptureMode {
    Manual,
    Automated
};

enum class SequenceStep {
    Idle,           // Not running
    Initializing,   // Setting up sequence
    RotatingAndWaiting, // Rotating turntable and waiting for completion
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
    std::unique_ptr<TurntableController> turntable_controller_;
    
    // Session UI state
    char session_name_input_[256] = "";
    std::unique_ptr<FileExplorerWidget> file_explorer_widget_;
    
    // UI state
    CaptureMode current_mode_ = CaptureMode::Manual;
    bool is_capturing_ = false;
    float widget_height_ = 250.0f;
    
    // Manual capture state
    int manual_capture_count_ = 1;
    char manual_capture_name_[256] = "";
    bool single_camera_mode_ = false;
    std::string selected_camera_id_ = "";
    int selected_camera_index_ = -1;
    
    // Automated capture state
    int auto_capture_count_ = 36;
    float rotation_angle_ = 10.0f;
    float capture_delay_ = 2.0f;
    bool edit_by_captures_ = true; // true = edit captures, false = edit angle
    float turntable_speed_ = 70.0f; // Speed setting (seconds for 360°)
    int current_capture_index_ = 0;
    std::chrono::steady_clock::time_point last_capture_time_;
    std::chrono::steady_clock::time_point capture_start_time_;  // For async capture timing
    bool auto_sequence_active_ = false;
    bool waiting_for_capture_completion_ = false;  // For automated sequence sync

    // Background sequence threading
    std::thread sequence_thread_;
    std::atomic<bool> sequence_stop_requested_{false};
    std::atomic<bool> sequence_pause_requested_{false};
    std::mutex sequence_state_mutex_;

    // Thread-safe sequence state
    std::atomic<int> thread_safe_current_index_{0};
    std::atomic<bool> thread_safe_sequence_active_{false};
    std::string thread_safe_current_step_description_;
    std::mutex step_description_mutex_;
    
    // Turntable state
    std::atomic<bool> turntable_rotation_complete_{true};
    
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
    void RenderManualCapture();
    void RenderAutomatedCapture();
    void RenderCaptureControls();
    // void RenderSharedComponents(); // No longer needed - components rendered inline
    void RenderSessionControl();
    std::string GenerateDefaultSessionName() const;

    // Apple-inspired UI methods
    void RenderInitializingState();
    void RenderSetupMode();
    void RenderSessionSetup();
    void RenderCaptureMode();
    void RenderCompactModeSelector();
    void RenderManualMode();
    void RenderAutomatedMode();
    void RenderActiveSequenceControls();
    void RenderStatusCard(const char* title, bool connected, const std::string& status, ImVec2 size);

    // Capture logic
    void StartManualCapture();
    void StartSingleCameraCapture();
    void StartAutomatedSequence();
    void StopAutomatedSequence();
    void UpdateAutomatedSequence();
    void PerformSingleCapture(const std::string& capture_name = "");
    void PerformSyncCapture(const std::string& capture_name = "");  // For automated sequences

    // Background sequence methods
    void RunAutomatedSequenceInBackground();
    void UpdateSequenceStateFromThread();
    void StopSequenceThread();
    
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
    void RotateTurntableAndWait(float degrees); // Rotates and waits for completion
    bool IsTurntableConnected() const;
    
    // Turntable monitoring
    void WaitForTurntableRotation(float rotation_angle);
    bool IsTurntableRotationComplete() const { return turntable_rotation_complete_; }
    
    // Hemisphere visualization
    void RenderHemisphereVisualization(float size = 200.0f);
    void DrawHemisphereGrid(float size);
    void DrawCameraPositions(float size);
    
    // Modern Layout System
    struct LayoutCard {
        std::string title;
        std::function<void()> render_content;
        ImVec2 min_size;
        ImVec2 preferred_size;
        bool collapsible;
        bool collapsed;
        float priority; // Higher = more important, gets more space
    };
    
    void RenderCard(LayoutCard& card, ImVec2 available_size);
    void RenderModernLayout();
    std::vector<LayoutCard> CreateLayoutCards();
    ImVec2 CalculateOptimalCardSize(const LayoutCard& card, ImVec2 available_space, int cards_in_row);
    
    // Utility methods
    void LogMessage(const std::string& message);
    std::string GenerateCaptureFilename() const;
    bool ValidateSystemState() const;
};

} // namespace SaperaCapturePro