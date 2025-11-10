#pragma once

#include <memory>
#include "../PreferencesDialog.hpp"
#include "../../utils/SettingsManager.hpp"
#include "../../hardware/CameraManager.hpp"

namespace SaperaCapturePro {

class SettingsView {
public:
    SettingsView(SettingsManager* settings_mgr, CameraManager* camera_mgr);
    ~SettingsView();

    void Render();

private:
    SettingsManager* settings_manager_;
    CameraManager* camera_manager_;
    std::unique_ptr<PreferencesDialog> preferences_dialog_;

    void ApplyCameraSettingsToHardware();
};

} // namespace SaperaCapturePro
