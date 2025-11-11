#include "CaptureStudioPanel.hpp"
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <algorithm>

namespace SaperaCapturePro {

CaptureStudioPanel::CaptureStudioPanel() = default;

CaptureStudioPanel::~CaptureStudioPanel() {
    Shutdown();
}

void CaptureStudioPanel::Initialize(CameraManager* camera_manager, BluetoothManager* bluetooth_manager, SessionManager* session_manager, SettingsManager* settings_manager) {
    camera_manager_ = camera_manager;
    bluetooth_manager_ = bluetooth_manager;
    session_manager_ = session_manager;
    settings_manager_ = settings_manager;

    // Session control state initialized

    file_explorer_widget_ = std::make_unique<FileExplorerWidget>();
    file_explorer_widget_->Initialize(); // Initialize GDI+ for TIFF loading
    file_explorer_widget_->SetHeight(200.0f);
    file_explorer_widget_->SetShowPreview(true);
    file_explorer_widget_->SetLogCallback([this](const std::string& msg) { LogMessage(msg); });

    camera_preview_widget_ = std::make_unique<CameraPreviewWidget>();
    camera_preview_widget_->Initialize(camera_manager, settings_manager);
    camera_preview_widget_->SetLogCallback([this](const std::string& msg) { LogMessage(msg); });
    
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
    camera_preview_widget_.reset();
    
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

    RenderContent();

    ImGui::End();
}

void CaptureStudioPanel::RenderContent() {
    if (!camera_manager_ || !session_manager_) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "❌ System not initialized");
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

    // Integrated session control at top of panel
    RenderSessionControl();
    ImGui::Separator();

    // Tab system for Manual, Automated, and Files
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;

    if (ImGui::BeginTabBar("CaptureModeTabs", tab_bar_flags)) {

        if (ImGui::BeginTabItem("◆ Manual")) {
            current_mode_ = CaptureMode::Manual;
            RenderManualCapture();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("● Automated")) {
            current_mode_ = CaptureMode::Automated;
            RenderAutomatedCapture();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("◉ Preview")) {
            // Render camera preview widget
            if (camera_preview_widget_) {
                camera_preview_widget_->RenderContent();
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                    "❌ Camera preview not available");
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::PopStyleVar();
}

void CaptureStudioPanel::RenderModeSelector() {
    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;

    if (ImGui::BeginTabBar("CaptureModes", tab_bar_flags)) {

        if (ImGui::BeginTabItem("📷 Manual")) {
            current_mode_ = CaptureMode::Manual;
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("🔄 Automated")) {
            current_mode_ = CaptureMode::Automated;
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void CaptureStudioPanel::RenderManualCapture() {
    const float em = ImGui::GetFontSize();
    const ImVec2 content_region = ImGui::GetContentRegionAvail();

    // Single line layout with mode toggle + controls + button

    // Mode selection (inline)
    if (ImGui::RadioButton("All Cameras", !single_camera_mode_)) {
        single_camera_mode_ = false;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Single", single_camera_mode_)) {
        single_camera_mode_ = true;
    }

    ImGui::SameLine();
    ImGui::Dummy(ImVec2(20, 0));
    ImGui::SameLine();
    ImGui::Checkbox("Apply Crop", &apply_crop_manual_);

    ImGui::Separator();

    // Dynamic controls based on mode
    const float label_w = 50.0f;
    const float button_w = 150.0f;

    if (single_camera_mode_) {
        // Single camera mode - show camera selector
        const auto& cameras = camera_manager_->GetDiscoveredCameras();

        if (cameras.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "No cameras");
        } else {
            // Build camera list
            std::vector<std::string> camera_names;
            for (size_t i = 0; i < cameras.size(); ++i) {
                std::string display_name = cameras[i].name;
                if (!cameras[i].serialNumber.empty()) {
                    display_name += " (" + cameras[i].serialNumber + ")";
                }
                camera_names.push_back(display_name + (cameras[i].isConnected ? " ✓" : " ❌"));
            }

            // Ensure valid selection
            if (selected_camera_index_ >= static_cast<int>(cameras.size())) {
                selected_camera_index_ = -1;
                selected_camera_id_ = "";
            }

            // Camera combo on same line
            ImGui::SetNextItemWidth(content_region.x - button_w - 20);
            if (ImGui::BeginCombo("##CameraSelect",
                                selected_camera_index_ >= 0 ? camera_names[selected_camera_index_].c_str() : "Choose camera...")) {
                for (size_t i = 0; i < cameras.size(); ++i) {
                    bool is_selected = (selected_camera_index_ == static_cast<int>(i));
                    if (ImGui::Selectable(camera_names[i].c_str(), is_selected)) {
                        selected_camera_index_ = static_cast<int>(i);
                        selected_camera_id_ = cameras[i].id;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }
    } else {
        // All cameras mode - show count control
        ImGui::Text("Count:");
        ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(content_region.x * 0.2f);
        ImGui::SliderInt("##MC", &manual_capture_count_, 1, 10);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        if (ImGui::InputInt("##MCI", &manual_capture_count_)) {
            manual_capture_count_ = std::clamp(manual_capture_count_, 1, 10);
        }
    }

    // Name input on second line (compact)
    ImGui::Text("Name:"); ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##ManualName", "Optional", manual_capture_name_, sizeof(manual_capture_name_));

    // Button on same line
    ImGui::SameLine();

    bool can_capture = ValidateSystemState() && !is_capturing_;

    // Validation
    if (single_camera_mode_) {
        can_capture = can_capture && (selected_camera_index_ >= 0) && !selected_camera_id_.empty();
        const auto& cameras = camera_manager_->GetDiscoveredCameras();
        if (selected_camera_index_ >= 0 && selected_camera_index_ < static_cast<int>(cameras.size())) {
            can_capture = can_capture && cameras[selected_camera_index_].isConnected;
        }
    }

    // Capture button
    if (can_capture) {
        if (single_camera_mode_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.9f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.8f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.7f, 0.1f, 1.0f));
        }
        if (ImGui::Button("📸 Capture", ImVec2(button_w, em * 2.0f))) {
            if (single_camera_mode_) {
                StartSingleCameraCapture();
            } else {
                StartManualCapture();
            }
        }
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
        ImGui::Button("📸 Capture", ImVec2(button_w, em * 2.0f));
        ImGui::PopStyleVar();

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            if (!session_manager_->HasActiveSession()) {
                ImGui::Text("❌ No session");
            } else if (camera_manager_->GetConnectedCount() == 0) {
                ImGui::Text("❌ No cameras");
            } else if (is_capturing_) {
                ImGui::Text("⏳ Capturing...");
            } else if (single_camera_mode_ && selected_camera_index_ < 0) {
                ImGui::Text("❌ Select camera");
            }
            ImGui::EndTooltip();
        }
    }
}

void CaptureStudioPanel::RenderAutomatedCapture() {
    const float em = ImGui::GetFontSize();
    const ImVec2 content_region = ImGui::GetContentRegionAvail();

    if (auto_sequence_active_) {
        // === ACTIVE SEQUENCE VIEW ===
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 0.9f));
        ImGui::BeginChild("ActiveSequence", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar);

        // Header with progress
        float progress = auto_capture_count_ > 0 ? (float)current_capture_index_ / auto_capture_count_ : 0.0f;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.5f, 1.0f));
        ImGui::Text("RUNNING");
        ImGui::PopStyleColor();
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        ImGui::Text("%d/%d", current_capture_index_, auto_capture_count_);

        // Progress bar
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.3f, 1.0f));
        char prog_buf[32];
        snprintf(prog_buf, sizeof(prog_buf), "%.0f%%", progress * 100.0f);
        ImGui::ProgressBar(progress, ImVec2(-1, em * 1.2f), prog_buf);
        ImGui::PopStyleColor();

        // Current step
        if (!current_step_description_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
            ImGui::TextWrapped("%s", current_step_description_.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Separator();

        // Control buttons
        const float btn_w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;

        if (sequence_paused_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button("Resume", ImVec2(btn_w, em * 1.8f))) ResumeSequence();
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.5f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.6f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.4f, 0.1f, 1.0f));
            if (ImGui::Button("Pause", ImVec2(btn_w, em * 1.8f))) PauseSequence();
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();
        if (ImGui::Button("Skip", ImVec2(btn_w, em * 1.8f))) AdvanceToNextStep();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("Stop", ImVec2(btn_w, em * 1.8f))) StopAutomatedSequence();
        ImGui::PopStyleColor(3);

        ImGui::EndChild();
        ImGui::PopStyleColor();

    } else {
        // === CONFIGURATION VIEW ===
        bool can_start = ValidateSystemState() && IsTurntableConnected() && !is_capturing_;
        bool cameras_ok = camera_manager_->GetConnectedCount() > 0;
        bool turntable_ok = IsTurntableConnected();

        // Use 2-column layout for better space utilization
        ImGui::Columns(2, "AutoConfigCols", false);
        ImGui::SetColumnWidth(0, content_region.x * 0.65f);

        // === LEFT COLUMN: Controls ===

        // Status badges (non-clickable)
        if (cameras_ok) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.5f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.5f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.5f, 0.15f, 1.0f));
            ImGui::SmallButton("✓ Cam");
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.3f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.3f, 0.1f, 1.0f));
            ImGui::SmallButton("⚠ Cam");
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();
        if (turntable_ok) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.5f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.5f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.5f, 0.15f, 1.0f));
            ImGui::SmallButton("✓ Turn");
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.3f, 0.1f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.3f, 0.1f, 1.0f));
            ImGui::SmallButton("⚠ Turn");
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(10, 0));
        ImGui::SameLine();

        // Mode toggle buttons
        if (edit_by_captures_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
            ImGui::SmallButton("Count");
            ImGui::PopStyleColor();
        } else {
            if (ImGui::SmallButton("Count")) edit_by_captures_ = true;
        }

        ImGui::SameLine();
        if (!edit_by_captures_) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
            ImGui::SmallButton("Angle");
            ImGui::PopStyleColor();
        } else {
            if (ImGui::SmallButton("Angle")) edit_by_captures_ = false;
        }

        ImGui::Separator();

        // Compact input controls
        const float label_w = 70.0f;
        const float slider_w = content_region.x * 0.25f;
        const float input_w = 50.0f;

        // Primary input
        if (edit_by_captures_) {
            ImGui::Text("Captures");
            ImGui::SameLine(label_w);
            ImGui::SetNextItemWidth(slider_w);
            if (ImGui::SliderInt("##AC", &auto_capture_count_, 6, 360)) {
                rotation_angle_ = 360.0f / static_cast<float>(auto_capture_count_);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(input_w);
            if (ImGui::InputInt("##ACI", &auto_capture_count_)) {
                auto_capture_count_ = std::clamp(auto_capture_count_, 6, 360);
                rotation_angle_ = 360.0f / static_cast<float>(auto_capture_count_);
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::Text("→ %.2f°", rotation_angle_);
            ImGui::PopStyleColor();
        } else {
            ImGui::Text("Angle");
            ImGui::SameLine(label_w);
            ImGui::SetNextItemWidth(slider_w);
            if (ImGui::SliderFloat("##RA", &rotation_angle_, 1.0f, 60.0f, "%.1f°")) {
                auto_capture_count_ = static_cast<int>(std::round(360.0f / rotation_angle_));
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(input_w);
            if (ImGui::InputFloat("##RAI", &rotation_angle_, 0.1f, 1.0f, "%.1f")) {
                rotation_angle_ = std::clamp(rotation_angle_, 1.0f, 60.0f);
                auto_capture_count_ = static_cast<int>(std::round(360.0f / rotation_angle_));
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::Text("→ %d", auto_capture_count_);
            ImGui::PopStyleColor();
        }

        // Apply crop checkbox
        ImGui::Checkbox("Apply Crop", &apply_crop_automated_);

        // Timing
        ImGui::Text("Speed");
        ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(slider_w);
        ImGui::SliderFloat("##TS", &turntable_speed_, 35.64f, 131.0f, "%.0f s");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(input_w);
        if (ImGui::InputFloat("##TSI", &turntable_speed_, 1.0f, 5.0f, "%.0f")) {
            turntable_speed_ = std::clamp(turntable_speed_, 35.64f, 131.0f);
        }

        ImGui::NextColumn();

        // === RIGHT COLUMN: Summary & Start ===

        float rotation_time = (rotation_angle_ * turntable_speed_) / 360.0f;
        // Estimate capture time: ~1-2 seconds per camera with stagger delays
        float estimated_capture_time = 2.0f; // Conservative estimate
        float total_time = auto_capture_count_ * (estimated_capture_time + rotation_time);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("~%.1f min", total_time / 60.0f);
        ImGui::Text("%.1fs/step", rotation_time + estimated_capture_time);
        ImGui::Text("%d pos", auto_capture_count_);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Start button
        if (can_start) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.8f, 1.0f));
            if (ImGui::Button("▶ Start", ImVec2(-1, em * 2.5f))) {
                StartAutomatedSequence();
            }
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
            ImGui::Button("▶ Start", ImVec2(-1, em * 2.5f));
            ImGui::PopStyleVar();
        }

        ImGui::Columns(1);
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

void CaptureStudioPanel::StartManualCapture() {
    if (!ValidateSystemState()) return;

    std::string capture_name = std::string(manual_capture_name_);
    if (capture_name.empty()) {
        capture_name = GenerateCaptureFilename();
    }
    
    LogMessage("[STUDIO] Starting manual capture: " + capture_name + " (" + std::to_string(manual_capture_count_) + " shots)");
    
    for (int i = 0; i < manual_capture_count_; i++) {
        std::string shot_name = capture_name;
        if (manual_capture_count_ > 1) {
            shot_name += "_" + std::to_string(i + 1);
        }
        PerformSingleCapture(shot_name);
    }
}

void CaptureStudioPanel::StartSingleCameraCapture() {
    if (!ValidateSystemState()) return;

    if (selected_camera_id_.empty() || selected_camera_index_ < 0) {
        LogMessage("[STUDIO] Error: No camera selected for single camera capture");
        return;
    }

    const auto& cameras = camera_manager_->GetDiscoveredCameras();
    if (selected_camera_index_ >= static_cast<int>(cameras.size())) {
        LogMessage("[STUDIO] Error: Selected camera index out of range");
        return;
    }

    const auto& selected_camera = cameras[selected_camera_index_];
    if (!selected_camera.isConnected) {
        LogMessage("[STUDIO] Error: Selected camera is not connected");
        return;
    }

    std::string capture_name = std::string(manual_capture_name_);
    if (capture_name.empty()) {
        capture_name = GenerateCaptureFilename();
    }

    // Add camera identifier to filename
    std::string camera_identifier = selected_camera.name;
    if (!selected_camera.serialNumber.empty()) {
        camera_identifier += "_" + selected_camera.serialNumber;
    }

    // Clean camera identifier for filename
    std::replace_if(camera_identifier.begin(), camera_identifier.end(),
                   [](char c) { return !std::isalnum(c); }, '_');

    std::string final_name = capture_name + "_" + camera_identifier;

    LogMessage("[STUDIO] Starting single camera capture: " + selected_camera.name + " -> " + final_name);

    // Apply camera settings from config before capture
    ApplyCameraSettingsFromConfig();

    // Get current session path
    CaptureSession* current_session = session_manager_->GetCurrentSession();
    if (!current_session) {
        LogMessage("[STUDIO] Error: No active session");
        return;
    }

    is_capturing_ = true;
    capture_start_time_ = std::chrono::steady_clock::now();

    // Capture using the selected camera ID
    bool success = camera_manager_->CaptureCamera(selected_camera_id_, current_session->base_path);

    is_capturing_ = false;

    if (success) {
        LogMessage("[STUDIO] ✅ Single camera capture completed successfully");
        // Play completion sound
        NotificationSounds::Instance().PlayCompletionSound();
    } else {
        LogMessage("[STUDIO] ❌ Single camera capture failed");
        // Play notification sound for error
        NotificationSounds::Instance().PlayCompletionSound();
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
                    // More captures to go - rotate immediately (no delay needed)
                    SetCurrentStep(SequenceStep::RotatingAndWaiting, "Rotating turntable " + std::to_string(rotation_angle_) + "° and waiting for completion...", 60.0f);
                    RotateTurntableAndWait(rotation_angle_);
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

    // Apply camera settings from config before capture
    ApplyCameraSettingsFromConfig();

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

        // Get parallel capture settings - SINGLE INSTANCE READ
        const auto& camera_settings = settings_manager_->GetCameraSettings();
        CameraManager::CaptureParams capture_params;
        capture_params.parallel_groups = camera_settings.parallel_capture_groups;
        capture_params.group_delay_ms = camera_settings.capture_delay_ms;
        capture_params.stagger_delay_ms = camera_settings.stagger_delay_ms;

        // Use async camera manager to perform capture without blocking UI
        camera_manager_->CaptureAllCamerasAsync(session_path, capture_params,
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

        // Get parallel capture settings - SINGLE INSTANCE READ
        const auto& camera_settings = settings_manager_->GetCameraSettings();
        CameraManager::CaptureParams capture_params;
        capture_params.parallel_groups = camera_settings.parallel_capture_groups;
        capture_params.group_delay_ms = camera_settings.capture_delay_ms;
        capture_params.stagger_delay_ms = camera_settings.stagger_delay_ms;

        // Use synchronous camera manager for automated sequences
        if (camera_manager_->CaptureAllCameras(session_path, capture_params)) {
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
                // Rotate immediately (no delay needed)
                SetCurrentStep(SequenceStep::RotatingAndWaiting, "Rotating turntable " + std::to_string(rotation_angle_) + "° and waiting for completion...", 60.0f);
                RotateTurntableAndWait(rotation_angle_);
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

        // Apply camera settings from config before starting sequence
        ApplyCameraSettingsFromConfig();

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

            // Get parallel capture settings - SINGLE INSTANCE READ
            const auto& camera_settings = settings_manager_->GetCameraSettings();
            CameraManager::CaptureParams capture_params;
            capture_params.parallel_groups = camera_settings.parallel_capture_groups;
            capture_params.group_delay_ms = camera_settings.capture_delay_ms;
            capture_params.stagger_delay_ms = camera_settings.stagger_delay_ms;

            // Perform synchronous capture (blocks this background thread, not UI)
            if (camera_manager_->CaptureAllCameras(capture_path, capture_params)) {
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

                // No manual delay needed - stagger delays in CameraManager handle bandwidth
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

void CaptureStudioPanel::ApplyCameraSettingsFromConfig() {
    if (!camera_manager_ || !settings_manager_) {
        LogMessage("[STUDIO] Cannot apply settings: manager(s) not initialized");
        return;
    }

    const auto& camera_settings = settings_manager_->GetCameraSettings();

    // Apply exposure time
    camera_manager_->ApplyParameterToAllCameras("ExposureTime", std::to_string(camera_settings.exposure_time));
    LogMessage("[STUDIO] Applied ExposureTime: " + std::to_string(camera_settings.exposure_time) + " us");

    // Apply gain
    camera_manager_->ApplyParameterToAllCameras("Gain", std::to_string(camera_settings.gain));
    LogMessage("[STUDIO] Applied Gain: " + std::to_string(camera_settings.gain));

    // Apply white balance
    camera_manager_->ApplyParameterToAllCameras("BalanceRatioRed", std::to_string(camera_settings.white_balance_red));
    camera_manager_->ApplyParameterToAllCameras("BalanceRatioGreen", std::to_string(camera_settings.white_balance_green));
    camera_manager_->ApplyParameterToAllCameras("BalanceRatioBlue", std::to_string(camera_settings.white_balance_blue));
    LogMessage("[STUDIO] Applied White Balance: R=" + std::to_string(camera_settings.white_balance_red) +
               " G=" + std::to_string(camera_settings.white_balance_green) +
               " B=" + std::to_string(camera_settings.white_balance_blue));

    LogMessage("[STUDIO] ✅ All camera settings applied from config");
}


} // namespace SaperaCapturePro