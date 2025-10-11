#include "SettingsView.hpp"
#include <imgui.h>

namespace SaperaCapturePro {

SettingsView::SettingsView(SettingsManager* settings_mgr)
    : settings_manager_(settings_mgr)
{
    preferences_dialog_ = std::make_unique<PreferencesDialog>();
    preferences_dialog_->SetSettings(settings_mgr);
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

} // namespace SaperaCapturePro
