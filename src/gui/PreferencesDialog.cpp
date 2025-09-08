#include "PreferencesDialog.hpp"
#include <imgui.h>
#include <algorithm>

namespace SaperaCapturePro {

PreferencesDialog::PreferencesDialog() = default;

void PreferencesDialog::Show(bool* p_open) {
  if (!p_open || !*p_open) return;
  
  ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
  
  if (ImGui::Begin("Preferences", p_open, ImGuiWindowFlags_NoDocking)) {
    // Load current settings on first open
    static bool first_open = true;
    if (first_open && settings_manager_) {
      temp_ui_scale_ = settings_manager_->GetAppSettings().ui_scale;
      temp_dark_theme_ = settings_manager_->GetAppSettings().dark_theme;
      temp_auto_save_ = settings_manager_->GetAppSettings().auto_save_settings;
      first_open = false;
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
    
    if (ImGui::Button("Apply")) {
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
  
  ImGui::Checkbox("Auto-save settings on exit", &temp_auto_save_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Automatically save all settings when closing the application");
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
    }
  }
}

void PreferencesDialog::RenderAppearanceTab() {
  ImGui::Text("Appearance Settings");
  ImGui::Separator();
  ImGui::Spacing();
  
  // UI Scale slider
  ImGui::Text("UI Scale");
  ImGui::SliderFloat("##UIScale", &temp_ui_scale_, 0.5f, 2.0f, "%.2f");
  
  ImGui::SameLine();
  if (ImGui::Button("Reset##UIScale")) {
    temp_ui_scale_ = 1.0f;
  }
  
  // Preview text at different scales
  ImGui::Spacing();
  ImGui::Text("Preview:");
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
  
  float preview_scales[] = {0.75f, 1.0f, 1.25f, 1.5f};
  const char* preview_labels[] = {"75%", "100%", "125%", "150%"};
  
  for (int i = 0; i < 4; i++) {
    if (ImGui::RadioButton(preview_labels[i], temp_ui_scale_ == preview_scales[i])) {
      temp_ui_scale_ = preview_scales[i];
    }
    if (i < 3) ImGui::SameLine();
  }
  
  ImGui::PopStyleVar();
  
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  
  // Theme selection
  ImGui::Text("Theme");
  if (ImGui::RadioButton("Dark Theme", temp_dark_theme_)) {
    temp_dark_theme_ = true;
  }
  if (ImGui::RadioButton("Light Theme", !temp_dark_theme_)) {
    temp_dark_theme_ = false;
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
  
  ImGui::Checkbox("VSync", &temp_vsync_);
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
  
  // Apply UI scale
  if (temp_ui_scale_ != settings_manager_->GetAppSettings().ui_scale) {
    settings_manager_->GetAppSettings().ui_scale = temp_ui_scale_;
    if (on_ui_scale_changed_) {
      on_ui_scale_changed_(temp_ui_scale_);
    }
  }
  
  // Apply other settings
  settings_manager_->GetAppSettings().dark_theme = temp_dark_theme_;
  settings_manager_->GetAppSettings().auto_save_settings = temp_auto_save_;
  
  // Save settings
  settings_manager_->Save();
}

void PreferencesDialog::ResetSettings() {
  temp_ui_scale_ = 1.0f;
  temp_dark_theme_ = true;
  temp_auto_save_ = true;
  temp_vsync_ = true;
  
  if (on_ui_scale_changed_) {
    on_ui_scale_changed_(1.0f);
  }
}

}  // namespace SaperaCapturePro