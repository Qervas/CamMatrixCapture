#include "Application.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <filesystem>

namespace SaperaCapturePro {

Application::Application() = default;

Application::~Application() {
  Shutdown();
}

bool Application::Initialize() {
  try {
    InitializeSettings();
    InitializeGUI();
    InitializeBluetooth();
    
    AddGlobalLog("Application initialized successfully", LogLevel::kSuccess);
    return true;
  } catch (const std::exception& e) {
    std::cerr << "Failed to initialize application: " << e.what() << std::endl;
    return false;
  }
}

void Application::InitializeSettings() {
  // Create config directory if it doesn't exist
  std::filesystem::create_directories("config");
  
  // Initialize settings manager
  settings_manager_ = std::make_unique<SettingsManager>("config/settings.json");
  settings_manager_->Load();
  
  // Initialize session manager
  session_manager_ = std::make_unique<SessionManager>("neural_dataset");
}

void Application::InitializeGUI() {
  // Initialize GUI manager
  gui_manager_ = std::make_unique<GuiManager>();
  
  const auto& app_settings = settings_manager_->GetAppSettings();
  if (!gui_manager_->Initialize("Camera Matrix Capture", 
                                app_settings.window_width, 
                                app_settings.window_height)) {
    throw std::runtime_error("Failed to initialize GUI manager");
  }
  
  // Apply saved UI scale
  gui_manager_->SetUIScale(app_settings.ui_scale);
  
  // Initialize GUI components
  preferences_dialog_ = std::make_unique<PreferencesDialog>();
  preferences_dialog_->SetSettings(settings_manager_.get());
  preferences_dialog_->SetOnUIScaleChanged([this](float scale) {
    OnUIScaleChanged(scale);
  });
  
  log_panel_ = std::make_unique<LogPanel>();
  SetGlobalLogPanel(log_panel_.get());
}

void Application::InitializeBluetooth() {
  // Get Bluetooth manager singleton
  bluetooth_manager_ = &BluetoothManager::GetInstance();
  
  // Initialize Bluetooth GUI
  bluetooth_gui_ = std::make_unique<BluetoothGui>();
}

void Application::Run() {
  while (!gui_manager_->ShouldClose() && is_running_) {
    gui_manager_->BeginFrame();
    
    // Render dockspace and main menu
    RenderDockSpace();
    RenderMainMenuBar();
    
    // Render panels
    if (show_camera_panel_) RenderCameraPanel();
    if (show_capture_panel_) RenderCapturePanel();
    if (show_log_panel_) log_panel_->Render(&show_log_panel_);
    if (show_bluetooth_panel_) bluetooth_gui_->Render(&show_bluetooth_panel_);
    if (show_session_manager_) RenderSessionManagerPanel();
    if (show_color_panel_) RenderColorPanel();
    if (show_network_panel_) RenderNetworkPanel();
    
    // Render dialogs
    preferences_dialog_->Show(&show_preferences_);
    
    gui_manager_->EndFrame();
  }
}

void Application::Shutdown() {
  // Save settings if auto-save is enabled
  if (settings_manager_ && settings_manager_->GetAppSettings().auto_save_settings) {
    SaveSettings();
  }
  
  // Cleanup
  bluetooth_gui_.reset();
  // bluetooth_manager_ is a singleton, no need to delete
  log_panel_.reset();
  preferences_dialog_.reset();
  gui_manager_.reset();
  session_manager_.reset();
  settings_manager_.reset();
}

void Application::RenderMainMenuBar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("New Session", "Ctrl+N")) {
        session_manager_->StartNewSession("NewObject");
      }
      if (ImGui::MenuItem("Open Session...", "Ctrl+O")) {
        // TODO: Implement open session dialog
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Preferences...", "Ctrl+,")) {
        show_preferences_ = true;
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "Alt+F4")) {
        is_running_ = false;
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Camera Panel", nullptr, &show_camera_panel_);
      ImGui::MenuItem("Capture Panel", nullptr, &show_capture_panel_);
      ImGui::MenuItem("Log", nullptr, &show_log_panel_);
      ImGui::MenuItem("Bluetooth", nullptr, &show_bluetooth_panel_);
      ImGui::MenuItem("Session Manager", nullptr, &show_session_manager_);
      ImGui::MenuItem("Color Settings", nullptr, &show_color_panel_);
      ImGui::MenuItem("Network Settings", nullptr, &show_network_panel_);
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Camera")) {
      if (ImGui::MenuItem("Discover Cameras", "F5")) {
        // TODO: Trigger camera discovery
      }
      if (ImGui::MenuItem("Connect All", "F6")) {
        // TODO: Connect all cameras
      }
      if (ImGui::MenuItem("Disconnect All", "F7")) {
        // TODO: Disconnect all cameras
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Capture All", "Space")) {
        // TODO: Capture from all cameras
      }
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About...")) {
        // TODO: Show about dialog
      }
      if (ImGui::MenuItem("Documentation")) {
        // TODO: Open documentation
      }
      ImGui::EndMenu();
    }
    
    ImGui::EndMainMenuBar();
  }
}

void Application::RenderDockSpace() {
  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
  
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);
  
  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
  window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  
  ImGui::Begin("DockSpace", nullptr, window_flags);
  ImGui::PopStyleVar(3);
  
  ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
  
  // Set up default layout on first run
  static bool first_time = true;
  if (first_time) {
    first_time = false;
    
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
    
    auto dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
    auto dock_id_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);
    auto dock_id_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
    
    ImGui::DockBuilderDockWindow("Camera Control", dock_id_left);
    ImGui::DockBuilderDockWindow("Capture", dock_id_right);
    ImGui::DockBuilderDockWindow("Log", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Bluetooth Control", dock_id_right);
    ImGui::DockBuilderDockWindow("Session Manager", dockspace_id);
    ImGui::DockBuilderDockWindow("Color Settings", dock_id_right);
    ImGui::DockBuilderDockWindow("Network Settings", dock_id_right);
    
    ImGui::DockBuilderFinish(dockspace_id);
  }
  
  ImGui::End();
}

void Application::RenderCameraPanel() {
  if (ImGui::Begin("Camera Control", &show_camera_panel_)) {
    // TODO: Implement camera panel content
    ImGui::Text("Camera controls will be here");
  }
  ImGui::End();
}

void Application::RenderCapturePanel() {
  if (ImGui::Begin("Capture", &show_capture_panel_)) {
    // TODO: Implement capture panel content
    ImGui::Text("Capture controls will be here");
  }
  ImGui::End();
}

void Application::RenderSessionManagerPanel() {
  if (ImGui::Begin("Session Manager", &show_session_manager_)) {
    // TODO: Implement session manager content
    ImGui::Text("Session management will be here");
  }
  ImGui::End();
}

void Application::RenderColorPanel() {
  if (ImGui::Begin("Color Settings", &show_color_panel_)) {
    // TODO: Implement color settings content
    ImGui::Text("Color settings will be here");
  }
  ImGui::End();
}

void Application::RenderNetworkPanel() {
  if (ImGui::Begin("Network Settings", &show_network_panel_)) {
    // TODO: Implement network settings content
    ImGui::Text("Network settings will be here");
  }
  ImGui::End();
}

void Application::OnUIScaleChanged(float scale) {
  gui_manager_->SetUIScale(scale);
  AddGlobalLog("UI scale changed to " + std::to_string(scale), LogLevel::kInfo);
}

void Application::SaveSettings() {
  if (settings_manager_) {
    // Save window size
    int width, height;
    glfwGetWindowSize(gui_manager_->GetWindow(), &width, &height);
    settings_manager_->GetAppSettings().window_width = width;
    settings_manager_->GetAppSettings().window_height = height;
    
    settings_manager_->Save();
  }
}

void Application::LoadSettings() {
  if (settings_manager_) {
    settings_manager_->Load();
  }
}

}  // namespace SaperaCapturePro