#include "SettingsView.hpp"
#include <imgui.h>
#include <iostream>

namespace SaperaCapturePro {

SettingsView::SettingsView(SettingsManager* settings_mgr, CameraManager* camera_mgr)
    : settings_manager_(settings_mgr), camera_manager_(camera_mgr)
{
    preferences_dialog_ = std::make_unique<PreferencesDialog>();
    preferences_dialog_->SetSettings(settings_mgr);

    // Register callback to apply camera settings when user clicks Save
    preferences_dialog_->SetOnCameraSettingsChanged([this]() {
        ApplyCameraSettingsToHardware();
    });
}

SettingsView::~SettingsView() = default;

void SettingsView::Render() {
    if (!settings_manager_) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "❌ Settings system not initialized");
        return;
    }

    // Render preferences inline (without window wrapper)
    preferences_dialog_->RenderContent();
}

void SettingsView::ApplyCameraSettingsToHardware() {
    if (!camera_manager_ || !settings_manager_) {
        std::cout << "[SETTINGS] Cannot apply settings: manager(s) not initialized" << std::endl;
        return;
    }

    // Check if any cameras are connected
    if (camera_manager_->GetConnectedCount() == 0) {
        std::cout << "[SETTINGS] No cameras connected - settings will be applied on next capture" << std::endl;
        return;
    }

    const auto& camera_settings = settings_manager_->GetCameraSettings();

    std::cout << "[SETTINGS] Applying camera settings to hardware:" << std::endl;
    std::cout << "  Exposure: " << camera_settings.exposure_time << "μs" << std::endl;
    std::cout << "  Gain: " << camera_settings.gain << std::endl;
    std::cout << "  White Balance (R/G/B): " << camera_settings.white_balance_red
              << "/" << camera_settings.white_balance_green
              << "/" << camera_settings.white_balance_blue << std::endl;
    std::cout << "  Gamma: " << camera_settings.gamma << std::endl;

    // Apply critical parameters (exposure and gain)
    std::cout << "[SETTINGS] Applying critical parameters..." << std::endl;
    camera_manager_->ApplyParameterToAllCameras("ExposureTime", std::to_string(camera_settings.exposure_time));
    camera_manager_->ApplyParameterToAllCameras("Gain", std::to_string(camera_settings.gain));

    // Try to apply white balance (may not be supported by all cameras)
    std::cout << "[SETTINGS] Applying white balance (if supported)..." << std::endl;
    camera_manager_->ApplyParameterToAllCameras("BalanceRatioRed", std::to_string(camera_settings.white_balance_red));
    camera_manager_->ApplyParameterToAllCameras("BalanceRatioGreen", std::to_string(camera_settings.white_balance_green));
    camera_manager_->ApplyParameterToAllCameras("BalanceRatioBlue", std::to_string(camera_settings.white_balance_blue));

    // Try to apply gamma (DISABLED - not supported by Nano-C4020 cameras)
    // Note: Gamma causes "Argument invalid value (2)" error on these cameras
    // std::cout << "[SETTINGS] Applying gamma (if supported)..." << std::endl;
    // camera_manager_->ApplyParameterToAllCameras("Gamma", std::to_string(camera_settings.gamma));

    std::cout << "[SETTINGS] ✅ Camera settings applied (Gamma skipped - not supported by hardware)" << std::endl;
}

} // namespace SaperaCapturePro
