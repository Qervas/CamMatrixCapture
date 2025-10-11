#pragma once

#include <memory>
#include "../PreferencesDialog.hpp"
#include "../../utils/SettingsManager.hpp"

namespace SaperaCapturePro {

class SettingsView {
public:
    SettingsView(SettingsManager* settings_mgr);
    ~SettingsView();

    void Render();

private:
    SettingsManager* settings_manager_;
    std::unique_ptr<PreferencesDialog> preferences_dialog_;
};

} // namespace SaperaCapturePro
