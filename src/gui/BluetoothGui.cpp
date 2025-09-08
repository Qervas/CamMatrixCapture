#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include "BluetoothGui.hpp"
#include "../bluetooth/BluetoothManager.hpp"
#include "../bluetooth/BluetoothCommands.hpp"
#include <thread>
#include <cstring>

namespace SaperaCapturePro {

BluetoothGui::BluetoothGui() {
    UpdateStepAngle();
}

BluetoothGui::~BluetoothGui() {
    Shutdown();
}

void BluetoothGui::Initialize(BluetoothManager* manager) {
    m_bluetoothManager = manager;
    
    if (m_bluetoothManager) {
        // Set callbacks
        m_bluetoothManager->SetDeviceDiscoveredCallback(
            [this](const std::string& id, const std::string& name) {
                OnDeviceDiscovered(id, name);
            }
        );
        
        m_bluetoothManager->SetConnectionStatusCallback(
            [this](const std::string& id, bool connected) {
                OnConnectionStatusChanged(id, connected);
            }
        );
        
        // Load current settings
        strcpy(m_serviceUuid, m_bluetoothManager->GetServiceUUID().c_str());
        strcpy(m_charUuid, m_bluetoothManager->GetCharacteristicUUID().c_str());
    }
}

void BluetoothGui::Shutdown() {
    if (m_bluetoothManager && m_isScanning) {
        StopScanning();
    }
}

void BluetoothGui::Render(bool* show_panel) {
    ImGui::Begin("🔷 Bluetooth Control", show_panel, ImGuiWindowFlags_NoCollapse);
    
    // Status bar
    ImGui::Text("Status:");
    ImGui::SameLine();
    if (!m_connectedDeviceId.empty()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
        ImGui::SameLine();
        ImGui::Text("- %s", m_connectedDeviceId.c_str());
    } else if (m_isConnecting) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Connecting...");
    } else if (m_isScanning) {
        ImGui::TextColored(ImVec4(0.0f, 0.5f, 1.0f, 1.0f), "Scanning...");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Disconnected");
    }
    
    ImGui::Separator();
    
    // Tab bar for different sections
    if (ImGui::BeginTabBar("BluetoothTabs")) {
        RenderScannerTab();
        RenderControlTab();
        RenderCaptureSequenceTab();
        RenderSettingsTab();
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void BluetoothGui::OnDeviceDiscovered(const std::string& deviceId, const std::string& deviceName) {
    m_discoveredDevices.push_back({deviceId, deviceName});
}

void BluetoothGui::OnConnectionStatusChanged(const std::string& deviceId, bool connected) {
    if (connected) {
        m_connectedDeviceId = deviceId;
        m_isConnecting = false;
    } else {
        if (m_connectedDeviceId == deviceId) {
            m_connectedDeviceId = "";
        }
        m_isConnecting = false;
    }
}

void BluetoothGui::RenderScannerTab() {
    if (ImGui::BeginTabItem("Scanner")) {
        // Scan controls
        if (!m_isScanning) {
            if (ImGui::Button("Start Scan", ImVec2(120, 0))) {
                StartScanning();
            }
            ImGui::SameLine();
            ImGui::Text("Click to start scanning for devices");
        } else {
            if (ImGui::Button("Stop Scan", ImVec2(120, 0))) {
                StopScanning();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Scanning... (%zu devices found)", m_discoveredDevices.size());
        }
        
        ImGui::Separator();
        
        // Discovered devices list
        ImGui::Text("Discovered Devices:");
        ImGui::BeginChild("DeviceList", ImVec2(0, 200), true);
        
        for (const auto& [id, name] : m_discoveredDevices) {
            bool is_selected = (m_selectedDeviceId == id);
            bool is_connected = (m_connectedDeviceId == id);
            
            if (is_connected) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            }
            
            if (ImGui::Selectable(name.c_str(), is_selected)) {
                m_selectedDeviceId = id;
            }
            
            if (is_connected) {
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::Text(" [Connected]");
            }
        }
        
        ImGui::EndChild();
        
        // Connection controls
        if (!m_selectedDeviceId.empty()) {
            ImGui::Separator();
            
            if (m_connectedDeviceId == m_selectedDeviceId) {
                if (ImGui::Button("Disconnect", ImVec2(120, 0))) {
                    DisconnectCurrentDevice();
                }
            } else {
                if (ImGui::Button("Connect", ImVec2(120, 0))) {
                    ConnectToSelectedDevice();
                }
            }
        }
        
        ImGui::EndTabItem();
    }
}

void BluetoothGui::RenderControlTab() {
    if (ImGui::BeginTabItem("Control")) {
        if (!m_connectedDeviceId.empty()) {
            // Rotation controls
            ImGui::Text("Rotation Control:");
            
            // Rotation Angle - Slider + Input
            ImGui::PushItemWidth(150);
            ImGui::SliderFloat("##RotAngleSlider", &m_rotationAngle, -360.0f, 360.0f);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::InputFloat("Angle (°)", &m_rotationAngle, 0.1f, 1.0f, "%.1f");
            ImGui::PopItemWidth();
            
            // Rotation Speed - Slider + Input  
            ImGui::PushItemWidth(150);
            ImGui::SliderFloat("##RotSpeedSlider", &m_rotationSpeed, 35.64f, 131.0f);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::InputFloat("Speed", &m_rotationSpeed, 0.1f, 1.0f, "%.2f");
            ImGui::PopItemWidth();
            
            ClampValues();
            
            // Rotation buttons
            if (ImGui::Button("Rotate", ImVec2(80, 0))) {
                SendRotationCommand();
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop", ImVec2(80, 0))) {
                m_bluetoothManager->StopRotation(m_connectedDeviceId);
            }
            ImGui::SameLine();
            if (ImGui::Button("Zero", ImVec2(80, 0))) {
                m_bluetoothManager->ReturnToZero(m_connectedDeviceId);
            }
            
            // Quick rotation buttons
            ImGui::Text("Quick Rotate:");
            if (ImGui::Button("-90°", ImVec2(60, 0))) {
                m_bluetoothManager->RotateTurntable(m_connectedDeviceId, -90.0f);
            }
            ImGui::SameLine();
            if (ImGui::Button("-45°", ImVec2(60, 0))) {
                m_bluetoothManager->RotateTurntable(m_connectedDeviceId, -45.0f);
            }
            ImGui::SameLine();
            if (ImGui::Button("-30°", ImVec2(60, 0))) {
                m_bluetoothManager->RotateTurntable(m_connectedDeviceId, -30.0f);
            }
            ImGui::SameLine();
            if (ImGui::Button("+30°", ImVec2(60, 0))) {
                m_bluetoothManager->RotateTurntable(m_connectedDeviceId, 30.0f);
            }
            ImGui::SameLine();
            if (ImGui::Button("+45°", ImVec2(60, 0))) {
                m_bluetoothManager->RotateTurntable(m_connectedDeviceId, 45.0f);
            }
            ImGui::SameLine();
            if (ImGui::Button("+90°", ImVec2(60, 0))) {
                m_bluetoothManager->RotateTurntable(m_connectedDeviceId, 90.0f);
            }
            
            ImGui::Separator();
            
            // Tilt controls
            ImGui::Text("Tilt Control:");
            
            // Tilt Angle - Slider + Input
            ImGui::PushItemWidth(150);
            ImGui::SliderFloat("##TiltAngleSlider", &m_tiltAngle, -30.0f, 30.0f);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::InputFloat("Tilt (°)", &m_tiltAngle, 0.1f, 1.0f, "%.1f");
            ImGui::PopItemWidth();
            
            // Tilt Speed - Slider + Input
            ImGui::PushItemWidth(150);
            ImGui::SliderFloat("##TiltSpeedSlider", &m_tiltSpeed, 9.0f, 35.0f);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::InputFloat("T.Speed", &m_tiltSpeed, 0.1f, 1.0f, "%.1f");
            ImGui::PopItemWidth();
            
            ClampValues();
            
            // Tilt buttons
            if (ImGui::Button("Tilt", ImVec2(80, 0))) {
                SendTiltCommand();
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop Tilt", ImVec2(80, 0))) {
                m_bluetoothManager->StopTilt(m_connectedDeviceId);
            }
            ImGui::SameLine();
            if (ImGui::Button("Level", ImVec2(80, 0))) {
                m_bluetoothManager->TiltTurntable(m_connectedDeviceId, 0.0f);
            }
            
            ImGui::Separator();
            
            // Status query
            if (ImGui::Button("Get Status", ImVec2(120, 0))) {
                m_bluetoothManager->GetTurntableStatus(m_connectedDeviceId, 
                    [](const std::string& response) {
                        // Response callback - could be logged or displayed
                    });
            }
            
        } else {
            ImGui::Text("No device connected");
            ImGui::Text("Please connect to a device in the Scanner tab");
        }
        
        ImGui::EndTabItem();
    }
}

void BluetoothGui::RenderCaptureSequenceTab() {
    if (ImGui::BeginTabItem("Capture Sequence")) {
        if (!m_connectedDeviceId.empty()) {
            ImGui::Text("Automated Capture Sequence:");
            
            ImGui::Checkbox("Auto-rotate during capture", &m_autoRotateCapture);
            
            if (m_autoRotateCapture) {
                // Number of steps - Slider + Input
                ImGui::PushItemWidth(150);
                ImGui::SliderInt("##StepsSlider", &m_captureSteps, 1, 72);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::PushItemWidth(80);
                ImGui::InputInt("Steps", &m_captureSteps);
                ImGui::PopItemWidth();
                
                // Clamp steps
                if (m_captureSteps < 1) m_captureSteps = 1;
                if (m_captureSteps > 360) m_captureSteps = 360;
                
                UpdateStepAngle();
                ImGui::Text("Step angle: %.2f degrees", m_stepAngle);
                
                // Preset sequences
                ImGui::Text("Presets:");
                if (ImGui::Button("Quick (8 steps)", ImVec2(120, 0))) {
                    m_captureSteps = 8;
                    UpdateStepAngle();
                }
                ImGui::SameLine();
                if (ImGui::Button("Standard (12 steps)", ImVec2(120, 0))) {
                    m_captureSteps = 12;
                    UpdateStepAngle();
                }
                ImGui::SameLine();
                if (ImGui::Button("Detailed (24 steps)", ImVec2(120, 0))) {
                    m_captureSteps = 24;
                    UpdateStepAngle();
                }
                
                ImGui::Separator();
                
                if (ImGui::Button("Start Capture Sequence", ImVec2(200, 30))) {
                    StartCaptureSequence();
                }
            }
            
        } else {
            ImGui::Text("No device connected");
            ImGui::Text("Please connect to a device in the Scanner tab");
        }
        
        ImGui::EndTabItem();
    }
}

void BluetoothGui::RenderSettingsTab() {
    if (ImGui::BeginTabItem("Settings")) {
        ImGui::Text("Bluetooth Configuration:");
        
        ImGui::PushItemWidth(400);
        
        if (ImGui::InputText("Service UUID", m_serviceUuid, sizeof(m_serviceUuid))) {
            if (m_bluetoothManager) {
                m_bluetoothManager->SetServiceUUID(m_serviceUuid);
            }
        }
        
        if (ImGui::InputText("Characteristic UUID", m_charUuid, sizeof(m_charUuid))) {
            if (m_bluetoothManager) {
                m_bluetoothManager->SetCharacteristicUUID(m_charUuid);
            }
        }
        
        ImGui::PopItemWidth();
        
        ImGui::Separator();
        
        if (ImGui::Button("Save Settings", ImVec2(120, 0))) {
            if (m_bluetoothManager) {
                m_bluetoothManager->SaveSettings();
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Load Settings", ImVec2(120, 0))) {
            if (m_bluetoothManager) {
                m_bluetoothManager->LoadSettings();
                strcpy(m_serviceUuid, m_bluetoothManager->GetServiceUUID().c_str());
                strcpy(m_charUuid, m_bluetoothManager->GetCharacteristicUUID().c_str());
            }
        }
        
        ImGui::EndTabItem();
    }
}

void BluetoothGui::StartScanning() {
    if (m_bluetoothManager) {
        m_discoveredDevices.clear();
        m_bluetoothManager->StartScanning();
        m_isScanning = true;
    }
}

void BluetoothGui::StopScanning() {
    if (m_bluetoothManager) {
        m_bluetoothManager->StopScanning();
        m_isScanning = false;
    }
}

void BluetoothGui::ConnectToSelectedDevice() {
    if (m_bluetoothManager && !m_selectedDeviceId.empty()) {
        m_isConnecting = true;
        std::thread([this]() {
            bool success = m_bluetoothManager->ConnectToDevice(m_selectedDeviceId);
            m_isConnecting = false;
            // Connection status will be updated via callback
        }).detach();
    }
}

void BluetoothGui::DisconnectCurrentDevice() {
    if (m_bluetoothManager && !m_connectedDeviceId.empty()) {
        m_bluetoothManager->DisconnectDevice(m_connectedDeviceId);
    }
}

void BluetoothGui::SendRotationCommand() {
    if (m_bluetoothManager && !m_connectedDeviceId.empty()) {
        m_bluetoothManager->RotateTurntable(m_connectedDeviceId, m_rotationAngle);
    }
}

void BluetoothGui::SendTiltCommand() {
    if (m_bluetoothManager && !m_connectedDeviceId.empty()) {
        m_bluetoothManager->TiltTurntable(m_connectedDeviceId, m_tiltAngle);
    }
}

void BluetoothGui::StartCaptureSequence() {
    if (m_bluetoothManager && !m_connectedDeviceId.empty()) {
        // Return to zero first
        m_bluetoothManager->ReturnToZero(m_connectedDeviceId);
        
        // TODO: Integrate with actual camera capture system
        // For now, just demonstrate the rotation sequence
        std::thread([this]() {
            for (int i = 0; i < m_captureSteps; ++i) {
                // Rotate to next position
                m_bluetoothManager->RotateTurntable(m_connectedDeviceId, m_stepAngle);
                
                // Here you would trigger camera capture
                // CaptureAllCameras();
                
                // Wait for rotation to complete
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            }
        }).detach();
    }
}

void BluetoothGui::ClampValues() {
    // Clamp rotation values
    if (m_rotationAngle < -360.0f) m_rotationAngle = -360.0f;
    if (m_rotationAngle > 360.0f) m_rotationAngle = 360.0f;
    if (m_rotationSpeed < 35.64f) m_rotationSpeed = 35.64f;
    if (m_rotationSpeed > 131.0f) m_rotationSpeed = 131.0f;
    
    // Clamp tilt values
    if (m_tiltAngle < -30.0f) m_tiltAngle = -30.0f;
    if (m_tiltAngle > 30.0f) m_tiltAngle = 30.0f;
    if (m_tiltSpeed < 9.0f) m_tiltSpeed = 9.0f;
    if (m_tiltSpeed > 35.0f) m_tiltSpeed = 35.0f;
}

void BluetoothGui::UpdateStepAngle() {
    m_stepAngle = 360.0f / m_captureSteps;
}

} // namespace SaperaCapturePro