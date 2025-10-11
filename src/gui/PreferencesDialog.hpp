#pragma once

#include <functional>
#include "../utils/SettingsManager.hpp"
#include "../utils/NotificationSounds.hpp"
#include "LogPanel.hpp"

namespace SaperaCapturePro {

class PreferencesDialog {
 public:
  PreferencesDialog();
  ~PreferencesDialog() = default;

  void Show(bool* p_open);
  void RenderContent();  // Render without window wrapper

  void SetSettings(SettingsManager* settings) { settings_manager_ = settings; }

  void SetOnUIScaleChanged(std::function<void(float)> callback) {
    on_ui_scale_changed_ = callback;
    ui_scale_callback_ = callback;  // Also set immediate callback
  }

  // Camera setting callbacks
  void SetOnCameraSettingsChanged(std::function<void()> callback) {
    on_camera_settings_changed_ = callback;
  }

  void SetOnExposureChanged(std::function<void(int)> callback) {
    on_exposure_changed_ = callback;
  }

  void SetOnGainChanged(std::function<void(float)> callback) {
    on_gain_changed_ = callback;
  }

  void SetOnWhiteBalanceChanged(std::function<void(float, float, float)> callback) {
    on_white_balance_changed_ = callback;
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

  // Camera parameter temporary values
  int temp_exposure_time_ = 40000;
  float temp_gain_ = 1.0f;
  bool temp_auto_exposure_ = false;
  bool temp_auto_gain_ = false;
  float temp_white_balance_red_ = 1.0f;
  float temp_white_balance_green_ = 1.0f;
  float temp_white_balance_blue_ = 1.0f;
  bool temp_auto_white_balance_ = false;
  float temp_gamma_ = 1.0f;
  // Advanced color processing
  int temp_color_method_ = 1;
  int temp_bayer_align_ = 2;
  bool temp_use_hardware_conversion_ = false;
  int temp_output_format_index_ = 0; // 0=RGB888,1=RGB8888,2=RGB101010
  float temp_wb_offset_r_ = 0.0f;
  float temp_wb_offset_g_ = 0.0f;
  float temp_wb_offset_b_ = 0.0f;

  // Log settings temporary values
  bool temp_log_auto_delete_ = false;
  int temp_log_max_messages_ = 0;  // 0 = unlimited

  // Sound settings temporary values
  bool temp_completion_sound_enabled_ = true;
  int temp_completion_sound_type_ = 0; // 0=WindowsDing, 1=WindowsNotification, etc.
  char temp_custom_sound_path_[256] = "";

  // Callbacks
  std::function<void(float)> on_ui_scale_changed_;
  std::function<void(float)> ui_scale_callback_;  // For immediate updates

  // Camera setting callbacks
  std::function<void()> on_camera_settings_changed_;
  std::function<void(int)> on_exposure_changed_;
  std::function<void(float)> on_gain_changed_;
  std::function<void(float, float, float)> on_white_balance_changed_;

  void RenderGeneralTab();
  void RenderAppearanceTab();
  void RenderPerformanceTab();
  void RenderCameraTab();
  void RenderAboutTab();

  void ApplySettings();
  void SaveSettings();
  void ResetSettings();
};

}  // namespace SaperaCapturePro