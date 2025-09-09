#pragma once

#include <memory>
#include <vector>
#include <string>
#include "GuiManager.hpp"
#include "PreferencesDialog.hpp"
#include "LogPanel.hpp"
#include "HardwarePanel.hpp"
#include "widgets/CaptureStudioPanel.hpp"
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
  
  // GUI components
  std::unique_ptr<PreferencesDialog> preferences_dialog_;
  std::unique_ptr<LogPanel> log_panel_;
  std::unique_ptr<HardwarePanel> hardware_panel_;
  std::unique_ptr<CaptureStudioPanel> capture_studio_panel_;
  
  // Window states
  bool show_preferences_ = false;
  bool show_hardware_panel_ = true;
  bool show_capture_studio_ = true;
  bool show_log_panel_ = true;
  bool show_session_manager_ = false;
  bool show_network_panel_ = false;
  
  // Application state
  bool is_running_ = true;
  
  // Image preview state
  std::string selected_image_path_;
  unsigned int preview_texture_id_ = 0;
  int preview_width_ = 0;
  int preview_height_ = 0;
  
  void InitializeSettings();
  void InitializeGUI();
  void InitializeBluetooth();
  
  void RenderDockSpace();
  void RenderSessionManagerPanel();
  void RenderNetworkPanel();
  
  void OnUIScaleChanged(float scale);
  void SaveSettings();
  void LoadSettings();
  
  // Image preview helpers
  bool LoadImagePreview(const std::string& image_path);
  void ClearImagePreview();
};

}  // namespace SaperaCapturePro