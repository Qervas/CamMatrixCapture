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
    }
    
    // Left side - category list
    ImGui::BeginChild("left_pane", ImVec2(150, 0), true);
    
    static int selected_tab = 0;
    if (ImGui::Selectable("General", selected_tab == 0)) selected_tab = 0;
    if (ImGui::Selectable("Appearance", selected_tab == 1)) selected_tab = 1;
    if (ImGui::Selectable("Performance", selected_tab == 2)) selected_tab = 2;
    if (ImGui::Selectable("About", selected_tab == 3)) selected_tab = 3;
    
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Right side - settings for selected category
    ImGui::BeginChild("right_pane", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
    
    switch (selected_tab) {
      case 0: RenderGeneralTab(); break;
      case 1: RenderAppearanceTab(); break;
      case 2: RenderPerformanceTab(); break;
      case 3: RenderAboutTab(); break;
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
  
  // UI Scale slider with extended range - takes effect immediately
  ImGui::Text("UI Scale");
  ImGui::SetNextItemWidth(300);
  if (ImGui::SliderFloat("##UIScale", &temp_ui_scale_, 0.5f, 8.0f, "%.2fx")) {
    // Apply immediately when dragging
    if (ui_scale_callback_) {
      ui_scale_callback_(temp_ui_scale_);
    }
  }
  
  ImGui::SameLine();
  if (ImGui::Button("Reset##UIScale")) {
    temp_ui_scale_ = 1.0f;
    // Apply immediately when reset
    if (ui_scale_callback_) {
      ui_scale_callback_(temp_ui_scale_);
    }
  }
  
  // Display current scale percentage
  ImGui::SameLine();
  ImGui::Text("(%d%%)", static_cast<int>(temp_ui_scale_ * 100));
  
  // Quick preset buttons - Extended range
  ImGui::Spacing();
  ImGui::Text("Quick Presets:");
  
  // First row of presets (smaller scales)
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
  
  float preset_scales_1[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f};
  const char* preset_labels_1[] = {"50%", "75%", "100%", "125%", "150%"};
  
  for (int i = 0; i < 5; i++) {
    if (ImGui::Button(preset_labels_1[i])) {
      temp_ui_scale_ = preset_scales_1[i];
      // Apply immediately when preset is clicked
      if (ui_scale_callback_) {
        ui_scale_callback_(temp_ui_scale_);
      }
    }
    if (i < 4) ImGui::SameLine();
  }
  
  // Second row of presets (larger scales)
  float preset_scales_2[] = {2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
  const char* preset_labels_2[] = {"200%", "300%", "400%", "600%", "800%"};
  
  for (int i = 0; i < 5; i++) {
    if (ImGui::Button(preset_labels_2[i])) {
      temp_ui_scale_ = preset_scales_2[i];
      // Apply immediately when preset is clicked
      if (ui_scale_callback_) {
        ui_scale_callback_(temp_ui_scale_);
      }
    }
    if (i < 4) ImGui::SameLine();
  }
  
  ImGui::PopStyleVar();
  
  // Warning for extreme scales
  if (temp_ui_scale_ < 0.75f) {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), 
                      "Warning: Very small UI scale may be hard to read");
  } else if (temp_ui_scale_ > 4.0f) {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), 
                      "Warning: Very large UI scale may cause layout issues");
  }
  
  // Live preview text
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::Text("Live Preview (this text scales with setting):");
  
  // Apply temporary scale for preview
  ImGui::PushFont(ImGui::GetFont());
  ImGui::SetWindowFontScale(temp_ui_scale_);
  ImGui::TextWrapped("This is preview text at %.1fx scale. The quick brown fox jumps over the lazy dog.", temp_ui_scale_);
  ImGui::SetWindowFontScale(1.0f);
  ImGui::PopFont();
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  
  // Theme selection
  ImGui::Text("Theme");
  if (ImGui::RadioButton("Dark Theme", temp_dark_theme_)) {
    temp_dark_theme_ = true;
    if (settings_manager_) {
      std::cout << "[PREFERENCES] Dark theme selected, saving..." << std::endl;
      settings_manager_->GetAppSettings().dark_theme = temp_dark_theme_;
      settings_manager_->Save(); // Auto-save to local cache
    } else {
      std::cout << "[PREFERENCES] ERROR: settings_manager_ is null!" << std::endl;
    }
  }
  if (ImGui::RadioButton("Light Theme", !temp_dark_theme_)) {
    temp_dark_theme_ = false;
    if (settings_manager_) {
      std::cout << "[PREFERENCES] Light theme selected, saving..." << std::endl;
      settings_manager_->GetAppSettings().dark_theme = temp_dark_theme_;
      settings_manager_->Save(); // Auto-save to local cache
    } else {
      std::cout << "[PREFERENCES] ERROR: settings_manager_ is null!" << std::endl;
    }
  }
  
  if (!temp_dark_theme_) {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Light theme is not yet implemented");
  }
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  
  // Font settings (placeholder)
  ImGui::Text("Font");
  ImGui::TextDisabled("Font customization coming soon...");
}

void PreferencesDialog::RenderPerformanceTab() {
  ImGui::Text("Performance Settings");
  ImGui::Separator();
  ImGui::Spacing();
  
  if (ImGui::Checkbox("VSync", &temp_vsync_)) {
    // Auto-save VSync setting to local cache
    if (settings_manager_) {
      std::cout << "[PREFERENCES] VSync changed to " << (temp_vsync_ ? "ON" : "OFF") << ", saving..." << std::endl;
      settings_manager_->GetAppSettings().vsync = temp_vsync_;
      settings_manager_->Save();
    } else {
      std::cout << "[PREFERENCES] ERROR: settings_manager_ is null for VSync!" << std::endl;
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Enable vertical synchronization to prevent screen tearing");
  }
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  
  // Memory usage info
  ImGui::Text("Memory Usage");
  ImGui::TextDisabled("Application memory: N/A");
  ImGui::TextDisabled("Image buffer memory: N/A");
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  
  // Cache settings
  ImGui::Text("Cache Settings");
  if (ImGui::Button("Clear Image Cache")) {
    // TODO: Implement cache clearing
  }
  
  ImGui::SameLine();
  ImGui::TextDisabled("0 MB cached");
}

void PreferencesDialog::RenderAboutTab() {
  ImGui::Text("About");
  ImGui::Separator();
  ImGui::Spacing();
  
  ImGui::Text("Camera Matrix Capture");
  ImGui::Text("Version 1.0.0");
  ImGui::Spacing();
  
  ImGui::TextWrapped("A professional camera capture application for multi-camera setups with Sapera SDK integration.");
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  
  ImGui::Text("System Information");
  ImGui::BulletText("ImGui Version: %s", IMGUI_VERSION);
  ImGui::BulletText("OpenGL Version: 3.3");
  ImGui::BulletText("Sapera SDK: Integrated");
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  
  ImGui::Text("Credits");
  ImGui::TextWrapped("Built with Dear ImGui, GLFW, and Teledyne Sapera SDK");
}

void PreferencesDialog::ApplySettings() {
  if (!settings_manager_) return;
  
  // Validate and clamp UI scale
  temp_ui_scale_ = std::clamp(temp_ui_scale_, 0.5f, 8.0f);
  
  // Apply all settings to settings manager
  auto& app_settings = settings_manager_->GetAppSettings();
  app_settings.ui_scale = temp_ui_scale_;
  app_settings.dark_theme = temp_dark_theme_;
  app_settings.vsync = temp_vsync_;
  app_settings.auto_save_settings = temp_auto_save_settings_;
  app_settings.window_width = temp_window_width_;
  app_settings.window_height = temp_window_height_;
  app_settings.window_x = temp_window_x_;
  app_settings.window_y = temp_window_y_;
}

void PreferencesDialog::SaveSettings() {
  if (!settings_manager_) return;
  
  // Save settings to file
  settings_manager_->Save();
}

void PreferencesDialog::ResetSettings() {
  // Reset all temporary values to defaults
  temp_ui_scale_ = 1.0f;
  temp_dark_theme_ = true;
  temp_vsync_ = true;
  temp_auto_save_settings_ = true;
  temp_window_width_ = 1200;
  temp_window_height_ = 800;
  temp_window_x_ = 100;
  temp_window_y_ = 100;
  
  // Apply UI scale immediately when resetting
  if (ui_scale_callback_) {
    ui_scale_callback_(1.0f);
  }
}

}  // namespace SaperaCapturePro