#include "AutomatedCapturePanel.hpp"
#include <glad/glad.h>
#include "../capture/AutomatedCaptureController.hpp" // Include first for CapturePosition
#include "../bluetooth/BluetoothManager.hpp"
#include "../bluetooth/BluetoothCommands.hpp"
#include "../hardware/CameraManager.hpp"
#include "../utils/SessionManager.hpp"
#include "../rendering/HemisphereRenderer.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <chrono>

#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SaperaCapturePro {

AutomatedCapturePanel::AutomatedCapturePanel() = default;

AutomatedCapturePanel::~AutomatedCapturePanel() {
    Shutdown();
}

void AutomatedCapturePanel::Initialize() {
    // Initialize 3D rendering components
    hemisphere_renderer_ = std::make_unique<HemisphereRenderer>();
    hemisphere_renderer_->Initialize();
    
    // Initialize capture controller
    capture_controller_ = std::make_unique<AutomatedCaptureController>();
    
    // Set up OpenGL framebuffer for 3D viewport
    InitializeFramebuffer();
    
    // Generate initial capture positions
    GenerateCapturePositions();
    
    LogMessage("[AUTOMATED] Panel initialized successfully");
}

void AutomatedCapturePanel::Render(bool* p_open) {
    if (!p_open || !*p_open) return;
    
    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("🤖 Automated Capture", p_open, ImGuiWindowFlags_NoCollapse)) {
        
        ImGui::Text("Neural Rendering Dataset Generator");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Control section
        RenderControlSection();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // 3D visualization section
        RenderVisualizationSection();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Progress section
        if (session_.isActive) {
            RenderProgressSection();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
        
        // Advanced settings (collapsible)
        if (ImGui::CollapsingHeader("⚙ Advanced Settings", show_advanced_settings_ ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            RenderAdvancedSettings();
        }
    }
    
    ImGui::End();
    
    // Update automation if active
    if (session_.isActive && !session_.isPaused) {
        UpdateProgress();
    }
}

void AutomatedCapturePanel::RenderControlSection() {
    // Quick Setup Section
    ImGui::Text("🎯 Quick Setup");
    
    // Simple mode selection with friendly names
    const char* mode_items[] = { "📐 Standard Scan", "🌐 360° Complete" };
    int current_mode = static_cast<int>(session_.mode);
    if (ImGui::Combo("Scan Type", &current_mode, mode_items, IM_ARRAYSIZE(mode_items))) {
        session_.mode = static_cast<CaptureMode>(current_mode);
        GenerateCapturePositions();
    }
    
    // Quick presets
    ImGui::SameLine();
    if (ImGui::Button("Quick")) {
        session_.stepsPerRotation = 8;
        GenerateCapturePositions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Standard")) {
        session_.stepsPerRotation = 12;
        GenerateCapturePositions();
    }
    ImGui::SameLine();
    if (ImGui::Button("Detailed")) {
        session_.stepsPerRotation = 24;
        GenerateCapturePositions();
    }
    
    ImGui::Spacing();
    
    // Turntable Controls - Direct and Friendly
    ImGui::Text("🎛️ Turntable Test");
    
    // Speed presets with friendly names
    static int speed_preset = 1; // Default to Medium
    const char* speed_items[] = { "🐌 Slow & Steady", "⚡ Medium Speed", "🚀 Fast Motion" };
    ImGui::Combo("Speed", &speed_preset, speed_items, IM_ARRAYSIZE(speed_items));
    
    // Get speeds based on preset
    float rotation_speed = (speed_preset == 0) ? 45.0f : (speed_preset == 1) ? 70.0f : 100.0f;
    float tilt_speed = (speed_preset == 0) ? 12.0f : (speed_preset == 1) ? 20.0f : 30.0f;
    
    // Direct test buttons - organized and friendly
    if (ImGui::Button("↻ Test Left 15°", ImVec2(110, 0))) TestRotation(-15.0f, rotation_speed);
    ImGui::SameLine();
    if (ImGui::Button("↺ Test Right 15°", ImVec2(110, 0))) TestRotation(15.0f, rotation_speed);
    ImGui::SameLine();
    if (ImGui::Button("🏠 Home", ImVec2(60, 0))) TestReturnToZero();
    
    if (ImGui::Button("↗ Tilt Up 15°", ImVec2(110, 0))) TestTilt(15.0f, tilt_speed);
    ImGui::SameLine();
    if (ImGui::Button("↘ Tilt Down 15°", ImVec2(110, 0))) TestTilt(-15.0f, tilt_speed);
    ImGui::SameLine();
    if (ImGui::Button("📐 Level", ImVec2(60, 0))) TestTiltToZero();
    
    // Emergency stop - always visible
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("🛑 STOP", ImVec2(60, 0))) EmergencyStopTurntable();
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
    
    // Simple stats
    ImGui::Text("📊 %zu positions planned • Position %d of %zu", 
                session_.positions.size(), session_.currentIndex + 1, session_.positions.size());
    
    ImGui::Spacing();
    
    // Control buttons
    bool can_start = !session_.isActive && 
                     bluetooth_manager_ && bluetooth_manager_->IsConnected() && 
                     camera_manager_ && camera_manager_->GetConnectedCount() > 0;
    
    if (can_start) {
        if (ImGui::Button("▶ Start Automated Capture", ImVec2(200, 40))) {
            StartAutomatedCapture();
        }
    } else if (session_.isActive) {
        if (session_.isPaused) {
            if (ImGui::Button("▶ Resume", ImVec2(90, 30))) {
                PauseAutomatedCapture(); // Toggle pause
            }
        } else {
            if (ImGui::Button("⏸ Pause", ImVec2(90, 30))) {
                PauseAutomatedCapture();
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("⏹ Stop", ImVec2(90, 30))) {
            StopAutomatedCapture();
        }
    } else {
        // Show why we can't start
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::Button("▶ Start Automated Capture", ImVec2(200, 40));
        ImGui::PopStyleColor();
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Requirements:\n- Bluetooth turntable connected\n- At least one camera connected\n- Active session");
        }
        
        // Status messages
        if (!bluetooth_manager_ || !bluetooth_manager_->IsConnected()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ Bluetooth turntable not connected");
        }
        if (!camera_manager_ || camera_manager_->GetConnectedCount() == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⚠ No cameras connected");
        }
    }
}

void AutomatedCapturePanel::RenderVisualizationSection() {
    ImGui::Text("◆ 3D Hemisphere View");
    
    // Get available space for viewport
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    int desired_width = static_cast<int>(std::min(content_region.x - 20.0f, 400.0f));
    int desired_height = static_cast<int>(desired_width * 0.75f); // 4:3 aspect ratio
    
    // Resize framebuffer if needed
    if (desired_width != viewport_width_ || desired_height != viewport_height_) {
        viewport_width_ = desired_width;
        viewport_height_ = desired_height;
        ResizeFramebuffer(viewport_width_, viewport_height_);
    }
    
    // Render 3D scene to framebuffer
    RenderToFramebuffer();
    
    // Display the rendered texture
    ImVec2 viewport_pos = ImGui::GetCursorScreenPos();
    ImGui::Image(reinterpret_cast<void*>(color_texture_), 
                 ImVec2(static_cast<float>(viewport_width_), static_cast<float>(viewport_height_)),
                 ImVec2(0, 1), ImVec2(1, 0)); // Flip Y coordinate for OpenGL
    
    // Handle mouse interaction for 3D view
    if (ImGui::IsItemHovered()) {
        HandleViewportInteraction();
    }
    
    // View controls
    ImGui::Spacing();
    ImGui::Text("View Controls:");
    ImGui::SliderFloat("Azimuth", &view_azimuth_, -180.0f, 180.0f, "%.1f°");
    ImGui::SliderFloat("Elevation", &view_elevation_, -90.0f, 90.0f, "%.1f°");
    ImGui::SliderFloat("Distance", &view_distance_, 2.0f, 10.0f, "%.1f");
    
    if (ImGui::Button("Reset View")) {
        view_azimuth_ = 0.0f;
        view_elevation_ = 30.0f;
        view_distance_ = 5.0f;
    }
}

void AutomatedCapturePanel::RenderProgressSection() {
    ImGui::Text("◆ Capture Progress");
    
    // Overall progress bar
    float progress = static_cast<float>(session_.currentIndex) / static_cast<float>(session_.positions.size());
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), 
                      (std::to_string(session_.currentIndex) + " / " + std::to_string(session_.positions.size())).c_str());
    
    // Current position info
    if (session_.currentIndex < session_.positions.size()) {
        const auto& pos = session_.positions[session_.currentIndex];
        ImGui::Text("Current: Azimuth %.1f°, Elevation %.1f°", pos.azimuth, pos.elevation);
    }
    
    // Estimated time remaining (placeholder)
    ImGui::Text("Status: %s", session_.isPaused ? "Paused" : "Running");
}

void AutomatedCapturePanel::RenderAdvancedSettings() {
    ImGui::Text("🔧 Fine-Tune Settings");
    
    // Camera angle range
    ImGui::Text("📸 Camera Coverage:");
    ImGui::SliderFloat("Look Down", &session_.minElevation, -90.0f, 0.0f, "%.0f°");
    ImGui::SliderFloat("Look Up", &session_.maxElevation, 0.0f, 90.0f, "%.0f°");
    
    ImGui::Spacing();
    
    // Timing 
    ImGui::Text("⏱️ Timing:");
    ImGui::SliderFloat("Wait After Move", &session_.captureDelay, 0.5f, 5.0f, "%.1f sec");
    
    ImGui::Spacing();
    
    // Manual angle control for testing
    ImGui::Text("🎯 Custom Test Angles:");
    static float test_rotation = 30.0f;
    static float test_tilt = 15.0f;
    
    ImGui::SliderFloat("Test Rotation", &test_rotation, -180.0f, 180.0f, "%.0f°");
    ImGui::SameLine();
    if (ImGui::Button("Go")) {
        static int speed_preset = 1; // Use medium speed from main panel
        float rotation_speed = (speed_preset == 0) ? 45.0f : (speed_preset == 1) ? 70.0f : 100.0f;
        TestRotation(test_rotation, rotation_speed);
    }
    
    ImGui::SliderFloat("Test Tilt", &test_tilt, -45.0f, 45.0f, "%.0f°");  
    ImGui::SameLine();
    if (ImGui::Button("Tilt")) {
        static int speed_preset = 1;
        float tilt_speed = (speed_preset == 0) ? 12.0f : (speed_preset == 1) ? 20.0f : 30.0f;
        TestTilt(test_tilt, tilt_speed);
    }
}

// Turntable test methods implementation
void AutomatedCapturePanel::TestRotation(float angle, float speed) {
    if (!bluetooth_manager_ || !bluetooth_manager_->IsConnected()) {
        LogMessage("[TURNTABLE] No turntable connected");
        return;
    }
    
    auto connected_devices = bluetooth_manager_->GetConnectedDevices();
    if (connected_devices.empty()) {
        LogMessage("[TURNTABLE] No devices available");
        return;
    }
    
    const std::string& device_id = connected_devices[0];
    
    // Set speed first, then rotate
    bluetooth_manager_->SendCommand(device_id, BluetoothCommands::SetRotationSpeed(speed));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    bluetooth_manager_->SendCommand(device_id, BluetoothCommands::RotateByAngle(angle));
    
    LogMessage("[TURNTABLE] Test rotation: " + std::to_string(angle) + "° at speed " + std::to_string(speed));
}

void AutomatedCapturePanel::TestTilt(float angle, float speed) {
    if (!bluetooth_manager_ || !bluetooth_manager_->IsConnected()) {
        LogMessage("[TURNTABLE] No turntable connected");
        return;
    }
    
    auto connected_devices = bluetooth_manager_->GetConnectedDevices();
    if (connected_devices.empty()) {
        LogMessage("[TURNTABLE] No devices available");
        return;
    }
    
    const std::string& device_id = connected_devices[0];
    
    // Set speed first, then tilt
    bluetooth_manager_->SendCommand(device_id, BluetoothCommands::SetTiltSpeed(speed));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    bluetooth_manager_->SendCommand(device_id, BluetoothCommands::TiltByAngle(angle));
    
    LogMessage("[TURNTABLE] Test tilt: " + std::to_string(angle) + "° at speed " + std::to_string(speed));
}

void AutomatedCapturePanel::TestReturnToZero() {
    if (!bluetooth_manager_ || !bluetooth_manager_->IsConnected()) {
        LogMessage("[TURNTABLE] No turntable connected");
        return;
    }
    
    auto connected_devices = bluetooth_manager_->GetConnectedDevices();
    if (connected_devices.empty()) return;
    
    bluetooth_manager_->SendCommand(connected_devices[0], BluetoothCommands::ReturnToZero());
    LogMessage("[TURNTABLE] Returning to zero position");
}

void AutomatedCapturePanel::TestTiltToZero() {
    if (!bluetooth_manager_ || !bluetooth_manager_->IsConnected()) {
        LogMessage("[TURNTABLE] No turntable connected");
        return;
    }
    
    auto connected_devices = bluetooth_manager_->GetConnectedDevices();
    if (connected_devices.empty()) return;
    
    bluetooth_manager_->SendCommand(connected_devices[0], BluetoothCommands::TiltToZero());
    LogMessage("[TURNTABLE] Leveling tilt to zero");
}

void AutomatedCapturePanel::EmergencyStopTurntable() {
    if (!bluetooth_manager_ || !bluetooth_manager_->IsConnected()) {
        LogMessage("[TURNTABLE] No turntable connected");
        return;
    }
    
    auto connected_devices = bluetooth_manager_->GetConnectedDevices();
    if (connected_devices.empty()) return;
    
    const std::string& device_id = connected_devices[0];
    
    // Send both stop commands
    bluetooth_manager_->SendCommand(device_id, BluetoothCommands::StopRotation());
    bluetooth_manager_->SendCommand(device_id, BluetoothCommands::StopTilt());
    
    LogMessage("[TURNTABLE] EMERGENCY STOP - All movement halted");
}

void AutomatedCapturePanel::InitializeFramebuffer() {
    // Generate framebuffer
    glGenFramebuffers(1, &framebuffer_id_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_);
    
    // Generate color texture
    glGenTextures(1, &color_texture_);
    glBindTexture(GL_TEXTURE_2D, color_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewport_width_, viewport_height_, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture_, 0);
    
    // Generate depth texture
    glGenTextures(1, &depth_texture_);
    glBindTexture(GL_TEXTURE_2D, depth_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, viewport_width_, viewport_height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LogMessage("[AUTOMATED] ERROR: Framebuffer not complete!");
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void AutomatedCapturePanel::ResizeFramebuffer(int width, int height) {
    if (width <= 0 || height <= 0) return;
    
    // Resize color texture
    glBindTexture(GL_TEXTURE_2D, color_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    
    // Resize depth texture
    glBindTexture(GL_TEXTURE_2D, depth_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void AutomatedCapturePanel::RenderToFramebuffer() {
    // Bind our framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_);
    glViewport(0, 0, viewport_width_, viewport_height_);
    
    // Clear
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    
    if (hemisphere_renderer_) {
        // Set up view matrix based on controls
        float rad_azimuth = view_azimuth_ * M_PI / 180.0f;
        float rad_elevation = view_elevation_ * M_PI / 180.0f;
        
        // Render hemisphere and camera positions
        hemisphere_renderer_->Render(rad_azimuth, rad_elevation, view_distance_, 
                                   viewport_width_, viewport_height_, session_.positions);
    }
    
    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void AutomatedCapturePanel::HandleViewportInteraction() {
    ImGuiIO& io = ImGui::GetIO();
    
    if (ImGui::IsItemClicked()) {
        mouse_captured_ = true;
        last_mouse_pos_ = io.MousePos;
    }
    
    if (mouse_captured_) {
        if (io.MouseDown[0]) {
            ImVec2 delta = ImVec2(io.MousePos.x - last_mouse_pos_.x, io.MousePos.y - last_mouse_pos_.y);
            
            view_azimuth_ += delta.x * 0.5f;
            view_elevation_ -= delta.y * 0.5f;
            
            // Clamp elevation
            view_elevation_ = std::clamp(view_elevation_, -90.0f, 90.0f);
            
            // Wrap azimuth
            if (view_azimuth_ > 180.0f) view_azimuth_ -= 360.0f;
            if (view_azimuth_ < -180.0f) view_azimuth_ += 360.0f;
            
            last_mouse_pos_ = io.MousePos;
        } else {
            mouse_captured_ = false;
        }
    }
    
    // Mouse wheel for zoom
    if (io.MouseWheel != 0.0f) {
        view_distance_ -= io.MouseWheel * 0.5f;
        view_distance_ = std::clamp(view_distance_, 2.0f, 10.0f);
    }
}

void AutomatedCapturePanel::GenerateCapturePositions() {
    session_.positions.clear();
    
    switch (session_.mode) {
        case CaptureMode::Sequential:
            GenerateSequentialPositions();
            break;
        case CaptureMode::Uniform:
            GenerateUniformPositions();
            break;
    }
    
    LogMessage("[AUTOMATED] Generated " + std::to_string(session_.positions.size()) + " capture positions");
}

void AutomatedCapturePanel::GenerateSequentialPositions() {
    // Simple latitude band approach
    float elevation_range = session_.maxElevation - session_.minElevation;
    int elevation_steps = std::max(1, static_cast<int>(elevation_range / 30.0f) + 1); // Roughly every 30 degrees
    
    for (int elev_step = 0; elev_step < elevation_steps; ++elev_step) {
        float elevation = session_.minElevation + (elevation_range * elev_step / (elevation_steps - 1));
        
        // Fewer steps at extreme elevations (poles)
        float elev_factor = std::cos(elevation * M_PI / 180.0f);
        int azimuth_steps = std::max(4, static_cast<int>(session_.stepsPerRotation * elev_factor));
        
        for (int az_step = 0; az_step < azimuth_steps; ++az_step) {
            float azimuth = 360.0f * az_step / azimuth_steps;
            session_.positions.emplace_back(azimuth, elevation);
        }
    }
}

void AutomatedCapturePanel::GenerateUniformPositions() {
    // Fibonacci sphere distribution for uniform coverage
    int n = session_.stepsPerRotation * 3; // More points for better distribution
    
    for (int i = 0; i < n; ++i) {
        float theta = 2.0f * M_PI * i / 1.618033988749895f; // Golden ratio
        float phi = std::acos(1.0f - 2.0f * (i + 0.5f) / n);
        
        float elevation = 90.0f - (phi * 180.0f / M_PI); // Convert to elevation angle
        float azimuth = theta * 180.0f / M_PI; // Convert to degrees
        
        // Wrap azimuth to [0, 360)
        while (azimuth < 0.0f) azimuth += 360.0f;
        while (azimuth >= 360.0f) azimuth -= 360.0f;
        
        // Filter by elevation range
        if (elevation >= session_.minElevation && elevation <= session_.maxElevation) {
            session_.positions.emplace_back(azimuth, elevation);
        }
    }
}

void AutomatedCapturePanel::StartAutomatedCapture() {
    if (session_.isActive) return;
    
    session_.isActive = true;
    session_.isPaused = false;
    session_.currentIndex = 0;
    session_.totalProgress = 0.0f;
    
    // Reset captured flags
    for (auto& pos : session_.positions) {
        pos.captured = false;
        pos.imagePath.clear();
    }
    
    LogMessage("[AUTOMATED] Starting automated capture sequence (" + std::to_string(session_.positions.size()) + " positions)");
    
    if (capture_controller_) {
        capture_controller_->StartSequence(session_.positions, bluetooth_manager_, camera_manager_, session_manager_);
    }
}

void AutomatedCapturePanel::PauseAutomatedCapture() {
    session_.isPaused = !session_.isPaused;
    LogMessage(session_.isPaused ? "[AUTOMATED] Capture paused" : "[AUTOMATED] Capture resumed");
}

void AutomatedCapturePanel::StopAutomatedCapture() {
    if (!session_.isActive) return;
    
    session_.isActive = false;
    session_.isPaused = false;
    
    if (capture_controller_) {
        capture_controller_->StopSequence();
    }
    
    LogMessage("[AUTOMATED] Capture sequence stopped");
}

void AutomatedCapturePanel::UpdateProgress() {
    if (capture_controller_) {
        session_.currentIndex = capture_controller_->GetCurrentPositionIndex();
        
        if (session_.currentIndex >= session_.positions.size()) {
            // Sequence completed
            StopAutomatedCapture();
            LogMessage("[AUTOMATED] Capture sequence completed!");
        }
    }
}

void AutomatedCapturePanel::Shutdown() {
    if (framebuffer_id_) {
        glDeleteFramebuffers(1, &framebuffer_id_);
        framebuffer_id_ = 0;
    }
    if (color_texture_) {
        glDeleteTextures(1, &color_texture_);
        color_texture_ = 0;
    }
    if (depth_texture_) {
        glDeleteTextures(1, &depth_texture_);
        depth_texture_ = 0;
    }
    
    hemisphere_renderer_.reset();
    capture_controller_.reset();
}

} // namespace SaperaCapturePro