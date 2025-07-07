#include "CameraControlPanel.hpp"
#include "NeuralCaptureGUI.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>

CameraControlPanel::CameraControlPanel() {
    std::memset(camera_filter_, 0, sizeof(camera_filter_));
}

CameraControlPanel::~CameraControlPanel() = default;

void CameraControlPanel::Initialize() {
    // Initialize any resources if needed
}

void CameraControlPanel::Render(const std::vector<CameraInfo>& cameras) {
    if (!visible) return;

    ImGui::Begin("Camera Control", &visible);

    // Header with discovery and connection controls
    RenderConnectionControls();
    
    ImGui::Separator();
    
    // Camera filter
    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("##CameraFilter", camera_filter_, sizeof(camera_filter_));
    
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        std::memset(camera_filter_, 0, sizeof(camera_filter_));
    }

    // Auto-connect checkbox
    ImGui::SameLine();
    ImGui::Checkbox("Auto-connect discovered cameras", &auto_connect_);

    ImGui::Separator();

    // Camera table
    RenderCameraTable(cameras);

    // Camera details panel
    if (selected_camera_ >= 0 && selected_camera_ < static_cast<int>(cameras.size())) {
        ImGui::Separator();
        ImGui::Text("Camera Details:");
        RenderCameraDetails(cameras[selected_camera_]);
    }

    ImGui::End();
}

void CameraControlPanel::RenderConnectionControls() {
    // Discovery button
    if (ImGui::Button("🔍 Discover Cameras", ImVec2(140, 30))) {
        if (onDiscoverCameras) onDiscoverCameras();
    }
    
    ImGui::SameLine();
    
    // Connect all button
    if (ImGui::Button("🔗 Connect All", ImVec2(120, 30))) {
        if (onConnectAllCameras) onConnectAllCameras();
    }
    
    ImGui::SameLine();
    
    // Disconnect all button
    if (ImGui::Button("❌ Disconnect All", ImVec2(130, 30))) {
        if (onDisconnectAllCameras) onDisconnectAllCameras();
    }
}

void CameraControlPanel::RenderCameraTable(const std::vector<CameraInfo>& cameras) {
    // Filter cameras based on search text
    std::vector<int> filtered_indices;
    std::string filter_lower = camera_filter_;
    std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);
    
    for (int i = 0; i < static_cast<int>(cameras.size()); ++i) {
        if (filter_lower.empty()) {
            filtered_indices.push_back(i);
        } else {
            std::string serial_lower = cameras[i].serialNumber;
            std::string name_lower = cameras[i].userDefinedName;
            std::transform(serial_lower.begin(), serial_lower.end(), serial_lower.begin(), ::tolower);
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            
            if (serial_lower.find(filter_lower) != std::string::npos ||
                name_lower.find(filter_lower) != std::string::npos) {
                filtered_indices.push_back(i);
            }
        }
    }

    // Camera table
    if (ImGui::BeginTable("CameraTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY,
                          ImVec2(0, 300))) {
        
        // Table headers
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Serial Number", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Server", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Table rows
        for (int idx : filtered_indices) {
            const auto& camera = cameras[idx];
            
            ImGui::TableNextRow();
            
            // Index column
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(std::to_string(camera.cameraIndex).c_str(), 
                                  selected_camera_ == idx, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_camera_ = idx;
            }
            
            // Serial Number column
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", camera.serialNumber.c_str());
            
            // Name column
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", camera.userDefinedName.c_str());
            
            // Server column
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", camera.serverName.c_str());
            
            // Status column
            ImGui::TableSetColumnIndex(4);
            if (camera.isConnected) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Disconnected");
            }
            
            // Actions column
            ImGui::TableSetColumnIndex(5);
            ImGui::PushID(idx);
            
            if (camera.isConnected) {
                if (ImGui::Button("Disconnect", ImVec2(80, 0))) {
                    if (onDisconnectCamera) onDisconnectCamera(idx);
                }
            } else {
                if (ImGui::Button("Connect", ImVec2(80, 0))) {
                    if (onConnectCamera) onConnectCamera(idx);
                }
            }
            
            ImGui::PopID();
        }
        
        ImGui::EndTable();
    }

    // Summary information
    int total_cameras = static_cast<int>(cameras.size());
    int connected_cameras = 0;
    int capturing_cameras = 0;
    
    for (const auto& camera : cameras) {
        if (camera.isConnected) connected_cameras++;
        if (camera.isCapturing) capturing_cameras++;
    }

    ImGui::Text("Summary: %d total, %d connected, %d capturing", 
                total_cameras, connected_cameras, capturing_cameras);
}

void CameraControlPanel::RenderCameraDetails(const CameraInfo& camera) {
    ImGui::BeginChild("CameraDetails", ImVec2(0, 150), true);
    
    ImGui::Text("Camera Index: %d", camera.cameraIndex);
    ImGui::Text("Serial Number: %s", camera.serialNumber.c_str());
    ImGui::Text("User Defined Name: %s", camera.userDefinedName.c_str());
    ImGui::Text("Server Name: %s", camera.serverName.c_str());
    
    ImGui::Separator();
    
    // Status indicators
    ImGui::Text("Connection Status:");
    ImGui::SameLine();
    if (camera.isConnected) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Connected");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "✗ Disconnected");
    }
    
    ImGui::Text("Capture Status:");
    ImGui::SameLine();
    if (camera.isCapturing) {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "📷 Capturing");
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "⏸️ Idle");
    }
    
    ImGui::Separator();
    
    // Quick actions for selected camera
    if (camera.isConnected) {
        if (ImGui::Button("Disconnect This Camera", ImVec2(180, 0))) {
            if (onDisconnectCamera) onDisconnectCamera(selected_camera_);
        }
    } else {
        if (ImGui::Button("Connect This Camera", ImVec2(180, 0))) {
            if (onConnectCamera) onConnectCamera(selected_camera_);
        }
    }
    
    ImGui::EndChild();
} 