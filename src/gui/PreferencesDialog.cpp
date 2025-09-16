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
      // Advanced color
      temp_color_method_ = camera_settings.color_method;
      temp_bayer_align_ = camera_settings.bayer_align;
      temp_use_hardware_conversion_ = camera_settings.use_hardware_conversion;
      // Output format mapping
      if (camera_settings.color_output_format == std::string("RGB888")) temp_output_format_index_ = 0;
      else if (camera_settings.color_output_format == std::string("RGB8888")) temp_output_format_index_ = 1;
      else if (camera_settings.color_output_format == std::string("RGB101010")) temp_output_format_index_ = 2;
      else temp_output_format_index_ = 0;
      // WB offsets
      temp_wb_offset_r_ = camera_settings.white_balance_offset_red;
      temp_wb_offset_g_ = camera_settings.white_balance_offset_green;
      temp_wb_offset_b_ = camera_settings.white_balance_offset_blue;
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

void PreferencesDialog::RenderCameraTab() {
  ImGui::Text("Camera Settings");
  ImGui::Separator();
  ImGui::Spacing();
  
  ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "These settings apply to all cameras in the system.");
  ImGui::Spacing();
  
  // === EXPOSURE CONTROL ===
  if (ImGui::CollapsingHeader("Exposure Control", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushItemWidth(200);
    
    // Exposure Time with input validation and range
    int temp_exposure = temp_exposure_time_;
    if (ImGui::InputInt("Exposure Time (microseconds)", &temp_exposure, 100, 10000)) {
      temp_exposure_time_ = std::max(100, std::min(100000, temp_exposure));
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().exposure_time = temp_exposure_time_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
        
        // Apply immediately to active cameras
        if (on_exposure_changed_) {
          on_exposure_changed_(temp_exposure_time_);
        }
        if (on_camera_settings_changed_) {
          on_camera_settings_changed_();
        }
      }
    }
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 100 - 100,000 μs");
    
    // Auto exposure checkbox
    if (ImGui::Checkbox("Auto Exposure", &temp_auto_exposure_)) {
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().auto_exposure = temp_auto_exposure_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
      }
    }
    
    // Quick exposure presets
    ImGui::Spacing();
    ImGui::Text("Quick Presets:");
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    
    if (ImGui::Button("Fast (1ms)")) {
      temp_exposure_time_ = 1000;
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().exposure_time = temp_exposure_time_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
        
        // Apply immediately to active cameras
        if (on_exposure_changed_) {
          on_exposure_changed_(temp_exposure_time_);
        }
        if (on_camera_settings_changed_) {
          on_camera_settings_changed_();
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Normal (10ms)")) {
      temp_exposure_time_ = 10000;
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().exposure_time = temp_exposure_time_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
        
        // Apply immediately to active cameras
        if (on_exposure_changed_) {
          on_exposure_changed_(temp_exposure_time_);
        }
        if (on_camera_settings_changed_) {
          on_camera_settings_changed_();
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Long (40ms)")) {
      temp_exposure_time_ = 40000;
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().exposure_time = temp_exposure_time_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
        
        // Apply immediately to active cameras
        if (on_exposure_changed_) {
          on_exposure_changed_(temp_exposure_time_);
        }
        if (on_camera_settings_changed_) {
          on_camera_settings_changed_();
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Very Long (100ms)")) {
      temp_exposure_time_ = 100000;
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().exposure_time = temp_exposure_time_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
        
        // Apply immediately to active cameras
        if (on_exposure_changed_) {
          on_exposure_changed_(temp_exposure_time_);
        }
        if (on_camera_settings_changed_) {
          on_camera_settings_changed_();
        }
      }
    }
    
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
  }
  
  // === GAIN CONTROL ===
  if (ImGui::CollapsingHeader("Gain Control", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushItemWidth(200);
    
    // Gain with input validation
    float temp_gain = temp_gain_;
    if (ImGui::InputFloat("Gain (dB)", &temp_gain, 0.1f, 1.0f, "%.1f")) {
      temp_gain_ = std::max(0.0f, std::min(30.0f, temp_gain));
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().gain = temp_gain_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
        
        // Apply immediately to active cameras
        if (on_gain_changed_) {
          on_gain_changed_(temp_gain_);
        }
        if (on_camera_settings_changed_) {
          on_camera_settings_changed_();
        }
      }
    }
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.0 - 30.0 dB");
    
    // Auto gain checkbox
    if (ImGui::Checkbox("Auto Gain", &temp_auto_gain_)) {
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().auto_gain = temp_auto_gain_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
      }
    }
    
    ImGui::PopItemWidth();
  }
  
  // === WHITE BALANCE ===
  if (ImGui::CollapsingHeader("White Balance", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushItemWidth(200);
    
    // Auto white balance checkbox
    if (ImGui::Checkbox("Auto White Balance", &temp_auto_white_balance_)) {
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().auto_white_balance = temp_auto_white_balance_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
      }
    }
    
    // Manual white balance controls (disabled if auto is on)
    ImGui::BeginDisabled(temp_auto_white_balance_);
    
    float temp_wb_red = temp_white_balance_red_;
    if (ImGui::InputFloat("Red Gain", &temp_wb_red, 0.01f, 0.1f, "%.2f")) {
      temp_white_balance_red_ = std::max(0.1f, std::min(4.0f, temp_wb_red));
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().white_balance_red = temp_white_balance_red_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
      }
    }
    
    float temp_wb_green = temp_white_balance_green_;
    if (ImGui::InputFloat("Green Gain", &temp_wb_green, 0.01f, 0.1f, "%.2f")) {
      temp_white_balance_green_ = std::max(0.1f, std::min(4.0f, temp_wb_green));
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().white_balance_green = temp_white_balance_green_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
      }
    }
    
    float temp_wb_blue = temp_white_balance_blue_;
    if (ImGui::InputFloat("Blue Gain", &temp_wb_blue, 0.01f, 0.1f, "%.2f")) {
      temp_white_balance_blue_ = std::max(0.1f, std::min(4.0f, temp_wb_blue));
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().white_balance_blue = temp_white_balance_blue_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
      }
    }
    
    ImGui::EndDisabled();
    
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.1 - 4.0 for all color channels");
    
    // White balance presets
    if (!temp_auto_white_balance_) {
      ImGui::Spacing();
      ImGui::Text("White Balance Presets:");
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
      
      if (ImGui::Button("Neutral")) {
        temp_white_balance_red_ = temp_white_balance_green_ = temp_white_balance_blue_ = 1.0f;
        if (settings_manager_) {
          auto& cam_settings = settings_manager_->GetCameraSettings();
          cam_settings.white_balance_red = cam_settings.white_balance_green = cam_settings.white_balance_blue = 1.0f;
          if (settings_manager_->GetAppSettings().auto_save_settings) {
            settings_manager_->Save();
          }
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Daylight")) {
        temp_white_balance_red_ = 1.0f; temp_white_balance_green_ = 1.0f; temp_white_balance_blue_ = 1.3f;
        if (settings_manager_) {
          auto& cam_settings = settings_manager_->GetCameraSettings();
          cam_settings.white_balance_red = 1.0f; cam_settings.white_balance_green = 1.0f; cam_settings.white_balance_blue = 1.3f;
          if (settings_manager_->GetAppSettings().auto_save_settings) {
            settings_manager_->Save();
          }
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Tungsten")) {
        temp_white_balance_red_ = 1.4f; temp_white_balance_green_ = 1.0f; temp_white_balance_blue_ = 0.8f;
        if (settings_manager_) {
          auto& cam_settings = settings_manager_->GetCameraSettings();
          cam_settings.white_balance_red = 1.4f; cam_settings.white_balance_green = 1.0f; cam_settings.white_balance_blue = 0.8f;
          if (settings_manager_->GetAppSettings().auto_save_settings) {
            settings_manager_->Save();
          }
        }
      }
      
      ImGui::PopStyleVar();
    }
    
    ImGui::PopItemWidth();
  }
  
  // === GAMMA CONTROL ===
  if (ImGui::CollapsingHeader("Image Enhancement")) {
    ImGui::PushItemWidth(200);
    
    // Gamma correction
    float temp_gamma = temp_gamma_;
    if (ImGui::SliderFloat("Gamma", &temp_gamma, 0.1f, 3.0f, "%.2f")) {
      temp_gamma_ = temp_gamma;
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().gamma = temp_gamma_;
        if (settings_manager_->GetAppSettings().auto_save_settings) {
          settings_manager_->Save();
        }
      }
    }
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.1 - 3.0 (1.0 = linear)");
    
    ImGui::Separator();
    ImGui::Text("Color Conversion");
    // Color method 1-7
    int method = temp_color_method_;
    if (ImGui::SliderInt("Method (1=Fast, 7=Best)", &method, 1, 7)) {
      temp_color_method_ = method;
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().color_method = temp_color_method_;
        if (settings_manager_->GetAppSettings().auto_save_settings) settings_manager_->Save();
      }
    }
    
    // Bayer alignment
    static const char* bayer_items[] = {"GBRG","BGGR","RGGB","GRBG","RGBG","BGRG"};
    int bayer = temp_bayer_align_;
    if (ImGui::Combo("Bayer Alignment", &bayer, bayer_items, IM_ARRAYSIZE(bayer_items))) {
      temp_bayer_align_ = bayer;
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().bayer_align = temp_bayer_align_;
        if (settings_manager_->GetAppSettings().auto_save_settings) settings_manager_->Save();
      }
    }
    
    // Hardware conversion toggle
    if (ImGui::Checkbox("Use Hardware Conversion", &temp_use_hardware_conversion_)) {
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().use_hardware_conversion = temp_use_hardware_conversion_;
        if (settings_manager_->GetAppSettings().auto_save_settings) settings_manager_->Save();
      }
    }

    // Output format selection (RGB888 / RGB8888 / RGB101010)
    static const char* kOutFmt[] = {"RGB888","RGB8888","RGB101010"};
    int ofmt = temp_output_format_index_;
    if (ImGui::Combo("Output Format", &ofmt, kOutFmt, IM_ARRAYSIZE(kOutFmt))) {
      temp_output_format_index_ = ofmt;
      if (settings_manager_) {
        auto& cs = settings_manager_->GetCameraSettings();
        cs.color_output_format = kOutFmt[temp_output_format_index_];
        if (settings_manager_->GetAppSettings().auto_save_settings) settings_manager_->Save();
      }
    }
    
    // White balance offsets (advanced)
    ImGui::Separator();
    ImGui::Text("WB Offset (advanced)");
    float off_r = temp_wb_offset_r_;
    if (ImGui::InputFloat("Offset R", &off_r, 0.1f, 1.0f, "%.2f")) {
      temp_wb_offset_r_ = off_r;
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().white_balance_offset_red = temp_wb_offset_r_;
        if (settings_manager_->GetAppSettings().auto_save_settings) settings_manager_->Save();
      }
    }
    float off_g = temp_wb_offset_g_;
    if (ImGui::InputFloat("Offset G", &off_g, 0.1f, 1.0f, "%.2f")) {
      temp_wb_offset_g_ = off_g;
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().white_balance_offset_green = temp_wb_offset_g_;
        if (settings_manager_->GetAppSettings().auto_save_settings) settings_manager_->Save();
      }
    }
    float off_b = temp_wb_offset_b_;
    if (ImGui::InputFloat("Offset B", &off_b, 0.1f, 1.0f, "%.2f")) {
      temp_wb_offset_b_ = off_b;
      if (settings_manager_) {
        settings_manager_->GetCameraSettings().white_balance_offset_blue = temp_wb_offset_b_;
        if (settings_manager_->GetAppSettings().auto_save_settings) settings_manager_->Save();
      }
    }
    
    ImGui::PopItemWidth();
  }
  
  // === INFORMATION ===
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  
  ImGui::Text("Current Settings Summary:");
  ImGui::BulletText("Exposure: %d μs (%s)", temp_exposure_time_, temp_auto_exposure_ ? "Auto" : "Manual");
  ImGui::BulletText("Gain: %.1f dB (%s)", temp_gain_, temp_auto_gain_ ? "Auto" : "Manual");
  ImGui::BulletText("White Balance: %s", temp_auto_white_balance_ ? "Auto" : "Manual");
  if (!temp_auto_white_balance_) {
    ImGui::BulletText("  R=%.2f, G=%.2f, B=%.2f", temp_white_balance_red_, temp_white_balance_green_, temp_white_balance_blue_);
  }
  ImGui::BulletText("Gamma: %.2f", temp_gamma_);
  ImGui::BulletText("Color Method: %d", temp_color_method_);
  {
    static const char* kBayerLabels[6] = {"GBRG","BGGR","RGGB","GRBG","RGBG","BGRG"};
    const char* bayerLabel = (temp_bayer_align_ >= 0 && temp_bayer_align_ <= 5) ? kBayerLabels[temp_bayer_align_] : "RGGB";
    ImGui::BulletText("Bayer: %s", bayerLabel);
  }
  ImGui::BulletText("Hardware Conversion: %s", temp_use_hardware_conversion_ ? "Yes" : "No");
  {
    static const char* kOutFmtSummary[] = {"RGB888","RGB8888","RGB101010"};
    const char* fmt = (temp_output_format_index_>=0 && temp_output_format_index_<3) ? kOutFmtSummary[temp_output_format_index_] : kOutFmtSummary[0];
    ImGui::BulletText("Output Format: %s", fmt);
  }
  
  ImGui::Spacing();
  ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.0f, 1.0f), "Note: Changes take effect immediately and are saved automatically.");
  ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Restart the application to ensure all cameras use new settings.");
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
  camera_settings.color_method = temp_color_method_;
  camera_settings.bayer_align = temp_bayer_align_;
  camera_settings.use_hardware_conversion = temp_use_hardware_conversion_;
  {
    static const char* kOutFmtApply[] = {"RGB888","RGB8888","RGB101010"};
    const char* fmt = (temp_output_format_index_>=0 && temp_output_format_index_<3) ? kOutFmtApply[temp_output_format_index_] : kOutFmtApply[0];
    camera_settings.color_output_format = std::string(fmt);
  }
  camera_settings.white_balance_offset_red = temp_wb_offset_r_;
  camera_settings.white_balance_offset_green = temp_wb_offset_g_;
  camera_settings.white_balance_offset_blue = temp_wb_offset_b_;
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
  
  // Reset camera parameter values to defaults
  temp_exposure_time_ = 40000;
  temp_gain_ = 1.0f;
  temp_auto_exposure_ = false;
  temp_auto_gain_ = false;
  temp_white_balance_red_ = 1.0f;
  temp_white_balance_green_ = 1.0f;
  temp_white_balance_blue_ = 1.0f;
  temp_auto_white_balance_ = false;
  temp_gamma_ = 1.0f;
  
  // Apply UI scale immediately when resetting
  if (ui_scale_callback_) {
    ui_scale_callback_(1.0f);
  }
}

}  // namespace SaperaCapturePro