#include "PreferencesDialog.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace SaperaCapturePro {

PreferencesDialog::PreferencesDialog() = default;

void PreferencesDialog::Show(bool* p_open) {
  if (!p_open || !*p_open) return;

  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

  if (ImGui::Begin("Preferences", p_open, ImGuiWindowFlags_NoDocking)) {
    // Load current settings every time dialog is shown
    if (settings_manager_) {
      const auto& app_settings = settings_manager_->GetAppSettings();
      temp_ui_scale_ = app_settings.ui_scale;
      temp_dark_theme_ = app_settings.dark_theme;
      temp_vsync_ = app_settings.vsync;
      temp_auto_save_settings_ = app_settings.auto_save_settings;
      temp_window_width_ = app_settings.window_width;
      temp_window_height_ = app_settings.window_height;
      temp_window_x_ = app_settings.window_x;
      temp_window_y_ = app_settings.window_y;

      // Load camera settings
      const auto& camera_settings = settings_manager_->GetCameraSettings();
      temp_exposure_time_ = camera_settings.exposure_time;
      temp_gain_ = camera_settings.gain;
      temp_auto_exposure_ = camera_settings.auto_exposure;
      temp_auto_gain_ = camera_settings.auto_gain;
      temp_white_balance_red_ = camera_settings.white_balance_red;
      temp_white_balance_green_ = camera_settings.white_balance_green;
      temp_white_balance_blue_ = camera_settings.white_balance_blue;
      temp_auto_white_balance_ = camera_settings.auto_white_balance;
      temp_gamma_ = camera_settings.gamma;

      // Load log settings
      if (auto* log_panel = GetGlobalLogPanel()) {
        temp_log_auto_delete_ = log_panel->GetAutoDeleteEnabled();
        temp_log_max_messages_ = static_cast<int>(log_panel->GetMaxMessages());
      }

      // Load sound settings
      auto& sound_system = NotificationSounds::Instance();
      temp_completion_sound_enabled_ = sound_system.IsCompletionSoundEnabled();
      temp_completion_sound_type_ = static_cast<int>(sound_system.GetCompletionSoundType());
      strncpy_s(temp_custom_sound_path_, sound_system.GetCustomSoundPath().c_str(), sizeof(temp_custom_sound_path_) - 1);
    }

    // Left side - category list
    ImGui::BeginChild("left_pane", ImVec2(150, 0), true);

    static int selected_tab = 0;
    if (ImGui::Selectable("General", selected_tab == 0)) selected_tab = 0;
    if (ImGui::Selectable("Appearance", selected_tab == 1)) selected_tab = 1;
    if (ImGui::Selectable("Performance", selected_tab == 2)) selected_tab = 2;
    if (ImGui::Selectable("Camera", selected_tab == 3)) selected_tab = 3;
    if (ImGui::Selectable("About", selected_tab == 4)) selected_tab = 4;

    ImGui::EndChild();

    ImGui::SameLine();

    // Right side - settings for selected category
    ImGui::BeginChild("right_pane", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

    switch (selected_tab) {
      case 0: RenderGeneralTab(); break;
      case 1: RenderAppearanceTab(); break;
      case 2: RenderPerformanceTab(); break;
      case 3: RenderCameraTab(); break;
      case 4: RenderAboutTab(); break;
    }

    ImGui::EndChild();

    // Bottom buttons
    ImGui::Separator();

    if (ImGui::Button("Save")) {
      SaveSettings();
      ApplySettings();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset to Defaults")) {
      ResetSettings();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      *p_open = false;
    }
  }
  ImGui::End();
}

void PreferencesDialog::RenderGeneralTab() {
  ImGui::Text("General Settings");
  ImGui::Separator();
  ImGui::Spacing();

  // Auto-save setting
  if (ImGui::Checkbox("Auto-save settings", &temp_auto_save_settings_)) {
    if (settings_manager_) {
      settings_manager_->GetAppSettings().auto_save_settings = temp_auto_save_settings_;
      settings_manager_->Save();
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Log settings
  ImGui::Text("Log Settings");
  ImGui::Spacing();

  if (ImGui::Checkbox("Enable auto-delete old log messages", &temp_log_auto_delete_)) {
    if (auto* log_panel = GetGlobalLogPanel()) {
      log_panel->SetAutoDeleteEnabled(temp_log_auto_delete_);
    }
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("When disabled, log keeps all messages until manually cleared");
  }

  if (temp_log_auto_delete_) {
    ImGui::Text("Maximum log messages:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputInt("##MaxLogMessages", &temp_log_max_messages_)) {
      if (temp_log_max_messages_ < 0) temp_log_max_messages_ = 0;
      if (auto* log_panel = GetGlobalLogPanel()) {
        log_panel->SetMaxMessages(static_cast<size_t>(temp_log_max_messages_));
      }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(0 = unlimited)");
  } else {
    ImGui::TextDisabled("Log history: Unlimited (auto-delete disabled)");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Automatically save settings when changed");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("File Paths");
  ImGui::Spacing();

  if (settings_manager_) {
    static char output_folder[512];
    strcpy_s(output_folder, settings_manager_->GetAppSettings().last_output_folder.c_str());

    if (ImGui::InputText("Default Output Folder", output_folder, sizeof(output_folder))) {
      settings_manager_->GetAppSettings().last_output_folder = output_folder;
      settings_manager_->Save(); // Auto-save to local cache
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Notification sounds
  ImGui::Text("Notification Sounds");
  ImGui::Spacing();

  if (ImGui::Checkbox("Enable completion sound", &temp_completion_sound_enabled_)) {
    NotificationSounds::Instance().SetCompletionSoundEnabled(temp_completion_sound_enabled_);
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Play a sound when automated capture sequences complete");
  }

  if (temp_completion_sound_enabled_) {
    const char* sound_types[] = {
      "Clean Bell",
      "Trap Beat",
      "Microwave Ding (3x)",
      "Car Siren"
    };

    ImGui::Text("Sound type:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::Combo("##SoundType", &temp_completion_sound_type_, sound_types, IM_ARRAYSIZE(sound_types))) {
      NotificationSounds::Instance().SetCompletionSoundType(static_cast<NotificationSounds::SoundType>(temp_completion_sound_type_));
    }

    if (temp_completion_sound_type_ == 5) { // Custom sound file
      ImGui::Text("Custom sound file (leave empty for custom beats):");
      ImGui::SetNextItemWidth(300);
      if (ImGui::InputText("##CustomSoundPath", temp_custom_sound_path_, sizeof(temp_custom_sound_path_))) {
        NotificationSounds::Instance().SetCustomSoundPath(temp_custom_sound_path_);
      }
      ImGui::SameLine();
      if (ImGui::Button("Browse...")) {
        ImGui::SameLine();
        ImGui::TextDisabled("(file dialog coming soon)");
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Test Sound")) {
      NotificationSounds::Instance().TestSound(static_cast<NotificationSounds::SoundType>(temp_completion_sound_type_), 0.8f);
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Window settings
  ImGui::Text("Window Position & Size");
  ImGui::Spacing();

  if (ImGui::InputInt("Window Width", &temp_window_width_)) {
    temp_window_width_ = std::max(800, temp_window_width_); // Minimum width
    if (settings_manager_) {
      settings_manager_->GetAppSettings().window_width = temp_window_width_;
      settings_manager_->Save();
    }
  }

  if (ImGui::InputInt("Window Height", &temp_window_height_)) {
    temp_window_height_ = std::max(600, temp_window_height_); // Minimum height
    if (settings_manager_) {
      settings_manager_->GetAppSettings().window_height = temp_window_height_;
      settings_manager_->Save();
    }
  }

  if (ImGui::InputInt("Window X Position", &temp_window_x_)) {
    if (settings_manager_) {
      settings_manager_->GetAppSettings().window_x = temp_window_x_;
      settings_manager_->Save();
    }
  }

  if (ImGui::InputInt("Window Y Position", &temp_window_y_)) {
    if (settings_manager_) {
      settings_manager_->GetAppSettings().window_y = temp_window_y_;
      settings_manager_->Save();
    }
  }

  ImGui::TextDisabled("Note: Window position changes take effect on next app restart");
}

void PreferencesDialog::RenderAppearanceTab() {
  ImGui::Text("Appearance Settings");
  ImGui::Separator();
  ImGui::Spacing();

  // UI Scale
  ImGui::Text("UI Scale:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150);
  if (ImGui::SliderFloat("##UIScale", &temp_ui_scale_, 0.5f, 2.0f, "%.2f")) {
    if (on_ui_scale_changed_) {
      on_ui_scale_changed_(temp_ui_scale_);
    }
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Scale factor for UI elements. Requires restart for full effect.");
  }

  ImGui::Spacing();

  // Dark Theme
  if (ImGui::Checkbox("Dark Theme", &temp_dark_theme_)) {
    // Apply immediately or on save
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Enable dark color scheme for the interface");
  }
}

void PreferencesDialog::RenderPerformanceTab() {
  ImGui::Text("Performance Settings");
  ImGui::Separator();
  ImGui::Spacing();

  // VSync
  if (ImGui::Checkbox("Enable VSync", &temp_vsync_)) {
    // Apply immediately or on save
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Vertical synchronization to prevent screen tearing");
  }
}

void PreferencesDialog::RenderCameraTab() {
  ImGui::Text("Camera Settings");
  ImGui::Separator();
  ImGui::Spacing();

  // Exposure Time
  ImGui::Text("Exposure Time (us):");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150);
  if (ImGui::InputInt("##Exposure", &temp_exposure_time_)) {
    if (temp_exposure_time_ < 1) temp_exposure_time_ = 1;
    if (on_exposure_changed_) {
      on_exposure_changed_(temp_exposure_time_);
    }
  }

  // Gain
  ImGui::Text("Gain:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150);
  if (ImGui::SliderFloat("##Gain", &temp_gain_, 0.0f, 10.0f, "%.2f")) {
    if (on_gain_changed_) {
      on_gain_changed_(temp_gain_);
    }
  }

  // Auto Exposure
  if (ImGui::Checkbox("Auto Exposure", &temp_auto_exposure_)) {
    // Apply to camera
  }

  // Auto Gain
  if (ImGui::Checkbox("Auto Gain", &temp_auto_gain_)) {
    // Apply to camera
  }

  // White Balance
  ImGui::Text("White Balance:");
  ImGui::Text("Red:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100);
  if (ImGui::SliderFloat("##WBRed", &temp_white_balance_red_, 0.1f, 5.0f, "%.2f")) {
    if (on_white_balance_changed_) {
      on_white_balance_changed_(temp_white_balance_red_, temp_white_balance_green_, temp_white_balance_blue_);
    }
  }

  ImGui::Text("Green:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100);
  if (ImGui::SliderFloat("##WBGreen", &temp_white_balance_green_, 0.1f, 5.0f, "%.2f")) {
    if (on_white_balance_changed_) {
      on_white_balance_changed_(temp_white_balance_red_, temp_white_balance_green_, temp_white_balance_blue_);
    }
  }

  ImGui::Text("Blue:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100);
  if (ImGui::SliderFloat("##WBBlue", &temp_white_balance_blue_, 0.1f, 5.0f, "%.2f")) {
    if (on_white_balance_changed_) {
      on_white_balance_changed_(temp_white_balance_red_, temp_white_balance_green_, temp_white_balance_blue_);
    }
  }

  // Auto White Balance
  if (ImGui::Checkbox("Auto White Balance", &temp_auto_white_balance_)) {
    // Apply to camera
  }

  // Gamma
  ImGui::Text("Gamma:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150);
  ImGui::SliderFloat("##Gamma", &temp_gamma_, 0.1f, 5.0f, "%.2f");
}

void PreferencesDialog::RenderAboutTab() {
  ImGui::Text("About Camera Matrix Capture");
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("Version: 1.0.0");
  ImGui::Text("Built with:");
  ImGui::BulletText("ImGui");
  ImGui::BulletText("OpenGL");
  ImGui::BulletText("Sapera SDK");
  ImGui::BulletText("Bluetooth LE");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (ImGui::Button("Open GitHub Repository")) {
    // Open URL
  }
}

void PreferencesDialog::ApplySettings() {
  if (!settings_manager_) return;

  // Apply app settings
  auto& app_settings = settings_manager_->GetAppSettings();
  app_settings.ui_scale = temp_ui_scale_;
  app_settings.dark_theme = temp_dark_theme_;
  app_settings.vsync = temp_vsync_;
  app_settings.auto_save_settings = temp_auto_save_settings_;
  app_settings.window_width = temp_window_width_;
  app_settings.window_height = temp_window_height_;
  app_settings.window_x = temp_window_x_;
  app_settings.window_y = temp_window_y_;

  // Apply camera settings
  auto& camera_settings = settings_manager_->GetCameraSettings();
  camera_settings.exposure_time = temp_exposure_time_;
  camera_settings.gain = temp_gain_;
  camera_settings.auto_exposure = temp_auto_exposure_;
  camera_settings.auto_gain = temp_auto_gain_;
  camera_settings.white_balance_red = temp_white_balance_red_;
  camera_settings.white_balance_green = temp_white_balance_green_;
  camera_settings.white_balance_blue = temp_white_balance_blue_;
  camera_settings.auto_white_balance = temp_auto_white_balance_;
  camera_settings.gamma = temp_gamma_;

  // Call callbacks
  if (on_camera_settings_changed_) {
    on_camera_settings_changed_();
  }
}

void PreferencesDialog::SaveSettings() {
  if (settings_manager_) {
    settings_manager_->Save();
  }
}

void PreferencesDialog::ResetSettings() {
  if (!settings_manager_) return;

  // Reset to defaults
  temp_ui_scale_ = 1.0f;
  temp_dark_theme_ = true;
  temp_vsync_ = true;
  temp_auto_save_settings_ = true;
  temp_window_width_ = 1200;
  temp_window_height_ = 800;
  temp_window_x_ = 100;
  temp_window_y_ = 100;

  temp_exposure_time_ = 40000;
  temp_gain_ = 1.0f;
  temp_auto_exposure_ = false;
  temp_auto_gain_ = false;
  temp_white_balance_red_ = 1.0f;
  temp_white_balance_green_ = 1.0f;
  temp_white_balance_blue_ = 1.0f;
  temp_auto_white_balance_ = false;
  temp_gamma_ = 1.0f;

  temp_log_auto_delete_ = false;
  temp_log_max_messages_ = 0;

  temp_completion_sound_enabled_ = true;
  temp_completion_sound_type_ = 0;
  strcpy_s(temp_custom_sound_path_, "");
}

} // namespace SaperaCapturePro