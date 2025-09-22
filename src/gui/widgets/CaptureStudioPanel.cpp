#include "CaptureStudioPanel.hpp"
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace SaperaCapturePro {

CaptureStudioPanel::CaptureStudioPanel() = default;

CaptureStudioPanel::~CaptureStudioPanel() {
    Shutdown();
}

void CaptureStudioPanel::Initialize(CameraManager* camera_manager, BluetoothManager* bluetooth_manager, SessionManager* session_manager) {
    camera_manager_ = camera_manager;
    bluetooth_manager_ = bluetooth_manager;
    session_manager_ = session_manager;
    
    // Session control state initialized
    
    file_explorer_widget_ = std::make_unique<FileExplorerWidget>();
    file_explorer_widget_->Initialize(); // Initialize GDI+ for TIFF loading
    file_explorer_widget_->SetHeight(200.0f);
    file_explorer_widget_->SetShowPreview(true);
    file_explorer_widget_->SetLogCallback([this](const std::string& msg) { LogMessage(msg); });
    
    // Initialize turntable controller
    turntable_controller_ = std::make_unique<TurntableController>();
    turntable_controller_->SetLogCallback([this](const std::string& msg) { LogMessage(msg); });
    turntable_controller_->SetOnRotationComplete([this]() {
        LogMessage("[STUDIO] Turntable rotation completed - ready for capture");
    });

    // Initialize notification sounds
    NotificationSounds::Instance().Initialize();
    NotificationSounds::Instance().SetLogCallback([this](const std::string& msg) { LogMessage(msg); });

    LogMessage("[STUDIO] Capture Studio Panel initialized");
}

void CaptureStudioPanel::Shutdown() {
    if (auto_sequence_active_) {
        StopAutomatedSequence();
    }

    // Stop background sequence thread
    StopSequenceThread();

    file_explorer_widget_.reset();
    
    // Shutdown turntable controller
    if (turntable_controller_) {
        turntable_controller_->Disconnect();
        turntable_controller_.reset();
    }
    
    camera_manager_ = nullptr;
    bluetooth_manager_ = nullptr;
    session_manager_ = nullptr;
}

void CaptureStudioPanel::Render() {
    if (!ImGui::Begin("🎬 Capture Studio", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    if (!camera_manager_ || !session_manager_) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "❌ System not initialized");
        ImGui::End();
        return;
    }

    if (auto_sequence_active_) {
        UpdateSequenceStateFromThread();
        UpdateAutomatedSequence();
    }

    // Check for capture timeout (30 seconds)
    if (is_capturing_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - capture_start_time_).count();
        if (elapsed > 30) {
            LogMessage("[STUDIO] ⚠ Capture timeout after 30 seconds, resetting capture state");
            is_capturing_ = false;
        }
    }

    const float em = ImGui::GetFontSize();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);

    // Card: Automated Sequence (primary)
    ImGui::BeginChild("cs_auto", ImVec2(0, 0), true);

    // Integrated session control at top of panel
    RenderSessionControl();
    ImGui::Separator();

    ImGui::Text("🔄 Automated Capture");
    ImGui::Separator();

    // Two-column layout
    ImGui::Columns(2, "auto_cols_new", false);
    ImGui::SetColumnWidth(0, 20.0f * em);

    // Left column: Parameters
    ImGui::Text("360° Mode");
    if (ImGui::RadioButton("By Total Captures", edit_by_captures_)) edit_by_captures_ = true;
    if (ImGui::RadioButton("By Angle Step", !edit_by_captures_)) edit_by_captures_ = false;
    ImGui::Spacing();

    if (edit_by_captures_) {
        ImGui::Text("Total Captures");
        if (ImGui::SliderInt("##AutoCountNew", &auto_capture_count_, 6, 360)) {
            rotation_angle_ = 360.0f / static_cast<float>(auto_capture_count_);
        }
        ImGui::Text("Angle Step: %.2f°", rotation_angle_);
    } else {
        ImGui::Text("Angle Step");
        if (ImGui::SliderFloat("##RotAngleNew", &rotation_angle_, 1.0f, 60.0f, "%.2f°")) {
            auto_capture_count_ = static_cast<int>(std::round(360.0f / rotation_angle_));
        }
        ImGui::Text("Total Captures: %d", auto_capture_count_);
    }

    ImGui::Text("Turntable Speed (s/360°)");
    ImGui::SliderFloat("##TurntableSpeedNew", &turntable_speed_, 35.64f, 131.0f, "%.1f");
    ImGui::Text("Capture Delay (s)");
    ImGui::SliderFloat("##CaptureDelayNew", &capture_delay_, 0.5f, 10.0f, "%.1f");

    ImGui::NextColumn();

    // Right column: Actions and status
    if (auto_sequence_active_) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Active");
        ImGui::Text("Progress: %d/%d", current_capture_index_, auto_capture_count_);

        // Overall progress bar
        float overall_progress = auto_capture_count_ > 0 ? (float)current_capture_index_ / (float)auto_capture_count_ : 0.0f;
        char progress_label[64];
        snprintf(progress_label, sizeof(progress_label), "%d/%d (%.1f%%)",
                current_capture_index_, auto_capture_count_, overall_progress * 100.0f);
        ImGui::ProgressBar(overall_progress, ImVec2(-1, 0), progress_label);

        RenderStepIndicator();
        if (ImGui::Button(sequence_paused_ ? "▶ Resume" : "⏸ Pause", ImVec2(-1, 2.5f * em))) {
            if (sequence_paused_) ResumeSequence(); else PauseSequence();
        }
        if (ImGui::Button("⏭ Next Step", ImVec2(-1, 2.5f * em))) {
            AdvanceToNextStep();
        }
        if (ImGui::Button("⏹ Stop", ImVec2(-1, 2.5f * em))) {
            StopAutomatedSequence();
        }
    } else {
        // Compact control section
        bool can_start = ValidateSystemState() && IsTurntableConnected() && !is_capturing_;

        // Status indicators
        if (is_capturing_) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "📸 Capturing...");
        } else {
            if (!camera_manager_->GetConnectedCount()) {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "⚠ No cameras");
            }
            if (!IsTurntableConnected()) {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "⚠ No turntable");
            }
        }

        // Compact start button
        if (!can_start) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        if (ImGui::Button("▶ Start", ImVec2(100, 30))) {
            if (can_start) StartAutomatedSequence();
        }
        if (!can_start) ImGui::PopStyleVar();

        ImGui::SameLine();
        float rotation_time = (rotation_angle_ * turntable_speed_) / 360.0f;
        float total_sequence_time = auto_capture_count_ * (capture_delay_ + rotation_time);
        ImGui::Text("Est: %.1f min", total_sequence_time / 60.0f);

        ImGui::Spacing();
        ImGui::Text("Per step: %.1fs rotation + %.1fs capture", rotation_time, capture_delay_);
    }

    ImGui::Columns(1);
    ImGui::EndChild();

    ImGui::PopStyleVar();

    // Render file explorer at the bottom
    CaptureSession* current_session = session_manager_->GetCurrentSession();
    file_explorer_widget_->Render(current_session);

    ImGui::End();
}

void CaptureStudioPanel::RenderModeSelector() {
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    
    if (ImGui::BeginTabBar("CaptureModes", tab_bar_flags)) {
        
        if (ImGui::BeginTabItem("⚡ Quick")) {
            current_mode_ = CaptureMode::Quick;
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("🔄 Automated")) {
            current_mode_ = CaptureMode::Automated;
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("⚙️ Advanced")) {
            current_mode_ = CaptureMode::Advanced;
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
}

void CaptureStudioPanel::RenderQuickCapture() {
    ImGui::Text("Quick single or multi-shot capture");
    
    ImGui::Columns(2, "quick_cols", false);
    ImGui::SetColumnWidth(0, 200);
    
    // Column 1: Settings
    ImGui::Text("Capture Count:");
    ImGui::SliderInt("##QuickCount", &quick_capture_count_, 1, 10);
    
    ImGui::Text("Custom Name:");
    ImGui::InputTextWithHint("##QuickName", "Optional custom name", 
                           quick_capture_name_, sizeof(quick_capture_name_));
    
    ImGui::NextColumn();
    
    // Column 2: Action
    ImGui::Spacing();
    
    bool can_capture = ValidateSystemState() && !is_capturing_;
    
    if (!can_capture) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }
    
    if (ImGui::Button("📸 Capture Now", ImVec2(-1, 40))) {
        if (can_capture) {
            StartQuickCapture();
        }
    }
    
    if (!can_capture) {
        ImGui::PopStyleVar();
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            if (!session_manager_->HasActiveSession()) {
                ImGui::Text("❌ No active session");
            } else if (camera_manager_->GetConnectedCount() == 0) {
                ImGui::Text("❌ No cameras connected");
            } else if (is_capturing_) {
                ImGui::Text("⏳ Capture in progress");
            }
            ImGui::EndTooltip();
        }
    }
    
    ImGui::Columns(1);
}

void CaptureStudioPanel::RenderAutomatedCapture() {
    ImGui::Text("Automated turntable sequence capture");
    
    ImGui::Columns(3, "auto_cols", false);
    ImGui::SetColumnWidth(0, 150);
    ImGui::SetColumnWidth(1, 150);
    
    // Column 1: 360° Capture Settings
    ImGui::Text("360° Capture Mode:");
    if (ImGui::RadioButton("Edit by Total Captures", edit_by_captures_)) {
        edit_by_captures_ = true;
    }
    if (ImGui::RadioButton("Edit by Angle Step", !edit_by_captures_)) {
        edit_by_captures_ = false;
    }
    
    ImGui::Spacing();
    
    if (edit_by_captures_) {
        // Edit total captures, calculate angle
        ImGui::Text("Total Captures:");
        if (ImGui::SliderInt("##AutoCount", &auto_capture_count_, 6, 360)) {
            rotation_angle_ = 360.0f / static_cast<float>(auto_capture_count_);
        }
        ImGui::Text("→ Angle Step: %.2f°", rotation_angle_);
    } else {
        // Edit angle, calculate captures
        ImGui::Text("Angle Step:");
        if (ImGui::SliderFloat("##RotAngle", &rotation_angle_, 1.0f, 60.0f, "%.2f°")) {
            auto_capture_count_ = static_cast<int>(std::round(360.0f / rotation_angle_));
        }
        ImGui::Text("→ Total Captures: %d", auto_capture_count_);
    }
    
    ImGui::NextColumn();
    
    // Column 2: Timing & Speed
    ImGui::Text("Turntable Speed:");
    ImGui::SliderFloat("##TurntableSpeed", &turntable_speed_, 35.64f, 131.0f, "%.1fs/360°");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Speed = seconds for full 360° rotation");
        ImGui::Text("Angular velocity: %.2f°/sec", 360.0f / turntable_speed_);
        ImGui::EndTooltip();
    }
    
    ImGui::Text("Capture Delay:");
    ImGui::SliderFloat("##CaptureDelay", &capture_delay_, 0.5f, 10.0f, "%.1fs");
    
    // Calculate timing estimates
    float rotation_time = (rotation_angle_ * turntable_speed_) / 360.0f;
    float total_capture_time = auto_capture_count_ * capture_delay_;
    float total_rotation_time = (auto_capture_count_ - 1) * rotation_time; // No rotation after last capture
    float total_sequence_time = total_capture_time + total_rotation_time;
    
    ImGui::Spacing();
    ImGui::Text("⏱ Time Estimates:");
    ImGui::Text("Per rotation: %.1fs", rotation_time);
    ImGui::Text("Total sequence: %.1fmin", total_sequence_time / 60.0f);
    
    ImGui::NextColumn();
    
    // Column 3: Controls & Status
    if (auto_sequence_active_) {
        if (sequence_paused_) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "⏸ Paused");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "🔄 Active");
        }
        ImGui::Text("Progress: %d/%d", current_capture_index_, auto_capture_count_);
        
        // Render step indicator
        RenderStepIndicator();
        
        ImGui::Spacing();
        
        // Pause/Resume button
        if (sequence_paused_) {
            if (ImGui::Button("▶ Resume", ImVec2(80, 0))) {
                ResumeSequence();
            }
        } else {
            if (ImGui::Button("⏸ Pause", ImVec2(80, 0))) {
                PauseSequence();
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("⏭ Next Step", ImVec2(80, 0))) {
            AdvanceToNextStep();
        }
        
        if (ImGui::Button("⏹ Stop Sequence", ImVec2(-1, 0))) {
            StopAutomatedSequence();
        }
    } else {
        bool can_start = ValidateSystemState() && IsTurntableConnected() && !is_capturing_;
        
        if (!can_start) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        }
        
        if (ImGui::Button("▶ Start Sequence", ImVec2(-1, 0))) {
            if (can_start) {
                StartAutomatedSequence();
            }
        }
        
        if (!can_start) {
            ImGui::PopStyleVar();
            
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                if (!IsTurntableConnected()) {
                    ImGui::Text("❌ Turntable not connected");
                } else if (!session_manager_->HasActiveSession()) {
                    ImGui::Text("❌ No active session");
                } else if (camera_manager_->GetConnectedCount() == 0) {
                    ImGui::Text("❌ No cameras connected");
                }
                ImGui::EndTooltip();
            }
        }
    }
    
    ImGui::Columns(1);
}

void CaptureStudioPanel::RenderAdvancedCapture() {
    ImGui::Text("Advanced capture techniques and settings");
    
    ImGui::Columns(2, "advanced_cols", false);
    ImGui::SetColumnWidth(0, 250);
    
    // Column 1: Advanced Settings
    ImGui::Text("📷 Exposure Bracketing");
    ImGui::Checkbox("Enable HDR Bracketing", &advanced_settings_.enable_exposure_bracketing);
    
    if (advanced_settings_.enable_exposure_bracketing) {
        ImGui::Text("Exposure Stops: -1, 0, +1 EV");
    }
    
    ImGui::Spacing();
    ImGui::Text("🔍 Focus Stacking");
    ImGui::Checkbox("Enable Focus Stack", &advanced_settings_.enable_focus_stacking);
    
    if (advanced_settings_.enable_focus_stacking) {
        ImGui::SliderInt("Focus Steps", &advanced_settings_.focus_steps, 3, 20);
        ImGui::SliderFloat("Step Size", &advanced_settings_.focus_step_size, 0.05f, 1.0f, "%.2f");
    }
    
    ImGui::Spacing();
    ImGui::Text("💡 Lighting Variation");
    ImGui::Checkbox("Enable Multi-Light", &advanced_settings_.enable_lighting_variation);
    
    ImGui::NextColumn();
    
    // Column 2: Preview & Execute
    ImGui::Text("📊 Capture Preview");
    
    int total_shots = 1;
    if (advanced_settings_.enable_exposure_bracketing) total_shots *= 3;
    if (advanced_settings_.enable_focus_stacking) total_shots *= advanced_settings_.focus_steps;
    if (advanced_settings_.enable_lighting_variation) total_shots *= 2;
    
    ImGui::Text("Total shots per position: %d", total_shots);
    ImGui::Text("Est. time per position: %.1fs", total_shots * 0.5f);
    
    ImGui::Spacing();
    
    bool can_capture = ValidateSystemState() && !is_capturing_;
    
    if (!can_capture) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }
    
    if (ImGui::Button("🎯 Advanced Capture", ImVec2(-1, 40))) {
        if (can_capture) {
            LogMessage("[STUDIO] Advanced capture not yet implemented");
        }
    }
    
    if (!can_capture) {
        ImGui::PopStyleVar();
    }
    
    ImGui::Columns(1);
}

void CaptureStudioPanel::RenderCaptureControls() {
    if (is_capturing_) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "⏳ Capturing...");
        ImGui::SameLine();
        
        // Simple progress indicator
        static float progress = 0.0f;
        progress += 0.02f;
        if (progress > 1.0f) progress = 0.0f;
        ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
    }
}

// RenderSharedComponents is no longer needed as components are rendered inline
// void CaptureStudioPanel::RenderSharedComponents() {
//     // Render integrated session control
//     RenderSessionControl();
//
//     // Render file explorer with current session
//     CaptureSession* current_session = session_manager_->GetCurrentSession();
//     file_explorer_widget_->Render(current_session);
// }

void CaptureStudioPanel::StartQuickCapture() {
    if (!ValidateSystemState()) return;
    
    std::string capture_name = std::string(quick_capture_name_);
    if (capture_name.empty()) {
        capture_name = GenerateCaptureFilename();
    }
    
    LogMessage("[STUDIO] Starting quick capture: " + capture_name + " (" + std::to_string(quick_capture_count_) + " shots)");
    
    for (int i = 0; i < quick_capture_count_; i++) {
        std::string shot_name = capture_name;
        if (quick_capture_count_ > 1) {
            shot_name += "_" + std::to_string(i + 1);
        }
        PerformSingleCapture(shot_name);
    }
}

void CaptureStudioPanel::StartAutomatedSequence() {
    if (!ValidateSystemState() || !IsTurntableConnected()) return;

    LogMessage("[STUDIO] Starting automated sequence: " + std::to_string(auto_capture_count_) + " captures");

    // Stop any existing sequence thread first
    StopSequenceThread();

    // Reset thread-safe state
    sequence_stop_requested_ = false;
    sequence_pause_requested_ = false;
    thread_safe_current_index_ = 0;
    thread_safe_sequence_active_ = true;

    // Update UI state
    auto_sequence_active_ = true;
    current_capture_index_ = 0;

    // Start the sequence in a background thread
    sequence_thread_ = std::thread(&CaptureStudioPanel::RunAutomatedSequenceInBackground, this);
}

void CaptureStudioPanel::StopAutomatedSequence() {
    if (!auto_sequence_active_) return;

    LogMessage("[STUDIO] Stopping automated sequence...");

    // Signal background thread to stop
    sequence_stop_requested_ = true;
    thread_safe_sequence_active_ = false;

    // Wait for thread to finish
    StopSequenceThread();

    auto_sequence_active_ = false;
    sequence_paused_ = false;
    SetCurrentStep(SequenceStep::Idle, "Sequence stopped", 0.0f);

    LogMessage("[STUDIO] Automated sequence stopped at capture " + std::to_string(current_capture_index_) + "/" + std::to_string(auto_capture_count_));
}

void CaptureStudioPanel::UpdateAutomatedSequence() {
    if (!auto_sequence_active_ || sequence_paused_) return;
    
    UpdateStepProgress();
    
    auto now = std::chrono::steady_clock::now();
    auto step_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - step_start_time_).count() / 1000.0f;
    
    switch (current_step_) {
        case SequenceStep::Initializing:
            if (step_elapsed >= step_duration_seconds_) {
                // Set rotation speed for smooth operation
                auto device_ids = bluetooth_manager_->GetConnectedDevices();
                if (!device_ids.empty()) {
                    // Use the configured turntable speed (speed = seconds for 360°)
                    std::string speed_command = "+CT,TURNSPEED=" + std::to_string(turntable_speed_) + ";";
                    bluetooth_manager_->SendCommand(device_ids[0], speed_command);
                    LogMessage("[STUDIO] Set turntable speed: " + std::to_string(turntable_speed_) + "s/360° (≈" + std::to_string(360.0f/turntable_speed_) + "°/s)");
                }
                
                // Move to first capture (no rotation needed)
                SetCurrentStep(SequenceStep::Capturing, "Taking capture " + std::to_string(current_capture_index_ + 1) + "/" + std::to_string(auto_capture_count_), 2.0f);
            }
            break;
            
        case SequenceStep::RotatingAndWaiting:
            // Check if turntable rotation is complete using real angle monitoring
            if (IsTurntableRotationComplete()) {
                LogMessage("[STUDIO] 🎯 Turntable rotation complete - proceeding to capture");
                SetCurrentStep(SequenceStep::Capturing, "Taking capture " + std::to_string(current_capture_index_ + 1) + "/" + std::to_string(auto_capture_count_), 2.0f);
            } else if (step_elapsed >= 60.0f) { // 60 second timeout (matches monitoring timeout)
                LogMessage("[STUDIO] WARNING: Turntable rotation timed out, proceeding with capture anyway");
                SetCurrentStep(SequenceStep::Capturing, "Taking capture " + std::to_string(current_capture_index_ + 1) + "/" + std::to_string(auto_capture_count_), 2.0f);
            }
            break;
            
        case SequenceStep::Capturing:
            if (step_elapsed >= step_duration_seconds_) {
                // Take the capture synchronously for automated sequence
                std::string capture_name = "auto_" + std::to_string(current_capture_index_ + 1);
                PerformSyncCapture(capture_name);

                SetCurrentStep(SequenceStep::Processing, "Processing and saving images...", 1.0f);
            }
            break;
            
        case SequenceStep::Processing:
            if (step_elapsed >= step_duration_seconds_) {
                current_capture_index_++;
                
                if (current_capture_index_ >= auto_capture_count_) {
                    // Sequence complete
                    SetCurrentStep(SequenceStep::Completing, "Finalizing capture sequence...", 1.0f);
                } else {
                    // More captures to go - start delay for next one
                    SetCurrentStep(SequenceStep::WaitingForNext, "Waiting before next capture... (" + std::to_string(capture_delay_) + "s)", capture_delay_);
                }
            }
            break;
            
        case SequenceStep::WaitingForNext:
            if (step_elapsed >= step_duration_seconds_) {
                // Time to rotate for next capture
                SetCurrentStep(SequenceStep::RotatingAndWaiting, "Rotating turntable " + std::to_string(rotation_angle_) + "° and waiting for completion...", 60.0f); // 60s timeout
                RotateTurntableAndWait(rotation_angle_);
            }
            break;
            
        case SequenceStep::Completing:
            if (step_elapsed >= step_duration_seconds_) {
                LogMessage("[STUDIO] Automated sequence completed successfully!");
                auto_sequence_active_ = false;
                sequence_paused_ = false;
                SetCurrentStep(SequenceStep::Idle, "Sequence complete", 0.0f);
            }
            break;
            
        default:
            break;
    }
}

void CaptureStudioPanel::PerformSingleCapture(const std::string& capture_name) {
    if (!ValidateSystemState()) return;

    // Check if already capturing
    if (is_capturing_) {
        LogMessage("[STUDIO] Capture already in progress, please wait...");
        return;
    }

    is_capturing_ = true;

    try {
        // Get the next capture path from session manager (this creates a directory)
        auto* session = session_manager_->GetCurrentSession();
        if (!session) {
            LogMessage("[STUDIO] No active session for capture");
            is_capturing_ = false;
            return;
        }

        std::string session_path = session->GetNextCapturePath();
        LogMessage("[STUDIO] 📸 Starting async capture to: " + session_path);

        // Use async camera manager to perform capture without blocking UI
        camera_manager_->CaptureAllCamerasAsync(session_path, true, 750,
            [this, session_path](const std::string& message) {
                // This callback runs on the capture thread, so we just log
                LogMessage(message);

                // Check if capture completed successfully
                if (message.find("🎬 Async capture completed successfully") != std::string::npos) {
                    // Record with session manager
                    session_manager_->RecordCapture(session_path);
                    LogMessage("[STUDIO] ✅ Capture recorded in session");
                    is_capturing_ = false;
                } else if (message.find("❌ Async capture failed") != std::string::npos) {
                    LogMessage("[STUDIO] ❌ Capture failed");
                    is_capturing_ = false;
                }
            }
        );

        // Start a timer to check capture completion
        capture_start_time_ = std::chrono::steady_clock::now();

    } catch (const std::exception& e) {
        LogMessage("[STUDIO] Capture error: " + std::string(e.what()));
        is_capturing_ = false;
    }
}

void CaptureStudioPanel::PerformSyncCapture(const std::string& capture_name) {
    if (!ValidateSystemState()) return;

    // For automated sequences, we need synchronous capture to maintain workflow order
    LogMessage("[STUDIO] 📸 Starting sync capture for automated sequence: " + capture_name);

    try {
        // Get the next capture path from session manager (this creates a directory)
        auto* session = session_manager_->GetCurrentSession();
        if (!session) {
            LogMessage("[STUDIO] No active session for capture");
            return;
        }

        std::string session_path = session->GetNextCapturePath();
        LogMessage("[STUDIO] Capturing to: " + session_path);

        // Use synchronous camera manager for automated sequences
        if (camera_manager_->CaptureAllCameras(session_path, true, 750)) {
            LogMessage("[STUDIO] ✅ Sync capture successful: " + session_path);

            // Record with session manager
            session_manager_->RecordCapture(session_path);
            LogMessage("[STUDIO] ✅ Capture recorded in session");
        } else {
            LogMessage("[STUDIO] ❌ Sync capture failed");
        }

    } catch (const std::exception& e) {
        LogMessage("[STUDIO] Sync capture error: " + std::string(e.what()));
    }
}

void CaptureStudioPanel::RotateTurntable(float degrees) {
    if (!IsTurntableConnected()) return;
    
    LogMessage("[STUDIO] Rotating turntable " + std::to_string(degrees) + "° (no wait)");
    
    // Use proper turntable rotation command format
    std::string command = "+CT,TURNANGLE=" + std::to_string(degrees) + ";";
    
    auto device_ids = bluetooth_manager_->GetConnectedDevices();
    if (!device_ids.empty()) {
        bool success = bluetooth_manager_->SendCommand(device_ids[0], command);
        if (success) {
            LogMessage("[STUDIO] Rotation command sent: " + command);
        } else {
            LogMessage("[STUDIO] Failed to send rotation command");
        }
    } else {
        LogMessage("[STUDIO] No bluetooth devices connected");
    }
}

void CaptureStudioPanel::RotateTurntableAndWait(float degrees) {
    if (!IsTurntableConnected()) {
        LogMessage("[STUDIO] ERROR: Turntable not available for rotation with wait");
        return;
    }
    
    auto device_ids = bluetooth_manager_->GetConnectedDevices();
    if (device_ids.empty()) {
        LogMessage("[STUDIO] ERROR: No bluetooth devices connected for turntable control");
        return;
    }
    
    LogMessage("[STUDIO] Starting monitored turntable rotation: " + std::to_string(degrees) + " degrees");
    
    // Get initial angle before rotation
    LogMessage("[STUDIO] Starting rotation: " + std::to_string(degrees) + "°");
    
    // Start the rotation
    RotateTurntable(degrees);
    
    // Start time-based rotation wait in separate thread
    std::thread rotation_monitor([this, degrees]() {
        WaitForTurntableRotation(degrees);
    });
    rotation_monitor.detach();
}

bool CaptureStudioPanel::IsTurntableConnected() const {
    if (!bluetooth_manager_) return false;
    return !bluetooth_manager_->GetConnectedDevices().empty();
}

void CaptureStudioPanel::LogMessage(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
}

std::string CaptureStudioPanel::GenerateCaptureFilename() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "capture_" << std::put_time(std::localtime(&time_t), "%H%M%S");
    return ss.str();
}

bool CaptureStudioPanel::ValidateSystemState() const {
    return session_manager_ && session_manager_->HasActiveSession() && 
           camera_manager_ && camera_manager_->GetConnectedCount() > 0;
}

// Pauseable automation methods
void CaptureStudioPanel::PauseSequence() {
    if (!auto_sequence_active_ || sequence_paused_) return;

    sequence_paused_ = true;
    sequence_pause_requested_ = true;  // Signal background thread to pause
    SetCurrentStep(SequenceStep::Paused, "Sequence paused by user", 0.0f);
    LogMessage("[STUDIO] Sequence paused at step: " + GetStepName(current_step_));
}

void CaptureStudioPanel::ResumeSequence() {
    if (!auto_sequence_active_ || !sequence_paused_) return;

    sequence_paused_ = false;
    sequence_pause_requested_ = false;  // Signal background thread to resume
    LogMessage("[STUDIO] Sequence resumed");

    // Resume from where we left off - reset step timer
    step_start_time_ = std::chrono::steady_clock::now();
}

void CaptureStudioPanel::AdvanceToNextStep() {
    if (!auto_sequence_active_) return;
    
    LogMessage("[STUDIO] Advancing to next step (skipping current: " + GetStepName(current_step_) + ")");
    
    switch (current_step_) {
        case SequenceStep::Initializing:
            SetCurrentStep(SequenceStep::Capturing, "Taking capture " + std::to_string(current_capture_index_ + 1) + "/" + std::to_string(auto_capture_count_), 2.0f);
            break;
        case SequenceStep::RotatingAndWaiting:
            SetCurrentStep(SequenceStep::Capturing, "Taking capture " + std::to_string(current_capture_index_ + 1) + "/" + std::to_string(auto_capture_count_), 2.0f);
            break;
        case SequenceStep::Capturing:
            SetCurrentStep(SequenceStep::Processing, "Processing and saving images...", 1.0f);
            break;
        case SequenceStep::Processing:
            current_capture_index_++;
            if (current_capture_index_ >= auto_capture_count_) {
                SetCurrentStep(SequenceStep::Completing, "Finalizing capture sequence...", 1.0f);
            } else {
                SetCurrentStep(SequenceStep::WaitingForNext, "Waiting before next capture...", capture_delay_);
            }
            break;
        case SequenceStep::WaitingForNext:
            SetCurrentStep(SequenceStep::RotatingAndWaiting, "Rotating turntable " + std::to_string(rotation_angle_) + "° and waiting...", 60.0f);
            RotateTurntableAndWait(rotation_angle_);
            break;
        case SequenceStep::Completing:
            StopAutomatedSequence();
            break;
        case SequenceStep::Paused:
            ResumeSequence();
            break;
        default:
            break;
    }
}

void CaptureStudioPanel::SetCurrentStep(SequenceStep step, const std::string& description, float duration_seconds) {
    current_step_ = step;
    current_step_description_ = description;
    step_duration_seconds_ = duration_seconds;
    step_start_time_ = std::chrono::steady_clock::now();
    step_progress_ = 0.0f;
    
    LogMessage("[STUDIO] Step: " + GetStepName(step) + " - " + description);
}

void CaptureStudioPanel::UpdateStepProgress() {
    if (step_duration_seconds_ <= 0.0f) {
        step_progress_ = 1.0f;
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - step_start_time_).count() / 1000.0f;
    step_progress_ = std::min(1.0f, elapsed / step_duration_seconds_);
}

std::string CaptureStudioPanel::GetStepName(SequenceStep step) const {
    switch (step) {
        case SequenceStep::Idle: return "Idle";
        case SequenceStep::Initializing: return "Initializing";
        case SequenceStep::RotatingAndWaiting: return "Rotating & Waiting";
        case SequenceStep::Capturing: return "Capturing";
        case SequenceStep::Processing: return "Processing";
        case SequenceStep::WaitingForNext: return "Waiting";
        case SequenceStep::Completing: return "Completing";
        case SequenceStep::Paused: return "Paused";
        default: return "Unknown";
    }
}

void CaptureStudioPanel::RenderStepIndicator() {
    if (current_step_ == SequenceStep::Idle) return;
    
    // Step name and description
    ImGui::Text("Step: %s", GetStepName(current_step_).c_str());
    ImGui::Text("%s", current_step_description_.c_str());
    
    // Progress bar
    if (step_duration_seconds_ > 0.0f) {
        char progress_label[64];
        snprintf(progress_label, sizeof(progress_label), "%.1fs / %.1fs", 
                step_progress_ * step_duration_seconds_, step_duration_seconds_);
        ImGui::ProgressBar(step_progress_, ImVec2(-1, 0), progress_label);
    } else {
        // Indeterminate progress
        static float indeterminate_progress = 0.0f;
        indeterminate_progress += 0.02f;
        if (indeterminate_progress > 1.0f) indeterminate_progress = 0.0f;
        ImGui::ProgressBar(indeterminate_progress, ImVec2(-1, 0), "In Progress...");
    }
}

void CaptureStudioPanel::WaitForTurntableRotation(float rotation_angle) {
    // Calculate rotation time: (angle × speed) ÷ 360°
    float rotation_time = (std::abs(rotation_angle) * turntable_speed_) / 360.0f;
    
    // Add small buffer for mechanical settling (10% of rotation time, min 0.5s, max 2s)
    float buffer_time = std::min(std::max(rotation_time * 0.1f, 0.5f), 2.0f);
    float total_wait_time = rotation_time + buffer_time;
    
    LogMessage("[STUDIO] ⏱ Time-based wait: " + std::to_string(rotation_angle) + "° × " + 
               std::to_string(turntable_speed_) + "s/360° = " + std::to_string(rotation_time) + "s (+" + 
               std::to_string(buffer_time) + "s buffer)");
    
    turntable_rotation_complete_ = false;
    
    // Sleep for the calculated time
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(total_wait_time * 1000)));

    turntable_rotation_complete_ = true;
    LogMessage("[STUDIO] ✅ Turntable rotation complete after " + std::to_string(total_wait_time) + "s");
}

void CaptureStudioPanel::RenderSessionControl() {
    if (!session_manager_) return;

    // Inline session control without child window
    ImGui::Text("Session:");
    ImGui::SameLine();

    CaptureSession* session = session_manager_->GetCurrentSession();
    if (session) {
        // Active session - compact horizontal layout
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ %s", session->object_name.c_str());
        ImGui::SameLine();
        ImGui::Text("[%s]", session->session_id.substr(0, 8).c_str());
        ImGui::SameLine();
        ImGui::Text("| 📸 %d", session->capture_count);
        ImGui::SameLine();
        ImGui::Text("| 📂 %zu", session->capture_paths.size());
        ImGui::SameLine();

        // Right-aligned buttons
        float button_width = 60.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float pos = ImGui::GetWindowWidth() - button_width * 2 - spacing * 2 - ImGui::GetStyle().WindowPadding.x;
        if (pos > ImGui::GetCursorPosX())
            ImGui::SetCursorPosX(pos);

        if (ImGui::Button("Open", ImVec2(60, 0))) {
            std::filesystem::path abs_path = std::filesystem::absolute(session->base_path);
            std::string command = "explorer \"" + abs_path.string() + "\"";
            system(command.c_str());
            LogMessage("[SESSION] Opened session folder: " + abs_path.string());
        }
        ImGui::SameLine();
        if (ImGui::Button("End", ImVec2(60, 0))) {
            session_manager_->EndCurrentSession();
            LogMessage("[SESSION] Session ended");
        }
    } else {
        // No session - compact creation UI
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "No session");
        ImGui::SameLine();

        ImGui::SetNextItemWidth(180);
        ImGui::InputTextWithHint("##SessionName", "Name (optional)",
                               session_name_input_, sizeof(session_name_input_));
        ImGui::SameLine();

        if (ImGui::Button("Start", ImVec2(60, 0))) {
            std::string session_name = std::string(session_name_input_);

            // Generate default name if empty
            if (session_name.empty()) {
                session_name = GenerateDefaultSessionName();
                LogMessage("[SESSION] Using auto-generated name: " + session_name);
            }

            if (session_manager_->StartNewSession(session_name)) {
                LogMessage("[SESSION] New session started: " + session_name);
                std::memset(session_name_input_, 0, sizeof(session_name_input_));
            }
        }
    }
}

std::string CaptureStudioPanel::GenerateDefaultSessionName() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "capture_" << std::put_time(std::localtime(&time_t), "%m%d_%H%M");
    return ss.str();
}

// Background sequence threading methods
void CaptureStudioPanel::RunAutomatedSequenceInBackground() {
    LogMessage("[THREAD] Starting automated sequence in background...");

    try {
        auto* session = session_manager_->GetCurrentSession();
        if (!session) {
            LogMessage("[THREAD] ERROR: No active session for automated capture");
            thread_safe_sequence_active_ = false;
            return;
        }

        for (int i = 0; i < auto_capture_count_ && !sequence_stop_requested_; ++i) {
            if (sequence_pause_requested_) {
                LogMessage("[THREAD] Sequence paused, waiting...");
                while (sequence_pause_requested_ && !sequence_stop_requested_) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                if (sequence_stop_requested_) break;
                LogMessage("[THREAD] Sequence resumed");
            }

            // Update thread-safe progress
            thread_safe_current_index_ = i;

            // Update step description for UI
            {
                std::lock_guard<std::mutex> lock(step_description_mutex_);
                thread_safe_current_step_description_ = "Capturing position " + std::to_string(i + 1) + "/" + std::to_string(auto_capture_count_);
            }

            LogMessage("[THREAD] Starting capture " + std::to_string(i + 1) + "/" + std::to_string(auto_capture_count_));

            // Get next capture path for this capture
            std::string capture_path = session->GetNextCapturePath();

            // Perform synchronous capture (blocks this background thread, not UI)
            if (camera_manager_->CaptureAllCameras(capture_path, true, 750)) {
                session_manager_->RecordCapture(capture_path);
                LogMessage("[THREAD] Capture " + std::to_string(i + 1) + " completed successfully");
            } else {
                LogMessage("[THREAD] ERROR: Capture " + std::to_string(i + 1) + " failed");
            }

            // Check if this is the last capture
            if (i == auto_capture_count_ - 1) {
                LogMessage("[THREAD] Final capture completed, sequence finished");
                break;
            }

            // Rotate turntable for next position
            if (!sequence_stop_requested_) {
                {
                    std::lock_guard<std::mutex> lock(step_description_mutex_);
                    thread_safe_current_step_description_ = "Rotating turntable...";
                }

                LogMessage("[THREAD] Rotating turntable " + std::to_string(rotation_angle_) + " degrees");
                RotateTurntableAndWait(rotation_angle_);

                // Wait for capture delay
                if (capture_delay_ > 0 && !sequence_stop_requested_) {
                    {
                        std::lock_guard<std::mutex> lock(step_description_mutex_);
                        thread_safe_current_step_description_ = "Waiting " + std::to_string(capture_delay_) + "s before next capture...";
                    }

                    auto delay_start = std::chrono::steady_clock::now();
                    while (std::chrono::duration<float>(std::chrono::steady_clock::now() - delay_start).count() < capture_delay_) {
                        if (sequence_stop_requested_) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            }
        }

        // Sequence completed
        if (!sequence_stop_requested_) {
            LogMessage("[THREAD] ✅ Automated sequence completed successfully! Total captures: " + std::to_string(auto_capture_count_));
            // Play completion sound
            NotificationSounds::Instance().PlayCompletionSound();
        } else {
            LogMessage("[THREAD] Automated sequence stopped by user");
        }

    } catch (const std::exception& e) {
        LogMessage("[THREAD] ERROR: Exception in automated sequence: " + std::string(e.what()));
    } catch (...) {
        LogMessage("[THREAD] ERROR: Unknown exception in automated sequence");
    }

    // Clean up thread state
    thread_safe_sequence_active_ = false;
    thread_safe_current_index_ = 0;
    {
        std::lock_guard<std::mutex> lock(step_description_mutex_);
        thread_safe_current_step_description_ = "Sequence completed";
    }
}

void CaptureStudioPanel::StopSequenceThread() {
    if (sequence_thread_.joinable()) {
        LogMessage("[THREAD] Stopping sequence thread...");
        sequence_stop_requested_ = true;
        sequence_thread_.join();
        LogMessage("[THREAD] Sequence thread stopped");
    }
}

void CaptureStudioPanel::UpdateSequenceStateFromThread() {
    // Update UI state from thread-safe variables
    current_capture_index_ = thread_safe_current_index_.load();
    auto_sequence_active_ = thread_safe_sequence_active_.load();

    // Update step description
    {
        std::lock_guard<std::mutex> lock(step_description_mutex_);
        current_step_description_ = thread_safe_current_step_description_;
    }

    // Update UI state when sequence completes
    if (!auto_sequence_active_ && sequence_thread_.joinable()) {
        StopSequenceThread();
        is_capturing_ = false;
        SetCurrentStep(SequenceStep::Idle, "Sequence completed", 0.0f);
    }
}


} // namespace SaperaCapturePro