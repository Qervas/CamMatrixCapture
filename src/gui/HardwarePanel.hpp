#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <imgui.h>
#include "../bluetooth/BluetoothManager.hpp"
#include "../hardware/CameraManager.hpp"
#include "../utils/SessionManager.hpp"
#include "../utils/SettingsManager.hpp"

namespace SaperaCapturePro {

class HardwarePanel {
public:
    HardwarePanel();
    ~HardwarePanel();

    void Initialize(BluetoothManager* bluetooth_manager, CameraManager* camera_manager, SessionManager* session_manager, SettingsManager* settings_manager);
    void Render(bool* p_open);
    void RenderContent();  // Render without window wrapper
    void Shutdown();

    // Status getters
    bool IsBluetoothConnected() const;
    bool AreCamerasConnected() const;
    int GetConnectedCameraCount() const;

    // Callback for logging
    void SetLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }
    
    // Quick Connect functionality
    void QuickConnectAll();
    void StoreLastConnectionInfo();

private:
    // System references
    BluetoothManager* bluetooth_manager_ = nullptr;
    CameraManager* camera_manager_ = nullptr;
    SessionManager* session_manager_ = nullptr;
    SettingsManager* settings_manager_ = nullptr;

    // UI state
    int active_tab_ = 0; // 0 = Cameras, 1 = Bluetooth
    bool show_advanced_bluetooth_ = false;
    
    // Bluetooth UI state
    bool is_connecting_bluetooth_ = false;
    bool is_scanning_bluetooth_ = false;
    std::string selected_bluetooth_device_id_;
    
    // Camera UI state
    std::string selected_camera_id_;
    
    // Rendering methods
    void RenderQuickConnectButton();
    void RenderCameraTab();
    void RenderBluetoothTab();
    void RenderCameraControls();
    void RenderCameraTable();
    void RenderBluetoothControls();
    void RenderBluetoothDeviceList();
    void RenderBluetoothAdvancedControls();
    
    // Helper methods
    void LogMessage(const std::string& message);
    std::string GetBluetoothStatusText() const;
    std::string GetCameraStatusText() const;
    
    // Callbacks
    std::function<void(const std::string&)> log_callback_;
};

} // namespace SaperaCapturePro