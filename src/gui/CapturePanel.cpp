#include "CapturePanel.hpp"
#include "NeuralCaptureGUI.hpp"
#include <imgui.h>
#include <cstring>
#include <filesystem>

CapturePanel::CapturePanel() {
    std::strcpy(session_name_buffer_, "neural_capture_session");
    std::strcpy(output_path_buffer_, "neural_dataset");
}

CapturePanel::~CapturePanel() = default;

void CapturePanel::Initialize() {
    // Initialize default values
}

void CapturePanel::Render(const CaptureSession& session) {
    if (!visible) return;

    ImGui::Begin("Capture Control", &visible);

    // Session controls
    RenderSessionControls(session);
    
    ImGui::Separator();
    
    // Capture controls
    RenderCaptureControls(session);
    
    ImGui::Separator();
    
    // Format settings
    RenderFormatSettings();
    
    ImGui::Separator();
    
    // Output settings
    RenderOutputSettings();
    
    ImGui::Separator();
    
    // Batch settings
    RenderBatchSettings();

    ImGui::End();
}

void CapturePanel::RenderSessionControls(const CaptureSession& session) {
    ImGui::Text("Session Management");
    
    // Session name
    ImGui::Text("Session Name:");
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##SessionName", session_name_buffer_, sizeof(session_name_buffer_))) {
        if (onSetSessionName) {
            onSetSessionName(session_name_buffer_);
        }
    }
    
    // Current session info
    if (session.isActive) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active Session: %s", session.sessionName.c_str());
        ImGui::Text("Started: %s", session.timestamp.c_str());
        ImGui::Text("Captures: %d", session.captureCount);
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No active session");
    }
    
    // Session controls
    if (ImGui::Button("New Session", ImVec2(100, 30))) {
        if (onResetCapture) onResetCapture();
    }
    
    ImGui::SameLine();
    if (session.isActive) {
        if (ImGui::Button("End Session", ImVec2(100, 30))) {
            if (onStopCapture) onStopCapture();
        }
    }
}

void CapturePanel::RenderCaptureControls(const CaptureSession& session) {
    ImGui::Text("Capture Operations");
    
    // Large capture buttons
    ImVec2 button_size(120, 40);
    
    if (!session.isActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.6f, 0.0f, 1.0f));
        
        if (ImGui::Button("▶️ START CAPTURE", button_size)) {
            if (onStartCapture) onStartCapture();
        }
        
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
        
        if (ImGui::Button("⏹️ STOP CAPTURE", button_size)) {
            if (onStopCapture) onStopCapture();
        }
        
        ImGui::PopStyleColor(3);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("🔄 RESET COUNTER", button_size)) {
        if (onResetCapture) onResetCapture();
    }
    
    // Continuous capture mode
    ImGui::Checkbox("Continuous Capture Mode", &continuous_capture_);
    if (continuous_capture_) {
        ImGui::SameLine();
        ImGui::Text("Interval:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::InputInt("ms##Interval", &capture_interval_ms_, 100, 1000);
        if (capture_interval_ms_ < 100) capture_interval_ms_ = 100;
    }
}

void CapturePanel::RenderFormatSettings() {
    ImGui::Text("Capture Format");
    
    const char* formats[] = { "TIFF (Color Processed)", "RAW (Bayer Data)" };
    
    if (ImGui::Combo("Format", &format_selection_, formats, 2)) {
        std::string format = (format_selection_ == 0) ? "TIFF" : "RAW";
        if (onSetCaptureFormat) {
            onSetCaptureFormat(format);
        }
    }
    
    // Format descriptions
    if (format_selection_ == 0) {
        ImGui::TextWrapped("TIFF: Color-processed images with standard RGB data. "
                          "Suitable for immediate viewing and standard image processing.");
    } else {
        ImGui::TextWrapped("RAW: Unprocessed Bayer sensor data. "
                          "Preserves maximum image quality for advanced processing and neural rendering.");
    }
    
    // File size estimation
    if (format_selection_ == 0) {
        ImGui::Text("Estimated file size per camera: ~37 MB (TIFF)");
    } else {
        ImGui::Text("Estimated file size per camera: ~12 MB (RAW)");
    }
    
    ImGui::Text("Total per capture (12 cameras): ~%.1f MB", 
                format_selection_ == 0 ? 444.0f : 144.0f);
}

void CapturePanel::RenderOutputSettings() {
    ImGui::Text("Output Settings");
    
    // Output path
    ImGui::Text("Output Directory:");
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##OutputPath", output_path_buffer_, sizeof(output_path_buffer_))) {
        if (onSetOutputPath) {
            onSetOutputPath(output_path_buffer_);
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        // TODO: Open file dialog
        // For now, just show a tooltip
        ImGui::SetTooltip("File dialog integration not implemented yet");
    }
    
    // Check if path exists
    bool path_exists = std::filesystem::exists(output_path_buffer_);
    if (path_exists) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Directory exists");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ Directory will be created");
    }
    
    // File naming pattern
    ImGui::Text("File Naming Pattern:");
    ImGui::BulletText("Session: capture_001_YYYYMMDD_HHMMSS/");
    ImGui::BulletText("Files: cam_XX_capture_001.%s", format_selection_ == 0 ? "tiff" : "raw");
    
    // Disk space check
    try {
        auto space = std::filesystem::space(std::filesystem::path(output_path_buffer_).parent_path());
        double available_gb = static_cast<double>(space.available) / (1024.0 * 1024.0 * 1024.0);
        
        if (available_gb > 10.0) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), 
                              "Available space: %.1f GB", available_gb);
        } else if (available_gb > 1.0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                              "Available space: %.1f GB (Low)", available_gb);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 
                              "Available space: %.1f GB (Critical)", available_gb);
        }
    } catch (...) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Cannot check disk space");
    }
}

void CapturePanel::RenderBatchSettings() {
    ImGui::Text("Batch Capture Settings");
    
    // Batch size
    ImGui::Text("Captures per batch:");
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("##BatchSize", &batch_size_, 1, 10);
    if (batch_size_ < 1) batch_size_ = 1;
    
    // Neural rendering specific settings
    ImGui::Separator();
    ImGui::Text("Neural Rendering Dataset");
    
    ImGui::TextWrapped("For optimal neural rendering results:");
    ImGui::BulletText("Use RAW format for maximum quality");
    ImGui::BulletText("Capture multiple viewpoints around the subject");
    ImGui::BulletText("Ensure consistent lighting conditions");
    ImGui::BulletText("Maintain fixed camera positions");
    
    // Quick batch operations
    if (ImGui::Button("Capture Single Frame", ImVec2(150, 0))) {
        batch_size_ = 1;
        if (onStartCapture) onStartCapture();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Capture 10 Frames", ImVec2(150, 0))) {
        batch_size_ = 10;
        if (onStartCapture) onStartCapture();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Capture 100 Frames", ImVec2(150, 0))) {
        batch_size_ = 100;
        if (onStartCapture) onStartCapture();
    }
} 