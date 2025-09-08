#pragma once

#include <functional>
#include "../utils/SettingsManager.hpp"

namespace SaperaCapturePro {

class PreferencesDialog {
 public:
  PreferencesDialog();
  ~PreferencesDialog() = default;

  void Show(bool* p_open);
  
  void SetSettings(SettingsManager* settings) { settings_manager_ = settings; }
  
  void SetOnUIScaleChanged(std::function<void(float)> callback) {
    on_ui_scale_changed_ = callback;
  }

 private:
  SettingsManager* settings_manager_ = nullptr;
  
  // Temporary values for editing
  float temp_ui_scale_ = 1.0f;
  bool temp_dark_theme_ = true;
  bool temp_auto_save_ = true;
  bool temp_vsync_ = true;
  
  // Callbacks
  std::function<void(float)> on_ui_scale_changed_;
  
  void RenderGeneralTab();
  void RenderAppearanceTab();
  void RenderPerformanceTab();
  void RenderAboutTab();
  
  void ApplySettings();
  void ResetSettings();
};

}  // namespace SaperaCapturePro