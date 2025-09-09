#include "CaptureStudioPanel.hpp"
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>

namespace SaperaCapturePro {

CaptureStudioPanel::CaptureStudioPanel() = default;

CaptureStudioPanel::~CaptureStudioPanel() {
    Shutdown();
}

void CaptureStudioPanel::Initialize(CameraManager* camera_manager, BluetoothManager* bluetooth_manager, SessionManager* session_manager) {
    camera_manager_ = camera_manager;
    bluetooth_manager_ = bluetooth_manager;
    session_manager_ = session_manager;
    
    // Initialize shared widgets
    session_widget_ = std::make_unique<SessionWidget>();
    session_widget_->Initialize(session_manager_);
    session_widget_->SetLogCallback([this](const std::string& msg) { LogMessage(msg); });
    
    file_explorer_widget_ = std::make_unique<FileExplorerWidget>();
    file_explorer_widget_->Initialize(); // Initialize GDI+ for TIFF loading
    file_explorer_widget_->SetHeight(200.0f);
    file_explorer_widget_->SetShowPreview(true);
    file_explorer_widget_->SetLogCallback([this](const std::string& msg) { LogMessage(msg); });
    
    LogMessage("[STUDIO] Capture Studio Panel initialized");
}

void CaptureStudioPanel::Shutdown() {
    if (auto_sequence_active_) {
        StopAutomatedSequence();
    }
    
    session_widget_.reset();
    file_explorer_widget_.reset();
    
    camera_manager_ = nullptr;
    bluetooth_manager_ = nullptr;
    session_manager_ = nullptr;
}

void CaptureStudioPanel::Render() {
    if (ImGui::Begin("🎬 Capture Studio", nullptr, ImGuiWindowFlags_NoCollapse)) {
        if (!camera_manager_ || !session_manager_) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "❌ System not initialized");
            ImGui::End();
            return;
        }
    
    // Update automated sequence if active
    if (auto_sequence_active_) {
        UpdateAutomatedSequence();
    }
    
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    
    if (ImGui::BeginChild("CaptureStudioPanel", ImVec2(0, widget_height_), true)) {
        
        ImGui::Text("🎬 Capture Studio");
        ImGui::SameLine();
        
        // Quick system status
        if (camera_manager_->GetConnectedCount() == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "⚠ No cameras");
        } else {
            int connected_cameras = camera_manager_->GetConnectedCount();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "📷 %d cameras", connected_cameras);
        }
        
        ImGui::SameLine();
        if (IsTurntableConnected()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "🔗 Turntable");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "⭕ No turntable");
        }
        
        ImGui::Separator();
        
        // Mode selector tabs
        RenderModeSelector();
        
        ImGui::Separator();
        
        // Mode-specific content
        switch (current_mode_) {
            case CaptureMode::Quick:
                RenderQuickCapture();
                break;
            case CaptureMode::Automated:
                RenderAutomatedCapture();
                break;
            case CaptureMode::Advanced:
                RenderAdvancedCapture();
                break;
        }
        
        ImGui::Separator();
        
        // Global capture controls
        RenderCaptureControls();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    
        // Render shared components below
        RenderSharedComponents();
    }
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
    
    // Column 1: Settings
    ImGui::Text("Total Captures:");
    ImGui::SliderInt("##AutoCount", &auto_capture_count_, 6, 72);
    
    ImGui::Text("Rotation Angle:");
    ImGui::SliderFloat("##RotAngle", &rotation_angle_, 1.0f, 30.0f, "%.1f°");
    
    ImGui::NextColumn();
    
    // Column 2: Timing
    ImGui::Text("Capture Delay:");
    ImGui::SliderFloat("##CaptureDelay", &capture_delay_, 0.5f, 10.0f, "%.1fs");
    
    float total_time = auto_capture_count_ * capture_delay_;
    ImGui::Text("Est. Time: %.1fmin", total_time / 60.0f);
    
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

void CaptureStudioPanel::RenderSharedComponents() {
    // Render session control
    session_widget_->Render();
    
    // Render file explorer with current session
    CaptureSession* current_session = session_manager_->GetCurrentSession();
    file_explorer_widget_->Render(current_session);
}

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
    
    auto_sequence_active_ = true;
    sequence_paused_ = false;
    current_capture_index_ = 0;
    last_capture_time_ = std::chrono::steady_clock::now() - std::chrono::seconds(static_cast<int>(capture_delay_));
    
    // Start with initialization step
    SetCurrentStep(SequenceStep::Initializing, "Setting up automated capture sequence...", 2.0f);
}

void CaptureStudioPanel::StopAutomatedSequence() {
    if (!auto_sequence_active_) return;
    
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
                    std::string speed_command = "+CT,TURNSPEED=70.0;";
                    bluetooth_manager_->SendCommand(device_ids[0], speed_command);
                }
                
                // Move to first capture (no rotation needed)
                SetCurrentStep(SequenceStep::Capturing, "Taking capture " + std::to_string(current_capture_index_ + 1) + "/" + std::to_string(auto_capture_count_), 2.0f);
            }
            break;
            
        case SequenceStep::Rotating:
            if (step_elapsed >= step_duration_seconds_) {
                SetCurrentStep(SequenceStep::WaitingToSettle, "Waiting for turntable to settle...", 0.5f);
            }
            break;
            
        case SequenceStep::WaitingToSettle:
            if (step_elapsed >= step_duration_seconds_) {
                SetCurrentStep(SequenceStep::Capturing, "Taking capture " + std::to_string(current_capture_index_ + 1) + "/" + std::to_string(auto_capture_count_), 2.0f);
            }
            break;
            
        case SequenceStep::Capturing:
            if (step_elapsed >= step_duration_seconds_) {
                // Take the capture
                std::string capture_name = "auto_" + std::to_string(current_capture_index_ + 1);
                PerformSingleCapture(capture_name);
                
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
                SetCurrentStep(SequenceStep::Rotating, "Rotating turntable " + std::to_string(rotation_angle_) + "°", 2.0f);
                RotateTurntable(rotation_angle_);
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
        LogMessage("[STUDIO] Starting capture to: " + session_path);
        
        // Use camera manager to perform capture with the session path
        if (camera_manager_->CaptureAllCameras(session_path)) {
            LogMessage("[STUDIO] Capture successful: " + session_path);
            
            // Record with session manager
            session_manager_->RecordCapture(session_path);
        } else {
            LogMessage("[STUDIO] Capture failed");
        }
        
    } catch (const std::exception& e) {
        LogMessage("[STUDIO] Capture error: " + std::string(e.what()));
    }
    
    is_capturing_ = false;
}

void CaptureStudioPanel::RotateTurntable(float degrees) {
    if (!IsTurntableConnected()) return;
    
    LogMessage("[STUDIO] Rotating turntable " + std::to_string(degrees) + "°");
    
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
    SetCurrentStep(SequenceStep::Paused, "Sequence paused by user", 0.0f);
    LogMessage("[STUDIO] Sequence paused at step: " + GetStepName(current_step_));
}

void CaptureStudioPanel::ResumeSequence() {
    if (!auto_sequence_active_ || !sequence_paused_) return;
    
    sequence_paused_ = false;
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
        case SequenceStep::Rotating:
            SetCurrentStep(SequenceStep::WaitingToSettle, "Waiting for turntable to settle...", 0.5f);
            break;
        case SequenceStep::WaitingToSettle:
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
            SetCurrentStep(SequenceStep::Rotating, "Rotating turntable " + std::to_string(rotation_angle_) + "°", 2.0f);
            RotateTurntable(rotation_angle_);
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
        case SequenceStep::Rotating: return "Rotating";
        case SequenceStep::WaitingToSettle: return "Settling";
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

} // namespace SaperaCapturePro