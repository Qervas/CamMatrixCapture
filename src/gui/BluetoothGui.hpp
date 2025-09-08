#pragma once

#include <string>
#include <vector>
#include <functional>

// Forward declare ImGui types to avoid including imgui.h in header
struct ImVec2;
struct ImVec4;

namespace SaperaCapturePro {

class BluetoothManager;

class BluetoothGui {
public:
    BluetoothGui();
    ~BluetoothGui();
    
    // Main rendering function
    void Render(bool* show_panel);
    
    // Initialize with Bluetooth manager
    void Initialize(BluetoothManager* manager);
    void Shutdown();
    
    // UI state getters/setters
    bool IsConnecting() const { return m_isConnecting; }
    const std::string& GetConnectedDeviceId() const { return m_connectedDeviceId; }
    
    // Event callbacks
    void OnDeviceDiscovered(const std::string& deviceId, const std::string& deviceName);
    void OnConnectionStatusChanged(const std::string& deviceId, bool connected);
    
private:
    // Bluetooth manager reference
    BluetoothManager* m_bluetoothManager = nullptr;
    
    // UI State
    bool m_isConnecting = false;
    std::string m_selectedDeviceId = "";
    std::string m_connectedDeviceId = "";
    std::vector<std::pair<std::string, std::string>> m_discoveredDevices;
    
    // Control parameters
    float m_rotationAngle = 30.0f;
    float m_tiltAngle = 0.0f;
    float m_rotationSpeed = 70.0f;
    float m_tiltSpeed = 20.0f;
    bool m_autoRotateCapture = false;
    int m_captureSteps = 12;
    float m_stepAngle = 30.0f;
    
    // Settings
    char m_serviceUuid[128] = "0000ffe0-0000-1000-8000-00805f9b34fb";
    char m_charUuid[128] = "0000ffe1-0000-1000-8000-00805f9b34fb";
    
    // Tab rendering functions
    void RenderScannerTab();
    void RenderControlTab();
    void RenderCaptureSequenceTab();
    void RenderSettingsTab();
    
    // Helper functions
    void StartScanning();
    void StopScanning();
    void ConnectToSelectedDevice();
    void DisconnectCurrentDevice();
    void SendRotationCommand();
    void SendTiltCommand();
    void StartCaptureSequence();
    
    // UI helpers
    void ClampValues();
    void UpdateStepAngle();
};

} // namespace SaperaCapturePro