#include "HardwarePanel.hpp"
#include "../utils/SessionManager.hpp"
#include "../bluetooth/BluetoothCommands.hpp"
#include "../hardware/CameraTypes.hpp"
#include <algorithm>

namespace SaperaCapturePro {

HardwarePanel::HardwarePanel() = default;

HardwarePanel::~HardwarePanel() {
    Shutdown();
}

void HardwarePanel::Initialize(BluetoothManager* bluetooth_manager, CameraManager* camera_manager, SessionManager* session_manager) {
    bluetooth_manager_ = bluetooth_manager;
    camera_manager_ = camera_manager;
    session_manager_ = session_manager;
}

void HardwarePanel::Shutdown() {
    bluetooth_manager_ = nullptr;
    camera_manager_ = nullptr;
    session_manager_ = nullptr;
}

void HardwarePanel::Render(bool* p_open) {
    if (!ImGui::Begin("🔧 Hardware Control", p_open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    
    // Status overview at top
    ImGui::Text("Hardware Status:");
    ImGui::SameLine();
    
    // Camera status
    if (AreCamerasConnected()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "📷 %d Cameras", GetConnectedCameraCount());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "📷 No Cameras");
    }
    
    ImGui::SameLine();
    ImGui::Text("•");
    ImGui::SameLine();
    
    // Bluetooth status
    if (IsBluetoothConnected()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "🔗 Bluetooth Connected");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "🔗 No Bluetooth");
    }
    
    ImGui::Separator();
    
    // Tab bar for camera and bluetooth controls
    if (ImGui::BeginTabBar("HardwareTabs", ImGuiTabBarFlags_None)) {
        
        // Camera tab
        if (ImGui::BeginTabItem("📷 Cameras")) {
            RenderCameraTab();
            ImGui::EndTabItem();
        }
        
        // Bluetooth tab
        if (ImGui::BeginTabItem("🔗 Bluetooth Turntable")) {
            RenderBluetoothTab();
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void HardwarePanel::RenderCameraTab() {
    ImGui::Text("Sapera SDK Camera System");
    ImGui::Separator();
    
    RenderCameraControls();
    ImGui::Spacing();
    RenderCameraTable();
}

void HardwarePanel::RenderBluetoothTab() {
    ImGui::Text("Turntable Bluetooth Control");
    ImGui::Separator();
    
    RenderBluetoothControls();
    ImGui::Spacing();
    RenderBluetoothDeviceList();
    
    if (IsBluetoothConnected()) {
        ImGui::Spacing();
        ImGui::Separator();
        RenderBluetoothAdvancedControls();
    }
}

void HardwarePanel::RenderCameraControls() {
    if (!camera_manager_) return;
    
    // Discovery and connection controls
    if (camera_manager_->IsDiscovering()) {
        // Show animated discovery button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        
        static float discovery_time = 0.0f;
        discovery_time += ImGui::GetIO().DeltaTime;
        int dots = static_cast<int>(discovery_time * 2.0f) % 4;
        std::string text = "🔍 Discovering";
        for (int i = 0; i < dots; i++) text += ".";
        
        ImGui::Button(text.c_str(), ImVec2(150, 35));
        ImGui::PopStyleColor(3);
    } else {
        if (ImGui::Button("🔍 Discover Cameras", ImVec2(150, 35))) {
            camera_manager_->DiscoverCameras([this](const std::string& msg) {
                LogMessage(msg);
            });
        }
    }
    
    ImGui::SameLine();
    
    // Connect/Disconnect button
    bool has_connected = camera_manager_->GetConnectedCount() > 0;
    if (has_connected) {
        if (ImGui::Button("❌ Disconnect All", ImVec2(150, 35))) {
            camera_manager_->DisconnectAllCameras();
            LogMessage("[CAMERA] Disconnected all cameras");
        }
    } else {
        if (camera_manager_->IsConnecting()) {
            // Show animated connection button
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            
            static float connection_time = 0.0f;
            connection_time += ImGui::GetIO().DeltaTime;
            int dots = static_cast<int>(connection_time * 2.0f) % 4;
            std::string text = "🔗 Connecting";
            for (int i = 0; i < dots; i++) text += ".";
            
            ImGui::Button(text.c_str(), ImVec2(150, 35));
            ImGui::PopStyleColor(3);
        } else {
            if (ImGui::Button("🔗 Connect All", ImVec2(150, 35))) {
                camera_manager_->ConnectAllCameras([this](const std::string& msg) {
                    LogMessage(msg);
                });
            }
        }
    }
    
    ImGui::SameLine();
    
    // Quick capture button
    bool can_capture = has_connected && session_manager_ && session_manager_->HasActiveSession() && !camera_manager_->IsCapturing();
    if (can_capture) {
        if (ImGui::Button("📸 Quick Capture", ImVec2(150, 35))) {
            auto* session = session_manager_->GetCurrentSession();
            std::string sessionPath = session->GetNextCapturePath();
            
            camera_manager_->CaptureAllCamerasAsync(sessionPath, true, 750, [this](const std::string& msg) {
                LogMessage(msg);
            });
        }
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::Button("📸 Quick Capture", ImVec2(150, 35));
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            if (!has_connected) {
                ImGui::Text("Connect cameras first");
            } else if (!session_manager_ || !session_manager_->HasActiveSession()) {
                ImGui::Text("Start a session first");
            } else if (camera_manager_->IsCapturing()) {
                ImGui::Text("Capture in progress");
            }
            ImGui::EndTooltip();
        }
    }
}

void HardwarePanel::RenderCameraTable() {
    if (!camera_manager_) return;
    
    auto cameras = camera_manager_->GetDiscoveredCameras();
    if (cameras.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No cameras discovered. Click 'Discover Cameras' to search.");
        return;
    }
    
    if (ImGui::BeginTable("CameraTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Camera ID", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Serial", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        
        for (const auto& camera : cameras) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            
            // Status indicator
            if (camera.isConnected) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● Online");
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "○ Offline");
            }
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", camera.id.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", camera.serialNumber.c_str());
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", camera.modelName.c_str());
        }
        
        ImGui::EndTable();
    }
}

void HardwarePanel::RenderBluetoothControls() {
    if (!bluetooth_manager_) return;
    
    // Scan and connection controls
    if (is_scanning_bluetooth_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        
        static float scan_time = 0.0f;
        scan_time += ImGui::GetIO().DeltaTime;
        int dots = static_cast<int>(scan_time * 2.0f) % 4;
        std::string text = "📡 Scanning";
        for (int i = 0; i < dots; i++) text += ".";
        
        ImGui::Button(text.c_str(), ImVec2(120, 35));
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        if (ImGui::Button("⏹ Stop", ImVec2(80, 35))) {
            bluetooth_manager_->StopScanning();
            is_scanning_bluetooth_ = false;
            LogMessage("[BLE] Stopped scanning");
        }
    } else {
        if (ImGui::Button("📡 Scan Devices", ImVec2(120, 35))) {
            bluetooth_manager_->StartScanning();
            is_scanning_bluetooth_ = true;
            LogMessage("[BLE] Started scanning for devices");
        }
    }
    
    ImGui::SameLine();
    
    // Connect/Disconnect button
    if (IsBluetoothConnected()) {
        if (ImGui::Button("❌ Disconnect", ImVec2(120, 35))) {
            if (!selected_bluetooth_device_id_.empty()) {
                bluetooth_manager_->DisconnectDevice(selected_bluetooth_device_id_);
                selected_bluetooth_device_id_.clear();
                LogMessage("[BLE] Disconnected turntable");
            }
        }
    } else {
        bool can_connect = !selected_bluetooth_device_id_.empty() && !is_connecting_bluetooth_;
        if (can_connect) {
            if (ImGui::Button("🔗 Connect", ImVec2(120, 35))) {
                is_connecting_bluetooth_ = true;
                bool success = bluetooth_manager_->ConnectToDevice(selected_bluetooth_device_id_);
                is_connecting_bluetooth_ = false;
                if (success) {
                    LogMessage("[BLE] Successfully connected to device");
                } else {
                    LogMessage("[BLE] Failed to connect to device");
                }
            }
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            ImGui::Button("🔗 Connect", ImVec2(120, 35));
            ImGui::PopStyleVar();
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                if (selected_bluetooth_device_id_.empty()) {
                    ImGui::Text("Select a device first");
                } else if (is_connecting_bluetooth_) {
                    ImGui::Text("Connection in progress");
                }
                ImGui::EndTooltip();
            }
        }
    }
}

void HardwarePanel::RenderBluetoothDeviceList() {
    if (!bluetooth_manager_) return;
    
    auto device_pairs = bluetooth_manager_->GetDiscoveredDevices();
    if (device_pairs.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No devices found. Click 'Scan Devices' to search for turntables.");
        return;
    }
    
    if (ImGui::BeginTable("BluetoothTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Device Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Device ID", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        
        for (const auto& device_pair : device_pairs) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            
            const std::string& device_id = device_pair.first;
            const std::string& device_name = device_pair.second;
            
            // Status indicator
            bool is_connected = bluetooth_manager_->IsDeviceConnected(device_id);
            if (is_connected) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "● Connected");
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "○ Available");
            }
            
            ImGui::TableSetColumnIndex(1);
            bool is_selected = (device_id == selected_bluetooth_device_id_);
            if (ImGui::Selectable(device_name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                selected_bluetooth_device_id_ = device_id;
            }
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", device_id.c_str());
        }
        
        ImGui::EndTable();
    }
}

void HardwarePanel::RenderBluetoothAdvancedControls() {
    if (!bluetooth_manager_ || !IsBluetoothConnected()) return;
    
    if (ImGui::CollapsingHeader("🎛️ Turntable Controls")) {
        ImGui::Text("Quick Test Controls:");
        
        // Test rotation buttons
        if (ImGui::Button("↺ +15°", ImVec2(80, 30))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                bluetooth_manager_->SendCommand(devices[0], BluetoothCommands::RotateByAngle(15.0f));
                LogMessage("[TURNTABLE] Rotating +15°");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("↻ -15°", ImVec2(80, 30))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                bluetooth_manager_->SendCommand(devices[0], BluetoothCommands::RotateByAngle(-15.0f));
                LogMessage("[TURNTABLE] Rotating -15°");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("🏠 Home", ImVec2(80, 30))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                bluetooth_manager_->SendCommand(devices[0], BluetoothCommands::ReturnToZero());
                LogMessage("[TURNTABLE] Returning to home position");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("⏹ Stop", ImVec2(80, 30))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                bluetooth_manager_->SendCommand(devices[0], BluetoothCommands::StopRotation());
                LogMessage("[TURNTABLE] Emergency stop");
            }
        }
        
        ImGui::Spacing();
        
        // Speed control
        static float rotation_speed = 70.0f;
        ImGui::Text("Rotation Speed:");
        if (ImGui::SliderFloat("##speed", &rotation_speed, 35.64f, 131.0f, "%.1f")) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                bluetooth_manager_->SendCommand(devices[0], BluetoothCommands::SetRotationSpeed(rotation_speed));
                LogMessage("[TURNTABLE] Speed set to " + std::to_string(rotation_speed));
            }
        }
    }
}

bool HardwarePanel::IsBluetoothConnected() const {
    return bluetooth_manager_ && bluetooth_manager_->IsConnected();
}

bool HardwarePanel::AreCamerasConnected() const {
    return camera_manager_ && camera_manager_->GetConnectedCount() > 0;
}

int HardwarePanel::GetConnectedCameraCount() const {
    return camera_manager_ ? camera_manager_->GetConnectedCount() : 0;
}

void HardwarePanel::LogMessage(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
}

} // namespace SaperaCapturePro