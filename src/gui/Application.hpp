#pragma once

#include <memory>
#include <vector>
#include <string>
#include "GuiManager.hpp"
#include "LogPanel.hpp"
#include "core/NavigationRail.hpp"
#include "views/CaptureView.hpp"
#include "views/FilesView.hpp"
#include "views/HardwareView.hpp"
#include "views/SettingsView.hpp"
#include "../utils/SettingsManager.hpp"
#include "../utils/SessionManager.hpp"
#include "../bluetooth/BluetoothManager.hpp"
#include "../hardware/CameraManager.hpp"

namespace SaperaCapturePro {

class Application {
 public:
  Application();
  ~Application();

  bool Initialize();
  void Run();
  void Shutdown();

 private:
  // Core managers
  std::unique_ptr<GuiManager> gui_manager_;
  std::unique_ptr<SettingsManager> settings_manager_;
  std::unique_ptr<SessionManager> session_manager_;
  BluetoothManager* bluetooth_manager_ = nullptr;  // Singleton
  CameraManager* camera_manager_ = nullptr;        // Singleton

  // Navigation and Views
  std::unique_ptr<NavigationRail> navigation_rail_;
  std::unique_ptr<CaptureView> capture_view_;
  std::unique_ptr<FilesView> files_view_;
  std::unique_ptr<HardwareView> hardware_view_;
  std::unique_ptr<SettingsView> settings_view_;

  // Bottom panel
  std::unique_ptr<LogPanel> log_panel_;

  // Dialog states
  bool show_about_dialog_ = false;
  bool show_documentation_dialog_ = false;

  // Application state
  bool is_running_ = true;
  NavigationItem current_view_ = NavigationItem::Capture;

  // Image preview state
  std::string selected_image_path_;
  unsigned int preview_texture_id_ = 0;
  int preview_width_ = 0;
  int preview_height_ = 0;

  void InitializeSettings();
  void InitializeGUI();
  void InitializeBluetooth();

  void RenderMainContent();
  void RenderAboutDialog();
  void RenderDocumentationDialog();

  void OnUIScaleChanged(float scale);
  void SaveSettings();
  void LoadSettings();

  // Image preview helpers
  bool LoadImagePreview(const std::string& image_path);
  void ClearImagePreview();
};

}  // namespace SaperaCapturePro