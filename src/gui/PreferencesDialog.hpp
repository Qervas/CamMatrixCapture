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
    ui_scale_callback_ = callback;  // Also set immediate callback
  }

 private:
  SettingsManager* settings_manager_ = nullptr;
  
  // Temporary values for editing
  float temp_ui_scale_ = 1.0f;
  bool temp_dark_theme_ = true;
  bool temp_vsync_ = true;
  bool temp_auto_save_settings_ = true;
  int temp_window_width_ = 1200;
  int temp_window_height_ = 800;
  int temp_window_x_ = 100;
  int temp_window_y_ = 100;
  
  // Callbacks
  std::function<void(float)> on_ui_scale_changed_;
  std::function<void(float)> ui_scale_callback_;  // For immediate updates
  
  void RenderGeneralTab();
  void RenderAppearanceTab();
  void RenderPerformanceTab();
  void RenderAboutTab();
  
  void ApplySettings();
  void SaveSettings();
  void ResetSettings();
};

}  // namespace SaperaCapturePro