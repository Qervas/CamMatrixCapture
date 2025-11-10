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

    RenderContent();

    ImGui::End();
}

void HardwarePanel::RenderContent() {
    const float em = ImGui::GetFontSize();
    const ImVec2 cardPadding(1.0f * em, 0.75f * em);
    const float cardSpacing = 0.75f * em;

    // Card 1: Status Overview
    ImGui::BeginChild("card_status", ImVec2(0, 2.5f * em), true);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.5f * em, 0.5f * em));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 0.25f * em);
    ImGui::Text("Hardware Status:");
    ImGui::SameLine();
    if (AreCamerasConnected()) {
        ImGui::TextColored(ImVec4(0.12f, 0.75f, 0.35f, 1.0f), "● %d Cameras", GetConnectedCameraCount());
    } else {
        ImGui::TextColored(ImVec4(0.85f, 0.6f, 0.1f, 1.0f), "○ No Cameras");
    }
    ImGui::SameLine();
    ImGui::Text("•");
    ImGui::SameLine();
    if (IsBluetoothConnected()) {
        ImGui::TextColored(ImVec4(0.12f, 0.75f, 0.35f, 1.0f), "● Bluetooth Connected");
    } else {
        ImGui::TextColored(ImVec4(0.85f, 0.6f, 0.1f, 1.0f), "○ No Bluetooth");
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0, cardSpacing));

    // Card 2: Quick Actions (ensure tall enough to avoid scrolling)
    ImGui::BeginChild("card_actions", ImVec2(0, 6.5f * em), true);
    RenderQuickConnectButton();
    ImGui::Spacing();
    RenderCameraControls();
    ImGui::EndChild();

    ImGui::Dummy(ImVec2(0, cardSpacing));

    // Card 3: Tabbed Controls
    ImGui::BeginChild("card_tabs", ImVec2(0, 0), true);
    if (ImGui::BeginTabBar("HardwareTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("◆ Cameras")) {
            RenderCameraTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("◉ Bluetooth Turntable")) {
            RenderBluetoothTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
}

void HardwarePanel::RenderCameraTab() {
    const float em = ImGui::GetFontSize();
    ImGui::Text("Sapera SDK Camera System");
    ImGui::Separator();
    ImGui::Spacing();

    // Camera ordering controls
    if (settings_manager_) {
        auto& order_settings = settings_manager_->GetCameraOrderSettings();

        ImGui::BeginChild("cam_order_controls", ImVec2(0, 2.5f * em), true);
        ImGui::Checkbox("Enable Custom Camera Ordering", &order_settings.use_custom_ordering);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reorder cameras to match physical layout.\nDrag rows up/down to reorder.");
        }

        if (order_settings.use_custom_ordering) {
            ImGui::SameLine();
            if (ImGui::Button("Apply Ordering")) {
                camera_manager_->ApplyCameraOrdering(order_settings);
                settings_manager_->Save();
                LogMessage("[ORDER] Camera ordering applied and saved");
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
    }

    // List
    ImGui::BeginChild("cam_list", ImVec2(0, 0), true);
    RenderCameraTable();
    ImGui::EndChild();
}

void HardwarePanel::RenderBluetoothTab() {
    const float em = ImGui::GetFontSize();
    ImGui::Text("Turntable Bluetooth Control");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("ble_controls", ImVec2(0, 3.5f * em), false);
    RenderBluetoothControls();
    ImGui::EndChild();

    ImGui::Spacing();

    ImGui::BeginChild("ble_list", ImVec2(0, 0), true);
    RenderBluetoothDeviceList();

    if (IsBluetoothConnected()) {
        ImGui::Spacing();
        ImGui::Separator();
        RenderBluetoothAdvancedControls();
    }

    ImGui::EndChild();
}

void HardwarePanel::RenderCameraControls() {
    if (!camera_manager_) return;

    const float em = ImGui::GetFontSize();
    const ImVec2 btn(9.0f * em, 2.2f * em);

    // Discovery and connection controls
    if (camera_manager_->IsDiscovering()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        static float discovery_time = 0.0f;
        discovery_time += ImGui::GetIO().DeltaTime;
        int dots = static_cast<int>(discovery_time * 2.0f) % 4;
        std::string text = "🔍 Discovering";
        for (int i = 0; i < dots; i++) text += ".";
        ImGui::Button(text.c_str(), btn);
        ImGui::PopStyleColor(3);
    } else {
        if (ImGui::Button("🔍 Discover Cameras", btn)) {
            camera_manager_->DiscoverCameras([this](const std::string& msg) {
                LogMessage(msg);
                // Auto-apply camera ordering after discovery completes
                if (msg.find("Discovery complete") != std::string::npos) {
                    if (settings_manager_) {
                        auto& order_settings = settings_manager_->GetCameraOrderSettings();
                        if (order_settings.use_custom_ordering) {
                            camera_manager_->ApplyCameraOrdering(order_settings);
                            LogMessage("[ORDER] Applied camera ordering from config");
                        }
                    }
                }
            });
        }
    }

    ImGui::SameLine();

    // Connect/Disconnect button
    bool has_connected = camera_manager_->GetConnectedCount() > 0;
    if (has_connected) {
        if (ImGui::Button("❌ Disconnect Cameras", btn)) {
            camera_manager_->DisconnectAllCameras();
            LogMessage("[CAMERA] Disconnected all cameras");
        }
    } else {
        if (camera_manager_->IsConnecting()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            static float connection_time = 0.0f;
            connection_time += ImGui::GetIO().DeltaTime;
            int dots = static_cast<int>(connection_time * 2.0f) % 4;
            std::string text = "🔗 Connecting";
            for (int i = 0; i < dots; i++) text += ".";
            ImGui::Button(text.c_str(), btn);
            ImGui::PopStyleColor(3);
        } else {
            if (ImGui::Button("🔗 Connect All", btn)) {
                camera_manager_->ConnectAllCameras([this](const std::string& msg) {
                    LogMessage(msg);
                });
            }
        }
    }

    // Quick Capture button removed by user request
}

void HardwarePanel::RenderCameraTable() {
    if (!camera_manager_ || !settings_manager_) return;
    const float em = ImGui::GetFontSize();

    auto cameras = camera_manager_->GetDiscoveredCameras();
    if (cameras.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No cameras discovered. Click 'Discover Cameras' to search.");
        return;
    }

    auto& order_settings = settings_manager_->GetCameraOrderSettings();
    bool ordering_enabled = order_settings.use_custom_ordering;

    // Initialize ordering for newly discovered cameras
    if (ordering_enabled) {
        for (size_t i = 0; i < cameras.size(); ++i) {
            if (!order_settings.HasCamera(cameras[i].serialNumber)) {
                order_settings.SetDisplayPosition(cameras[i].serialNumber, static_cast<int>(i));
            }
        }
        // Auto-apply ordering to update camera names
        camera_manager_->ApplyCameraOrdering(order_settings);
    } else {
        // Reset ordering if disabled
        camera_manager_->ApplyCameraOrdering(order_settings);
    }

    // Get ordered camera list
    auto display_cameras = ordering_enabled ? camera_manager_->GetOrderedCameras() : cameras;

    int column_count = ordering_enabled ? 6 : 4;
    if (ImGui::BeginTable("CameraTable", column_count, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        if (ordering_enabled) {
            ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 4.0f * em);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 6.0f * em);
        }
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 8.0f * em);
        ImGui::TableSetupColumn("Camera Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Serial", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < display_cameras.size(); ++i) {
            const auto& camera = display_cameras[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();

            if (ordering_enabled) {
                // Order column
                ImGui::TableSetColumnIndex(0);
                int current_pos = order_settings.GetDisplayPosition(camera.serialNumber);
                if (current_pos < 0) current_pos = static_cast<int>(i);
                ImGui::Text("%d", current_pos + 1);

                // Actions column
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

                // Move up button
                if (i > 0) {
                    if (ImGui::SmallButton("↑")) {
                        // Swap positions
                        const auto& prev_camera = display_cameras[i - 1];
                        int prev_pos = order_settings.GetDisplayPosition(prev_camera.serialNumber);
                        int curr_pos = order_settings.GetDisplayPosition(camera.serialNumber);

                        if (prev_pos < 0) prev_pos = static_cast<int>(i - 1);
                        if (curr_pos < 0) curr_pos = static_cast<int>(i);

                        order_settings.SetDisplayPosition(prev_camera.serialNumber, curr_pos);
                        order_settings.SetDisplayPosition(camera.serialNumber, prev_pos);
                        LogMessage("[ORDER] Moved camera up: " + camera.serialNumber);
                    }
                } else {
                    ImGui::Dummy(ImVec2(20, 20));
                }

                ImGui::SameLine();

                // Move down button
                if (i < display_cameras.size() - 1) {
                    if (ImGui::SmallButton("↓")) {
                        // Swap positions
                        const auto& next_camera = display_cameras[i + 1];
                        int next_pos = order_settings.GetDisplayPosition(next_camera.serialNumber);
                        int curr_pos = order_settings.GetDisplayPosition(camera.serialNumber);

                        if (next_pos < 0) next_pos = static_cast<int>(i + 1);
                        if (curr_pos < 0) curr_pos = static_cast<int>(i);

                        order_settings.SetDisplayPosition(next_camera.serialNumber, curr_pos);
                        order_settings.SetDisplayPosition(camera.serialNumber, next_pos);
                        LogMessage("[ORDER] Moved camera down: " + camera.serialNumber);
                    }
                } else {
                    ImGui::Dummy(ImVec2(20, 20));
                }

                ImGui::PopStyleVar();
            }

            // Status column
            ImGui::TableSetColumnIndex(ordering_enabled ? 2 : 0);
            if (camera.isConnected) {
                ImGui::TextColored(ImVec4(0.12f, 0.75f, 0.35f, 1.0f), "● Online");
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "○ Offline");
            }

            // Camera Name column
            ImGui::TableSetColumnIndex(ordering_enabled ? 3 : 1);
            ImGui::Text("%s", camera.name.c_str());

            // Serial column
            ImGui::TableSetColumnIndex(ordering_enabled ? 4 : 2);
            ImGui::Text("%s", camera.serialNumber.c_str());

            // Model column
            ImGui::TableSetColumnIndex(ordering_enabled ? 5 : 3);
            ImGui::Text("%s", camera.modelName.c_str());

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

void HardwarePanel::RenderBluetoothControls() {
    if (!bluetooth_manager_) return;
    const float em = ImGui::GetFontSize();
    const ImVec2 btnSmall(6.5f * em, 2.2f * em);

    if (is_scanning_bluetooth_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        static float scan_time = 0.0f;
        scan_time += ImGui::GetIO().DeltaTime;
        int dots = static_cast<int>(scan_time * 2.0f) % 4;
        std::string text = "📡 Scanning";
        for (int i = 0; i < dots; i++) text += ".";
        ImGui::Button(text.c_str(), btnSmall);
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        if (ImGui::Button("⏹ Stop", btnSmall)) {
            bluetooth_manager_->StopScanning();
            is_scanning_bluetooth_ = false;
            LogMessage("[BLE] Stopped scanning");
        }
    } else {
        if (ImGui::Button("📡 Scan Devices", btnSmall)) {
            bluetooth_manager_->StartScanning();
            is_scanning_bluetooth_ = true;
            LogMessage("[BLE] Started scanning for devices");
        }
    }

    ImGui::SameLine();

    if (IsBluetoothConnected()) {
        if (ImGui::Button("❌ Disconnect", btnSmall)) {
            if (!selected_bluetooth_device_id_.empty()) {
                bluetooth_manager_->DisconnectDevice(selected_bluetooth_device_id_);
                selected_bluetooth_device_id_.clear();
                LogMessage("[BLE] Disconnected turntable");
            }
        }
    } else {
        bool can_connect = !selected_bluetooth_device_id_.empty() && !is_connecting_bluetooth_;
        if (can_connect) {
            if (ImGui::Button("🔗 Connect", btnSmall)) {
                is_connecting_bluetooth_ = true;
                bool success = bluetooth_manager_->ConnectToDevice(selected_bluetooth_device_id_);
                is_connecting_bluetooth_ = false;
                if (success) {
                    LogMessage("[BLE] Successfully connected to device");
                    StoreLastConnectionInfo();
                } else {
                    LogMessage("[BLE] Failed to connect to device");
                }
            }
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            ImGui::Button("🔗 Connect", btnSmall);
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

    const float em = ImGui::GetFontSize();
    if (ImGui::BeginTable("BluetoothTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 10.0f * em);
        ImGui::TableSetupColumn("Device Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Device ID", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& device_pair : device_pairs) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const std::string& device_id = device_pair.first;
            const std::string& device_name = device_pair.second;
            bool is_connected = bluetooth_manager_->IsDeviceConnected(device_id);
            if (is_connected) {
                ImGui::TextColored(ImVec4(0.12f, 0.75f, 0.35f, 1.0f), "● Connected");
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

    const float em = ImGui::GetFontSize();

    if (ImGui::CollapsingHeader("🎛️ Turntable Controls")) {
        ImGui::Text("Quick Test Controls:");
        if (ImGui::Button("↺ +15°", ImVec2(5.0f * em, 2.0f * em))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                bluetooth_manager_->SendCommand(devices[0], BluetoothCommands::RotateByAngle(15.0f));
                LogMessage("[TURNTABLE] Rotating +15°");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("↻ -15°", ImVec2(5.0f * em, 2.0f * em))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                bluetooth_manager_->SendCommand(devices[0], BluetoothCommands::RotateByAngle(-15.0f));
                LogMessage("[TURNTABLE] Rotating -15°");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("🏠 Home", ImVec2(5.0f * em, 2.0f * em))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                bluetooth_manager_->SendCommand(devices[0], BluetoothCommands::ReturnToZero());
                LogMessage("[TURNTABLE] Returning to home position");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("⏹ Stop", ImVec2(5.0f * em, 2.0f * em))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                bluetooth_manager_->SendCommand(devices[0], BluetoothCommands::StopRotation());
                LogMessage("[TURNTABLE] Emergency stop");
            }
        }

        ImGui::Spacing();
        static float rotation_speed = 70.0f;
        ImGui::Text("Rotation Speed:");
        ImGui::SetNextItemWidth(12.0f * em);
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

        static float custom_angle = 10.0f;
        ImGui::Text("Custom Angle Rotation:");
        ImGui::SetNextItemWidth(8.0f * em);
        ImGui::InputFloat("##angle", &custom_angle, 0.1f, 1.0f, "%.1f°");
        ImGui::SameLine();
        if (ImGui::Button("↻ CW", ImVec2(4.5f * em, 2.0f * em))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                std::string command = "+CT,TURNANGLE=" + std::to_string(custom_angle) + ";";
                bluetooth_manager_->SendCommand(devices[0], command);
                float est_time = (custom_angle * rotation_speed) / 360.0f;
                LogMessage("[TURNTABLE] Rotating clockwise " + std::to_string(custom_angle) + "° (≈" + std::to_string(est_time) + "s)");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("↺ CCW", ImVec2(4.5f * em, 2.0f * em))) {
            auto devices = bluetooth_manager_->GetConnectedDevices();
            if (!devices.empty()) {
                std::string command = "+CT,TURNANGLE=-" + std::to_string(custom_angle) + ";";
                bluetooth_manager_->SendCommand(devices[0], command);
                float est_time = (custom_angle * rotation_speed) / 360.0f;
                LogMessage("[TURNTABLE] Rotating counter-clockwise " + std::to_string(custom_angle) + "° (≈" + std::to_string(est_time) + "s)");
            }
        }
        ImGui::SameLine();
        float est_rotation_time = (custom_angle * rotation_speed) / 360.0f;
        ImGui::Text("≈%.1fs", est_rotation_time);

        ImGui::Spacing();
        ImGui::Text("Quick Presets:");
        if (ImGui::Button("1°", ImVec2(3.5f * em, 1.8f * em))) { custom_angle = 1.0f; }
        ImGui::SameLine();
        if (ImGui::Button("5°", ImVec2(3.5f * em, 1.8f * em))) { custom_angle = 5.0f; }
        ImGui::SameLine();
        if (ImGui::Button("10°", ImVec2(3.5f * em, 1.8f * em))) { custom_angle = 10.0f; }
        ImGui::SameLine();
        if (ImGui::Button("15°", ImVec2(3.5f * em, 1.8f * em))) { custom_angle = 15.0f; }

        if (ImGui::Button("30°", ImVec2(3.5f * em, 1.8f * em))) { custom_angle = 30.0f; }
        ImGui::SameLine();
        if (ImGui::Button("45°", ImVec2(3.5f * em, 1.8f * em))) { custom_angle = 45.0f; }
        ImGui::SameLine();
        if (ImGui::Button("90°", ImVec2(3.5f * em, 1.8f * em))) { custom_angle = 90.0f; }
        ImGui::SameLine();
        if (ImGui::Button("180°", ImVec2(3.5f * em, 1.8f * em))) { custom_angle = 180.0f; }
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

    const float em = ImGui::GetFontSize();
    ImVec2 button_size(12.0f * em, 2.6f * em);

    if (both_connected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("❌ Disconnect All", button_size)) {
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
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
        if (ImGui::Button("⚡ Quick Connect", button_size)) {
            QuickConnectAll();
        }
        ImGui::PopStyleColor(3);
        
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::Button("⚡ Quick Connect", button_size);
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Connect to bluetooth and cameras first");
            ImGui::EndTooltip();
        }
    }
    // Removed auto-connect checkbox to declutter header
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

    if (camera_manager_->GetConnectedCount() == 0) {
        LogMessage("[QUICK] 📷 Discovering cameras...");
        camera_manager_->DiscoverCameras([this](const std::string& msg) {
            LogMessage("[QUICK] " + msg);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Apply camera ordering after discovery
        if (settings_manager_) {
            auto& order_settings = settings_manager_->GetCameraOrderSettings();
            if (order_settings.use_custom_ordering) {
                camera_manager_->ApplyCameraOrdering(order_settings);
                LogMessage("[QUICK] Applied camera ordering from config");
            }
        }

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

    auto connected_devices = bluetooth_manager_->GetConnectedDevices();
    if (!connected_devices.empty()) {
        app_settings.last_bluetooth_device_id = connected_devices[0];
        auto device_pairs = bluetooth_manager_->GetDiscoveredDevices();
        for (const auto& [id, name] : device_pairs) {
            if (id == connected_devices[0]) {
                app_settings.last_bluetooth_device_name = name;
                break;
            }
        }
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