#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// Include our capture system
#include "capture/NeuralCaptureSystem.hpp"

// Forward declarations
class CameraControlPanel;
class ParameterPanel;
class StatusPanel;
class CapturePanel;

struct CameraInfo {
    std::string serialNumber;
    std::string userDefinedName;
    std::string serverName;
    bool isConnected = false;
    bool isCapturing = false;
    int cameraIndex = 0;
};

struct CaptureSession {
    std::string sessionName;
    std::string timestamp;
    int captureCount = 0;
    std::string format = "TIFF";
    std::string outputPath;
    bool isActive = false;
};

class NeuralCaptureGUI {
public:
    NeuralCaptureGUI();
    ~NeuralCaptureGUI();

    bool Initialize();
    void Run();
    void Shutdown();

    // Callbacks for camera operations
    std::function<void()> onDiscoverCameras;
    std::function<void()> onConnectAllCameras;
    std::function<void()> onDisconnectAllCameras;
    std::function<void()> onStartCapture;
    std::function<void()> onStopCapture;
    std::function<void(const std::string&)> onSetCaptureFormat;
    std::function<void(const std::string&, const std::string&)> onSetParameter;
    std::function<void()> onResetCapture;
    std::function<void()> onExitApplication;

    // Data update methods
    void UpdateCameraList(const std::vector<CameraInfo>& cameras);
    void UpdateCaptureSession(const CaptureSession& session);
    void UpdateSystemStatus(const std::string& status);
    void AddLogMessage(const std::string& message, const std::string& level = "INFO");

private:
    GLFWwindow* window_ = nullptr;
    bool running_ = true;
    
    // UI state
    bool show_camera_panel_ = true;
    bool show_parameter_panel_ = true;
    bool show_capture_panel_ = true;
    bool show_log_panel_ = true;
    
    // REAL capture system integration
    std::unique_ptr<NeuralRenderingCaptureSystem> capture_system_;
    
    // Log messages
    std::vector<std::string> log_messages_;
    
    // Folder management
    std::string current_image_folder_;
    char image_folder_buffer_[512];

    // GUI Panels
    std::unique_ptr<CameraControlPanel> camera_panel_;
    std::unique_ptr<ParameterPanel> parameter_panel_;
    std::unique_ptr<StatusPanel> status_panel_;
    std::unique_ptr<CapturePanel> capture_panel_;

    // Application state
    std::vector<CameraInfo> cameras_;
    CaptureSession current_session_;
    std::string system_status_ = "Initializing...";
    
    // GUI state
    bool show_main_menu_ = true;
    bool show_toolbar_ = true;
    ImVec4 clear_color_ = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    void SetupImGuiStyle();
    void RenderMainMenuBar();
    void RenderToolbar();
    void RenderStatusBar();
    void RenderPanels();
    void HandleKeyboardShortcuts();
    
    static void GlfwErrorCallback(int error, const char* description);
    static void GlfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);

    // REAL system operations
    void DiscoverCameras();
    void ConnectAllCameras();
    void CaptureAllCameras();
    void ShowCameraStatus();
    
    // Utility methods
    std::string GetCurrentTimestamp();
    void OpenFolderInExplorer(const std::string& path);
}; 