#include "HardwarePanel.hpp"
#include "../utils/SessionManager.hpp"
#include "../bluetooth/BluetoothCommands.hpp"
#include "../hardware/CameraTypes.hpp"
#include <algorithm>
#include <thread>
#include <chrono>

namespace SaperaCapturePro {

HardwarePanel::HardwarePanel() = default;

HardwarePanel::~HardwarePanel() {
    Shutdown();
}

void HardwarePanel::Initialize(BluetoothManager* bluetooth_manager, CameraManager* camera_manager, SessionManager* session_manager, SettingsManager* settings_manager) {
    bluetooth_manager_ = bluetooth_manager;
    camera_manager_ = camera_manager;
    session_manager_ = session_manager;
    settings_manager_ = settings_manager;
}

void HardwarePanel::Shutdown() {
    bluetooth_manager_ = nullptr;
    camera_manager_ = nullptr;
    session_manager_ = nullptr;
    settings_manager_ = nullptr;
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
    
    // Quick Connect All button
    RenderQuickConnectButton();
    ImGui::Spacing();
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
                    StoreLastConnectionInfo(); // Save connection info for quick reconnect
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
        if (ImGui::SliderFloat("##speed", &rotation_speed, 35.64f, 131.0f, "%.1fs/360°")) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                bluetooth_manager_->SendCommand(devices[0], BluetoothCommands::SetRotationSpeed(rotation_speed));
                LogMessage("[TURNTABLE] Speed set to " + std::to_string(rotation_speed) + "s/360° (≈" + std::to_string(360.0f/rotation_speed) + "°/s)");
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Speed = seconds for complete 360° rotation");
            ImGui::Text("Current: %.1fs/360° = %.2f°/second", rotation_speed, 360.0f/rotation_speed);
            ImGui::Text("Lower values = faster rotation");
            ImGui::EndTooltip();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Custom angle rotation
        static float custom_angle = 10.0f;
        ImGui::Text("Custom Angle Rotation:");
        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputFloat("##angle", &custom_angle, 0.1f, 1.0f, "%.1f°");
        
        ImGui::SameLine();
        if (ImGui::Button("↻ CW", ImVec2(60, 30))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                std::string command = "+CT,TURNANGLE=" + std::to_string(custom_angle) + ";";
                bluetooth_manager_->SendCommand(devices[0], command);
                float est_time = (custom_angle * rotation_speed) / 360.0f;
                LogMessage("[TURNTABLE] Rotating clockwise " + std::to_string(custom_angle) + "° (≈" + std::to_string(est_time) + "s)");
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("↺ CCW", ImVec2(60, 30))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                std::string command = "+CT,TURNANGLE=-" + std::to_string(custom_angle) + ";";
                bluetooth_manager_->SendCommand(devices[0], command);
                float est_time = (custom_angle * rotation_speed) / 360.0f;
                LogMessage("[TURNTABLE] Rotating counter-clockwise " + std::to_string(custom_angle) + "° (≈" + std::to_string(est_time) + "s)");
            }
        }
        
        // Show rotation time estimate
        ImGui::SameLine();
        float est_rotation_time = (custom_angle * rotation_speed) / 360.0f;
        ImGui::Text("≈%.1fs", est_rotation_time);
        
        // Quick angle presets
        ImGui::Spacing();
        ImGui::Text("Quick Presets:");
        
        // First row of presets
        if (ImGui::Button("1°", ImVec2(40, 25))) { custom_angle = 1.0f; }
        ImGui::SameLine();
        if (ImGui::Button("5°", ImVec2(40, 25))) { custom_angle = 5.0f; }
        ImGui::SameLine();
        if (ImGui::Button("10°", ImVec2(40, 25))) { custom_angle = 10.0f; }
        ImGui::SameLine();
        if (ImGui::Button("15°", ImVec2(40, 25))) { custom_angle = 15.0f; }
        
        // Second row of presets
        if (ImGui::Button("30°", ImVec2(40, 25))) { custom_angle = 30.0f; }
        ImGui::SameLine();
        if (ImGui::Button("45°", ImVec2(40, 25))) { custom_angle = 45.0f; }
        ImGui::SameLine();
        if (ImGui::Button("90°", ImVec2(40, 25))) { custom_angle = 90.0f; }
        ImGui::SameLine();
        if (ImGui::Button("180°", ImVec2(40, 25))) { custom_angle = 180.0f; }
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

void HardwarePanel::RenderQuickConnectButton() {
    if (!settings_manager_) return;
    
    auto& app_settings = settings_manager_->GetAppSettings();
    bool has_last_connection = !app_settings.last_bluetooth_device_id.empty();
    bool both_connected = IsBluetoothConnected() && AreCamerasConnected();
    
    // Large Quick Connect button
    ImVec2 button_size(200, 45);
    
    if (both_connected) {
        // Show "Disconnect All" when everything is connected
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
        
        if (ImGui::Button("❌ Disconnect All", button_size)) {
            // Disconnect everything
            if (bluetooth_manager_) {
                auto devices = bluetooth_manager_->GetConnectedDevices();
                for (const auto& device_id : devices) {
                    bluetooth_manager_->DisconnectDevice(device_id);
                }
            }
            if (camera_manager_) {
                camera_manager_->DisconnectAllCameras();
            }
            LogMessage("[QUICK] Disconnected all hardware");
        }
        ImGui::PopStyleColor(3);
    } else if (has_last_connection) {
        // Show "Quick Connect All" when we have saved connection info
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
        
        std::string button_text = "⚡ Quick Connect All";
        if (!app_settings.last_bluetooth_device_name.empty()) {
            button_text += "\n(" + app_settings.last_bluetooth_device_name + " + Cameras)";
        }
        
        if (ImGui::Button(button_text.c_str(), button_size)) {
            QuickConnectAll();
        }
        ImGui::PopStyleColor(3);
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Connect to last used turntable and discover all cameras");
            if (!app_settings.last_bluetooth_device_name.empty()) {
                ImGui::Text("Last turntable: %s", app_settings.last_bluetooth_device_name.c_str());
            }
            ImGui::EndTooltip();
        }
    } else {
        // Show disabled button when no saved connection
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::Button("⚡ Quick Connect All\n(No saved connection)", button_size);
        ImGui::PopStyleVar();
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Connect to bluetooth and cameras first");
            ImGui::Text("Your connection will be saved for quick access");
            ImGui::EndTooltip();
        }
    }
    
    // Show quick connect toggle
    ImGui::SameLine();
    ImGui::Checkbox("Auto-connect enabled", &app_settings.auto_connect_enabled);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Save connection info for quick reconnection");
        ImGui::EndTooltip();
    }
    
    // Save settings if changed
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        settings_manager_->Save();
    }
}

void HardwarePanel::QuickConnectAll() {
    if (!settings_manager_ || !bluetooth_manager_ || !camera_manager_) {
        LogMessage("[QUICK] ERROR: Managers not initialized");
        return;
    }
    
    auto& app_settings = settings_manager_->GetAppSettings();
    if (app_settings.last_bluetooth_device_id.empty()) {
        LogMessage("[QUICK] ERROR: No saved bluetooth connection");
        return;
    }
    
    LogMessage("[QUICK] ⚡ Starting Quick Connect All...");
    
    // Step 1: Connect to saved bluetooth device
    if (!IsBluetoothConnected()) {
        LogMessage("[QUICK] 🔗 Connecting to turntable: " + app_settings.last_bluetooth_device_name);
        
        bool bt_success = bluetooth_manager_->ConnectToDevice(app_settings.last_bluetooth_device_id);
        if (!bt_success) {
            LogMessage("[QUICK] ❌ Failed to connect to saved turntable - try scanning first");
            return;
        }
        
        LogMessage("[QUICK] ✅ Turntable connected successfully");
        selected_bluetooth_device_id_ = app_settings.last_bluetooth_device_id;
    } else {
        LogMessage("[QUICK] 🔗 Turntable already connected");
    }
    
    // Step 2: Discover and connect all cameras
    if (camera_manager_->GetConnectedCount() == 0) {
        LogMessage("[QUICK] 📷 Discovering cameras...");
        
        camera_manager_->DiscoverCameras([this](const std::string& msg) {
            LogMessage("[QUICK] " + msg);
        });
        
        // Wait a moment for discovery to complete (this is async)
        // In a real implementation, you might want to use a callback or polling
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        auto cameras = camera_manager_->GetDiscoveredCameras();
        if (cameras.empty()) {
            LogMessage("[QUICK] ⚠️ No cameras discovered");
        } else {
            LogMessage("[QUICK] 🔗 Connecting to " + std::to_string(cameras.size()) + " camera(s)...");
            
            camera_manager_->ConnectAllCameras([this](const std::string& msg) {
                LogMessage("[QUICK] " + msg);
            });
            
            LogMessage("[QUICK] ✅ Camera connection initiated");
        }
    } else {
        LogMessage("[QUICK] 📷 Cameras already connected (" + std::to_string(camera_manager_->GetConnectedCount()) + ")");
    }
    
    LogMessage("[QUICK] 🎯 Quick Connect All completed!");
}

void HardwarePanel::StoreLastConnectionInfo() {
    if (!settings_manager_ || !bluetooth_manager_) return;
    
    auto& app_settings = settings_manager_->GetAppSettings();
    if (!app_settings.auto_connect_enabled) return;
    
    // Store currently connected bluetooth device
    auto connected_devices = bluetooth_manager_->GetConnectedDevices();
    if (!connected_devices.empty()) {
        app_settings.last_bluetooth_device_id = connected_devices[0];
        
        // Get device name from discovered devices
        auto device_pairs = bluetooth_manager_->GetDiscoveredDevices();
        for (const auto& [id, name] : device_pairs) {
            if (id == connected_devices[0]) {
                app_settings.last_bluetooth_device_name = name;
                break;
            }
        }
        
        // Save settings
        settings_manager_->Save();
        LogMessage("[QUICK] 💾 Saved connection info for: " + app_settings.last_bluetooth_device_name);
    }
}

void HardwarePanel::LogMessage(const std::string& message) {
    if (log_callback_) {
        log_callback_(message);
    }
}

} // namespace SaperaCapturePro