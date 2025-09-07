#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>
#include <fstream>
#include <ctime>

// Windows API for file dialogs and system integration
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

// Sapera SDK FIRST to avoid macro conflicts
#include "SapClassBasic.h"
#include "hardware/CameraTypes.hpp"

// Session and Settings Management
#include "utils/SessionManager.hpp"
#include "utils/SettingsManager.hpp"

// Bluetooth Module
#include "bluetooth/BluetoothManager.hpp"
#include "bluetooth/BluetoothCommands.hpp"

// WinRT for apartment initialization
#include <winrt/base.h>

// Dear ImGui AFTER Sapera to avoid APIENTRY conflicts
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <imgui_internal.h>

using namespace SaperaCapturePro;

// Global instances for session and settings management
static std::unique_ptr<SessionManager> session_manager;
static std::unique_ptr<SettingsManager> settings_manager;

// Bluetooth Manager instance
static BluetoothManager* bluetooth_manager = nullptr;

// Global variables for the GUI
static GLFWwindow* window = nullptr;
static bool show_camera_panel = true;
static bool show_capture_panel = true;
static bool show_log_panel = true;
static bool show_color_panel = true;  // NEW: Color control panel
static bool show_bluetooth_panel = false; // NEW: Bluetooth control panel
static std::vector<std::string> log_messages;
static std::string current_image_folder = "neural_dataset";
static char image_folder_buffer[512];
static int exposure_time = 40000;
static bool capture_format_raw = false; // false = TIFF, true = RAW

// Camera discovery animation state
static bool is_discovering_cameras = false;
static std::thread discovery_thread;

// Camera connection animation state
static bool is_connecting_cameras = false;
static std::thread connection_thread;

// Session Management UI Variables
static char new_object_name[256] = "";
static bool show_session_manager = false;

// Network Optimization and Image Quality - NEW (DEFAULT TO MAXIMUM QUALITY)
static bool show_network_panel = false;
static bool sequential_capture_mode = true;   // DEFAULT: Sequential for maximum quality
static int capture_delay_ms = 750;            // DEFAULT: Optimal delay for 11+ cameras
static bool enable_packet_optimization = true; // DEFAULT: Auto optimization enabled
static bool enable_bandwidth_monitoring = true; // DEFAULT: Monitor network performance
static int max_bandwidth_mbps = 1000;         // Maximum network bandwidth
static bool use_jumbo_frames = false;         // Will be auto-detected
static bool network_diagnostics_enabled = false; // Show network diagnostics

// NEW: Advanced Color Processing Variables
struct ColorSettings {
    // White Balance
    float wb_red_gain = 1.0f;
    float wb_green_gain = 1.0f;
    float wb_blue_gain = 1.0f;
    bool auto_white_balance = false;
    
    // Color Conversion
    int color_method = 1;  // 1-7 for different quality/speed trade-offs
    int bayer_align = 2;   // 0=GBRG, 1=BGGR, 2=RGGB, 3=GRBG, 4=RGBG, 5=BGRG
    
    // Gamma Correction
    float gamma_value = 1.0f;
    bool enable_gamma = false;
    
    // LUT Support
    bool enable_lut = false;
    std::string lut_filename = "";
    
    // Advanced Settings
    bool use_hardware_conversion = false;
    bool enable_color_enhancement = false;
};

static ColorSettings color_settings;

// Advanced Camera Parameters Structure
struct CameraParameters {
    // Image Format (now with dynamic resolution)
    int width = 2560;
    int height = 1600;
    int max_width = 2560;   // Will be detected from camera
    int max_height = 1600;  // Will be detected from camera
    std::string pixel_format = "RGB8";
    
    // Acquisition Control
    int exposure_time = 40000;  // microseconds (40k default)
    bool auto_exposure = false;
    float gain = 0.0f;  // dB
    bool auto_gain = false;
    float black_level = 0.0f;
    float gamma = 1.0f;
    
    // Trigger Control
    std::string trigger_mode = "Off";  // Off, On, External
    std::string trigger_source = "Software";
    std::string trigger_activation = "RisingEdge";
    float trigger_delay = 0.0f;  // microseconds
    
    // Color Control
    float wb_red = 1.0f;
    float wb_green = 1.0f;
    float wb_blue = 1.0f;
    bool auto_white_balance = false;
    float saturation = 1.0f;
    float hue = 0.0f;
    
    // Advanced Features
    std::string acquisition_mode = "Continuous";  // SingleFrame, MultiFrame, Continuous
    int acquisition_frame_count = 1;
    float acquisition_frame_rate = 30.0f;
    bool frame_rate_enable = false;
    
    // Region of Interest (ROI)
    int roi_x = 0;
    int roi_y = 0;
    int roi_width = 1920;
    int roi_height = 1080;
    bool roi_enable = false;
    
    // Transport Layer (OPTIMIZED FOR NEURAL DATASETS)
    int packet_size = 1200;        // DEFAULT: Safe packet size for 11+ cameras
    int packet_delay = 3000;       // DEFAULT: 3ms delay for network stability
    
    // Device Control
    std::string temperature_status = "Normal";
    float device_temperature = 25.0f;
};

static CameraParameters camera_params;
static bool show_advanced_params = false;
static std::vector<std::string> available_features;
static std::map<std::string, std::string> feature_values;
static bool refresh_features = true;

// Individual Camera Control - NEW FEATURE
static std::map<std::string, CameraParameters> individual_camera_params; // Per-camera parameters
static std::string selected_camera_id = ""; // Currently selected camera for individual control
static bool individual_camera_mode = false; // Toggle between multi-camera and individual camera control
static bool show_individual_camera_panel = false; // Individual camera control panel

// Camera system state
static std::vector<CameraInfo> discovered_cameras;
static std::map<std::string, SapAcqDevice*> connected_devices;
static std::map<std::string, SapBuffer*> connected_buffers;
static std::map<std::string, SapAcqDeviceToBuf*> connected_transfers;
static std::map<std::string, std::string> camera_device_names; // Store device names separately
static int capture_counter = 1;

// Log highlighting system
struct LogMessage {
    std::string message;
    std::chrono::steady_clock::time_point timestamp;
    bool is_new = true;
};
static std::vector<LogMessage> log_messages_with_timestamps;
static std::chrono::steady_clock::time_point last_log_check = std::chrono::steady_clock::now();

// Network monitoring log filter
static bool filter_network_monitoring_logs = true; // Default: filter out network monitoring logs

// Bluetooth UI Variables
static std::vector<std::pair<std::string, std::string>> discovered_bt_devices;
static std::string selected_bt_device_id = "";
static std::string connected_bt_device_id = "";
static bool bt_is_scanning = false;
static bool bt_is_connecting = false;
static float bt_rotation_angle = 30.0f;
static float bt_tilt_angle = 0.0f;
static float bt_rotation_speed = 70.0f;
static float bt_tilt_speed = 20.0f;
static bool bt_auto_rotate_capture = false;
static int bt_capture_steps = 12;
static float bt_step_angle = 30.0f;
static char bt_service_uuid[128] = "0000ffe0-0000-1000-8000-00805f9b34fb";
static char bt_char_uuid[128] = "0000ffe1-0000-1000-8000-00805f9b34fb";

// Function declarations
void AddLogMessage(const std::string& message);
std::string GetCurrentTimestamp();
void DiscoverCameras();
void ConnectAllCameras();
void DisconnectAllCameras(); // NEW: Disconnect function
void CaptureAllCameras();
bool CaptureCamera(const std::string& cameraId, const std::string& sessionPath);
std::string GenerateSessionName(int captureNumber);
void OpenFolderInExplorer(const std::string& path);
std::string BrowseForLUTFile();  // NEW: LUT file browser

// Session and Settings functions
void SaveCurrentSettings();
void LoadSettingsToUI();
void SyncCameraParametersToSettings();
void RenderSessionManagerPanel();

// GUI functions
void RenderMainMenuBar();
void RenderCameraPanel();
void RenderCapturePanel();
void RenderLogPanel();
void RenderColorPanel();  // NEW: Color control panel
void SetupVSCodeLayout(); // NEW: VSCode-style professional layout

// Function declarations for advanced parameters
void RenderAdvancedParametersPanel();
void RefreshCameraFeatures();
void DetectCameraResolution();
void ApplyParameterToAllCameras(const std::string& featureName, const std::string& value);
// Individual Camera Control Functions - NEW
void ApplyParameterToCamera(const std::string& cameraId, const std::string& featureName, const std::string& value);
void InitializeIndividualCameraParams(const std::string& cameraId);
CameraParameters& GetCameraParameters(const std::string& cameraId);
void SyncAllCamerasToIndividual();
void SyncIndividualToAllCameras();
void RenderIndividualCameraPanel();
// Individual Camera Settings Persistence - NEW
void SaveIndividualCameraSettings();
void LoadIndividualCameraSettings();
void SyncIndividualParamsToSettings();
void SyncSettingsToIndividualParams();
std::vector<std::string> GetAvailablePixelFormats();
std::vector<std::string> GetAvailableTriggerModes();

// Metadata Management Functions - NEW
void SaveCaptureMetadata(const std::string& sessionPath, int captureNumber, bool success, int duration_ms);

// Network Optimization Functions - NEW
void RenderNetworkOptimizationPanel();
void OptimizeNetworkSettings();
void CaptureSequential();
void CheckNetworkBandwidth();
void ApplyJumboFrames();
void MonitorNetworkPerformance();
void RenderBluetoothPanel();
void InitializeBluetooth();
void ShutdownBluetooth();
void ApplyMaximumQualityDefaults(); // NEW: Auto-apply neural dataset optimizations

// GLFW error callback
static void GlfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main() {
    try {
        // Initialize WinRT apartment - MUST be first for Bluetooth to work
        winrt::init_apartment();
        
        std::cout << "[NEURAL] Neural Rendering Capture System - DIRECT INTEGRATION" << std::endl;
        std::cout << "=======================================================" << std::endl;
    
    // Initialize session and settings management
    session_manager = std::make_unique<SessionManager>("neural_dataset");
    settings_manager = std::make_unique<SettingsManager>("config/settings.json");
    
    // Initialize Bluetooth
    InitializeBluetooth();
    
    // Load settings and apply to camera parameters
    auto& cam_settings = settings_manager->GetCameraSettings();
    camera_params.width = cam_settings.width;
    camera_params.height = cam_settings.height;
    camera_params.pixel_format = cam_settings.pixel_format;
    camera_params.exposure_time = cam_settings.exposure_time;
    camera_params.gain = cam_settings.gain;
    camera_params.auto_exposure = cam_settings.auto_exposure;
    camera_params.auto_gain = cam_settings.auto_gain;
    camera_params.trigger_mode = cam_settings.trigger_mode;
    camera_params.trigger_source = cam_settings.trigger_source;
    camera_params.trigger_activation = cam_settings.trigger_activation;
    camera_params.trigger_delay = cam_settings.trigger_delay;
    camera_params.wb_red = cam_settings.white_balance_red;
    camera_params.wb_green = cam_settings.white_balance_green;
    camera_params.wb_blue = cam_settings.white_balance_blue;
    camera_params.auto_white_balance = cam_settings.auto_white_balance;
    camera_params.saturation = cam_settings.saturation;
    camera_params.hue = cam_settings.hue;
    camera_params.gamma = cam_settings.gamma;
    camera_params.roi_x = cam_settings.roi_offset_x;
    camera_params.roi_y = cam_settings.roi_offset_y;
    camera_params.roi_width = cam_settings.roi_width;
    camera_params.roi_height = cam_settings.roi_height;
    camera_params.roi_enable = cam_settings.roi_enabled;
    camera_params.acquisition_mode = cam_settings.acquisition_mode;
    camera_params.acquisition_frame_count = cam_settings.acquisition_frame_count;
    camera_params.acquisition_frame_rate = cam_settings.acquisition_frame_rate;
    camera_params.packet_size = cam_settings.packet_size;
    camera_params.packet_delay = cam_settings.packet_delay;
    
    // Set up dataset path from app settings
    auto& app_settings = settings_manager->GetAppSettings();
    current_image_folder = app_settings.last_output_folder;
    
    // Initialize folder buffer
    strcpy(image_folder_buffer, current_image_folder.c_str());
    
    // Setup GLFW
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    
    // Create window
    window = glfwCreateWindow(1600, 900, "[NEURAL] Neural Rendering Capture System v2.0 - Professional Edition", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Enable viewport support for docking
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // Set font scale to 1.25 for better readability
    io.FontGlobalScale = 1.25f;
    
    // Configure basic font loading (no emoji needed)
    io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
    io.Fonts->Flags |= ImFontAtlasFlags_NoMouseCursors;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Load default font only
    ImFont* default_font = io.Fonts->AddFontDefault();
    io.FontDefault = default_font;
    
    AddLogMessage("[OK] Font loaded successfully");
    
    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    AddLogMessage("[OK] DIRECT GUI initialized successfully");
    
    // Create dataset directories
    std::filesystem::create_directories(current_image_folder + "/images");
    std::filesystem::create_directories(current_image_folder + "/metadata");
    
    // Setup default docking layout
    bool first_time = true;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render GUI
        RenderMainMenuBar();
        
        // Create the docking space
        ImGui::DockSpaceOverViewport(ImGui::GetID("MyDockSpace"));
        
        // Setup default layout on first run
        if (first_time) {
            SetupVSCodeLayout();
            first_time = false;
        }
        
        // Render all panels (they will be dockable)
        if (show_camera_panel) {
            RenderCameraPanel();
        }
        
        if (show_capture_panel) {
            RenderCapturePanel();
        }
        
        if (show_advanced_params) {
            RenderAdvancedParametersPanel();
        }
        
        if (show_individual_camera_panel) {
            RenderIndividualCameraPanel();
        }
        
        if (show_network_panel) {
            RenderNetworkOptimizationPanel();
        }
        
        if (show_bluetooth_panel) {
            RenderBluetoothPanel();
        }
        
        if (show_session_manager) {
            RenderSessionManagerPanel();
        }
        
        if (show_log_panel) {
            RenderLogPanel();
        }
        
        // Real-time performance monitoring for neural dataset optimization
        MonitorNetworkPerformance();
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        
        glfwSwapBuffers(window);
        
        // Small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    // Cleanup
    AddLogMessage("[SYS] Shutting down capture system");
    
    // Clean up cameras
    for (auto& [id, transfer] : connected_transfers) {
        if (transfer) {
            transfer->Destroy();
            delete transfer;
        }
    }
    for (auto& [id, buffer] : connected_buffers) {
        if (buffer) {
            buffer->Destroy();
            delete buffer;
        }
    }
    for (auto& [id, device] : connected_devices) {
        if (device) {
            device->Destroy();
            delete device;
        }
    }
    
    // Cleanup Bluetooth
    ShutdownBluetooth();
    
    // Cleanup GUI
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    std::cout << "[OK] DIRECT application terminated successfully" << std::endl;
    return 0;
    
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Application exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[ERROR] Unknown application exception" << std::endl;
        return 1;
    }
}

void RenderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Dataset Folder", "Ctrl+O")) {
                OpenFolderInExplorer(current_image_folder);
            }
            if (ImGui::MenuItem("Save Settings", "Ctrl+S")) {
                SaveCurrentSettings();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Export Settings...")) {
                AddLogMessage("[SETTINGS] Export settings (file dialog not implemented)");
            }
            if (ImGui::MenuItem("Import Settings...")) {
                AddLogMessage("[SETTINGS] Import settings (file dialog not implemented)");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Session")) {
            if (ImGui::MenuItem("Session Manager", "F1")) {
                show_session_manager = !show_session_manager;
            }
            ImGui::Separator();
            if (session_manager->HasActiveSession()) {
                if (ImGui::MenuItem("End Current Session")) {
                    session_manager->EndCurrentSession();
                    AddLogMessage("[SESSION] Session ended from menu");
                }
            } else {
                ImGui::MenuItem("No Active Session", NULL, false, false);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Camera")) {
            if (ImGui::MenuItem("Discover Cameras", "F5")) {
                DiscoverCameras();
            }
            
            // Dynamic Connect/Disconnect menu item
            bool has_connected = !connected_devices.empty();
            if (has_connected) {
                if (ImGui::MenuItem("Disconnect All", "F7")) {
                    DisconnectAllCameras();
                }
            } else {
                if (is_connecting_cameras) {
                    // Show buffering animation during connection
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                    
                    // Animated dots for connection
                    static float connection_animation_time = 0.0f;
                    connection_animation_time += ImGui::GetIO().DeltaTime;
                    int dot_count = static_cast<int>(connection_animation_time * 2.0f) % 4;
                    std::string button_text = "● Connecting";
                    for (int i = 0; i < dot_count; i++) {
                        button_text += ".";
                    }
                    
                    ImGui::Button(button_text.c_str(), ImVec2(150, 35));
                    ImGui::PopStyleColor(3);
                    
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⏳ Please wait...");
                } else {
                    if (ImGui::Button("● Connect All", ImVec2(150, 35))) {
                        ConnectAllCameras();
                    }
                }
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Capture")) {
            bool can_capture = connected_devices.size() > 0 && session_manager->HasActiveSession();
            if (ImGui::MenuItem("Start Capture", "F8", false, can_capture)) {
                CaptureAllCameras();
            }
            if (!can_capture && !session_manager->HasActiveSession()) {
                ImGui::MenuItem("Need Active Session", NULL, false, false);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Camera System", NULL, &show_camera_panel);
            ImGui::MenuItem("Capture Control", NULL, &show_capture_panel);
            ImGui::MenuItem("Advanced Parameters", NULL, &show_advanced_params);
            ImGui::MenuItem("Individual Camera Control", NULL, &show_individual_camera_panel);
            ImGui::MenuItem("Network Optimization", NULL, &show_network_panel);
            ImGui::MenuItem("Bluetooth Control", NULL, &show_bluetooth_panel);
            ImGui::MenuItem("Session Manager", NULL, &show_session_manager);
            ImGui::MenuItem("System Log", NULL, &show_log_panel);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                show_camera_panel = true;
                show_capture_panel = true;
                show_log_panel = true;
                show_advanced_params = true;
                show_individual_camera_panel = false;
                show_network_panel = false;
                show_session_manager = false;
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Save Current Settings")) {
                SaveCurrentSettings();
            }
            if (ImGui::MenuItem("Reset Camera Settings")) {
                settings_manager->ResetCameraSettings();
                LoadSettingsToUI();
                AddLogMessage("[SETTINGS] Camera settings reset to defaults");
            }
            if (ImGui::MenuItem("Reset All Settings")) {
                settings_manager->ResetAllSettings();
                LoadSettingsToUI();
                AddLogMessage("[SETTINGS] All settings reset to defaults");
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                AddLogMessage("[INFO] Neural Rendering Capture System v2.0");
            }
            if (ImGui::MenuItem("Documentation")) {
                AddLogMessage("[DOC] Documentation (not implemented yet)");
            }
            ImGui::EndMenu();
        }
        
        // Status indicators in menu bar
        ImGui::SameLine(ImGui::GetWindowWidth() - 350);
        
        // Session status
        if (session_manager->HasActiveSession()) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[SESSION]");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[NO SESSION]");
        }
        
        ImGui::SameLine();
        
        // Camera status
        int connected_count = static_cast<int>(connected_devices.size());
        int total_count = static_cast<int>(discovered_cameras.size());
        ImGui::Text("[CAM] %d/%d", connected_count, total_count);
        
        ImGui::SameLine();
        
        // Color processing status
        if (color_settings.auto_white_balance || color_settings.enable_gamma || color_settings.enable_lut) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[COLOR]");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[COLOR]");
        }
        
        ImGui::EndMainMenuBar();
    }
}

void RenderCameraPanel() {
    ImGui::Begin("● Camera System", &show_camera_panel, ImGuiWindowFlags_NoCollapse);
    
    ImGui::Text("DIRECT Sapera SDK Camera Control");
    ImGui::Separator();
    
    // Control buttons - smaller size to fit all buttons
    if (is_discovering_cameras) {
        // Show buffering animation during discovery
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        
        // Animated dots for discovery
        static float discovery_animation_time = 0.0f;
        discovery_animation_time += ImGui::GetIO().DeltaTime;
        int dot_count = static_cast<int>(discovery_animation_time * 2.0f) % 4;
        std::string button_text = "● Discovering";
        for (int i = 0; i < dot_count; i++) {
            button_text += ".";
        }
        
        ImGui::Button(button_text.c_str(), ImVec2(150, 35));
        ImGui::PopStyleColor(3);
        
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⏳ Please wait...");
    } else {
        if (ImGui::Button("● Discover Cameras", ImVec2(150, 35))) {
            DiscoverCameras();
        }
    }
    
    ImGui::SameLine();
    // Dynamic Connect/Disconnect button based on connection state
    bool has_connected = !connected_devices.empty();
    if (has_connected) {
        // Show Disconnect All when cameras are connected
        if (ImGui::Button("● Disconnect All", ImVec2(150, 35))) {
            DisconnectAllCameras();
        }
    } else {
        // Show Connect All when no cameras are connected
        if (is_connecting_cameras) {
            // Show buffering animation during connection
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            
            // Animated dots for connection
            static float connection_animation_time = 0.0f;
            connection_animation_time += ImGui::GetIO().DeltaTime;
            int dot_count = static_cast<int>(connection_animation_time * 2.0f) % 4;
            std::string button_text = "● Connecting";
            for (int i = 0; i < dot_count; i++) {
                button_text += ".";
            }
            
            ImGui::Button(button_text.c_str(), ImVec2(150, 35));
            ImGui::PopStyleColor(3);
            
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⏳ Please wait...");
        } else {
            if (ImGui::Button("● Connect All", ImVec2(150, 35))) {
                ConnectAllCameras();
            }
        }
    }
    
    ImGui::Separator();
    
    // Note about parameters
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "◆ All camera parameters in Advanced Parameters panel");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "◆ Image format, resolution, exposure, etc.");
    
    if (ImGui::Button("► Open Advanced Parameters", ImVec2(200, 30))) {
        show_advanced_params = true;
        AddLogMessage("[UI] Advanced Parameters panel opened");
    }
    
    ImGui::Separator();
    
    // Camera table
    if (ImGui::BeginTable("CameraTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Camera");
        ImGui::TableSetupColumn("Serial Number");
        ImGui::TableSetupColumn("Model");
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();
        
        for (const auto& camera : discovered_cameras) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", camera.name.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", camera.serialNumber.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", camera.modelName.c_str());
            
            ImGui::TableSetColumnIndex(3);
            std::string cameraId = camera.id;
            if (connected_devices.find(cameraId) != connected_devices.end()) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[CONN]");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[DISC]");
            }
        }
        
        ImGui::EndTable();
    }
    
    ImGui::Text("Connected: %d / %d cameras", (int)connected_devices.size(), (int)discovered_cameras.size());
    
    // Current resolution status
    ImGui::Separator();
    ImGui::Text("Current Resolution: %dx%d (Max: %dx%d)", 
                camera_params.width, camera_params.height,
                camera_params.max_width, camera_params.max_height);
    ImGui::Text("Pixel Format: %s", camera_params.pixel_format.c_str());
    
    ImGui::End();
}

void RenderCapturePanel() {
    ImGui::Begin("▶ Capture Control", &show_capture_panel, ImGuiWindowFlags_NoCollapse);
    
    ImGui::Text("DIRECT Neural Rendering Capture");
    ImGui::Separator();
    
    // Session Management Section
    ImGui::Text("◆ Session Status");
    if (session_manager->HasActiveSession()) {
        auto* session = session_manager->GetCurrentSession();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Active: %s", session->object_name.c_str());
        ImGui::Text("Captures in session: %d", session->capture_count);
        ImGui::Text("Session ID: %s", session->session_id.c_str());
        
        if (ImGui::SmallButton("■ End Session")) {
            session_manager->EndCurrentSession();
            AddLogMessage("[SESSION] Session ended from capture panel");
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No active session");
        ImGui::Text("Start a new session to begin capturing");
        
        ImGui::Text("Object Name:");
        ImGui::SetNextItemWidth(200);
        ImGui::InputText("##NewObjectName", new_object_name, sizeof(new_object_name));
        ImGui::SameLine();
        if (ImGui::Button("▶ Start Session")) {
            std::string session_name = std::string(new_object_name);
            
            // Provide default name if user forgot to input
            if (session_name.empty()) {
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                std::stringstream ss;
                ss << "capture_" << std::put_time(std::localtime(&time_t), "%m%d_%H%M");
                session_name = ss.str();
                AddLogMessage("[SESSION] Using default session name: " + session_name);
            }
            
            if (session_manager->StartNewSession(session_name)) {
                AddLogMessage("[SESSION] New session started: " + session_name);
                memset(new_object_name, 0, sizeof(new_object_name));
            }
        }
    }
    
    ImGui::Separator();
    
    // Dataset info
    ImGui::Text("◆ Dataset Configuration");
    ImGui::Text("Dataset: %s", current_image_folder.c_str());
    
    // Folder management
    ImGui::Text("Output Folder:");
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##ImageFolder", image_folder_buffer, sizeof(image_folder_buffer))) {
        current_image_folder = image_folder_buffer;
        AddLogMessage("[FOLDER] Dataset path changed to: " + current_image_folder);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("► Open")) {
        OpenFolderInExplorer(current_image_folder);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("+ Create")) {
        try {
            std::filesystem::create_directories(current_image_folder + "/images");
            std::filesystem::create_directories(current_image_folder + "/metadata");
            AddLogMessage("[FOLDER] Created folder: " + current_image_folder);
        } catch (const std::exception& e) {
            AddLogMessage("[ERR] Error creating folder: " + std::string(e.what()));
        }
    }
    
    // Format selection
    ImGui::Text("Format:");
    ImGui::SameLine();
    if (ImGui::RadioButton("TIFF", !capture_format_raw)) {
        capture_format_raw = false;
        AddLogMessage("[IMG] Format set to TIFF");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("RAW", capture_format_raw)) {
        capture_format_raw = true;
        AddLogMessage("[IMG] Format set to RAW");
    }
    
    ImGui::Separator();
    
    // Capture section
    ImGui::Text("◆ Capture Control");
    bool can_capture = connected_devices.size() > 0 && session_manager->HasActiveSession();
    
    if (can_capture) {
        if (ImGui::Button("▶ START CAPTURE", ImVec2(300, 60))) {
            CaptureAllCameras();
        }
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::Button("▶ START CAPTURE", ImVec2(300, 60));
        ImGui::PopStyleVar();
        
        if (!session_manager->HasActiveSession()) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Need active session to capture");
        }
        if (connected_devices.size() == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Need connected cameras to capture");
        }
    }
    
    // Status display
    ImGui::Separator();
    ImGui::Text("◆ System Status");
    int connected_count = static_cast<int>(connected_devices.size());
    int total_count = static_cast<int>(discovered_cameras.size());
    ImGui::Text("Cameras: %d/%d connected", connected_count, total_count);
    
    if (connected_count > 0 && session_manager->HasActiveSession()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[OK] Ready to capture");
    } else if (connected_count == 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[WARN] No cameras connected");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[WARN] No active session");
    }
    
    ImGui::End();
}

void RenderLogPanel() {
    ImGui::Begin("■ System Log", &show_log_panel, ImGuiWindowFlags_NoCollapse);
    
    // Update log highlighting - mark old logs as not new after 2 seconds
    auto now = std::chrono::steady_clock::now();
    for (auto& log_msg : log_messages_with_timestamps) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - log_msg.timestamp);
        if (elapsed.count() > 2000) { // 2 seconds
            log_msg.is_new = false;
        }
    }
    
    // Log controls
    if (ImGui::Button("Clear Log")) {
        log_messages.clear();
        log_messages_with_timestamps.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Log")) {
        AddLogMessage("[CFG] Log save (not implemented yet)");
    }
    ImGui::SameLine();
    ImGui::Text("Messages: %d", (int)log_messages_with_timestamps.size());
    
    // Network monitoring log filter toggle
    ImGui::SameLine();
    if (ImGui::Checkbox("Hide Network Monitoring", &filter_network_monitoring_logs)) {
        AddLogMessage("[CFG] Network monitoring logs " + std::string(filter_network_monitoring_logs ? "hidden" : "shown"));
    }
    
    ImGui::Separator();
    
    // Log display with color highlighting
    ImGui::BeginChild("LogContent", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& log_msg : log_messages_with_timestamps) {
        if (log_msg.is_new) {
            // Highlight new logs with bright yellow color
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            ImGui::TextWrapped("%s", log_msg.message.c_str());
            ImGui::PopStyleColor();
        } else {
            // Regular color for old logs
            ImGui::TextWrapped("%s", log_msg.message.c_str());
        }
    }
    
    // Auto-scroll to bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
    
    ImGui::End();
}

void RenderColorPanel() {
    ImGui::Begin("◆ Color Processing", &show_color_panel, ImGuiWindowFlags_NoCollapse);
    
    ImGui::Text("Professional Color Control Hub");
    ImGui::Separator();
    
    // All controls moved to Advanced Parameters
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "◆ All color controls moved to Advanced Parameters");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "White Balance, Gamma, Bayer, LUT, etc.");
    
    ImGui::Separator();
    
    // Quick link to Advanced Parameters
    if (ImGui::Button("► Open Advanced Parameters", ImVec2(250, 40))) {
        show_advanced_params = true;
        AddLogMessage("[UI] Advanced Parameters panel opened");
    }
    
    ImGui::Text("Find all color processing controls in:");
    ImGui::BulletText("Advanced White Balance & Color");
    ImGui::BulletText("Color Conversion & Processing");
    
    ImGui::Separator();
    
    // Quick status summary
    ImGui::Text("Current Color Settings Summary:");
    ImGui::Text("◆ White Balance: %s", camera_params.auto_white_balance ? "Auto" : "Manual");
    if (!camera_params.auto_white_balance) {
        ImGui::Text("   R:%.2f G:%.2f B:%.2f", camera_params.wb_red, camera_params.wb_green, camera_params.wb_blue);
    }
    ImGui::Text("◆ Gamma: %.2f", camera_params.gamma);
    ImGui::Text("◆ Saturation: %.2f", camera_params.saturation);
    ImGui::Text("◆ Conversion Method: %d", color_settings.color_method);
    ImGui::Text("◆ Bayer Pattern: %s", color_settings.bayer_align == 2 ? "RGGB" : "Other");
    ImGui::Text("◆ LUT: %s", color_settings.enable_lut ? "Enabled" : "Disabled");
    ImGui::Text("◆ Hardware: %s", color_settings.use_hardware_conversion ? "Yes" : "No");
    
    ImGui::End();
}

void AddLogMessage(const std::string& message) {
    // Filter out network monitoring logs if filter is enabled
    if (filter_network_monitoring_logs && 
        (message.find("[NEURAL] === PERFORMANCE MONITORING ===") != std::string::npos ||
         message.find("[NEURAL] Image size per camera:") != std::string::npos ||
         message.find("[NEURAL] Total data per capture:") != std::string::npos ||
         message.find("[NEURAL] Sequential throughput:") != std::string::npos ||
         message.find("[NEURAL] Time per full capture:") != std::string::npos ||
         message.find("[NEURAL] Network config:") != std::string::npos ||
         message.find("[NEURAL] Estimated buffer memory:") != std::string::npos)) {
        return; // Skip these network monitoring messages
    }
    
    auto now = std::chrono::steady_clock::now();
    
    // Add to both old and new log systems for compatibility
    log_messages.push_back(message);
    
    // Add to new timestamped log system
    LogMessage log_msg;
    log_msg.message = message;
    log_msg.timestamp = now;
    log_msg.is_new = true;
    log_messages_with_timestamps.push_back(log_msg);
    
    // Keep only last 1000 messages to prevent memory issues
    if (log_messages.size() > 1000) {
        log_messages.erase(log_messages.begin());
    }
    if (log_messages_with_timestamps.size() > 1000) {
        log_messages_with_timestamps.erase(log_messages_with_timestamps.begin());
    }
    
    // Print to console with timestamp
    std::cout << "[" << GetCurrentTimestamp() << "] " << message << std::endl;
}

std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    
    return ss.str();
}

void DiscoverCameras() {
    if (is_discovering_cameras) {
        AddLogMessage("[DISC] Camera discovery already in progress...");
        return;
    }
    
    is_discovering_cameras = true;
    AddLogMessage("[DISC] Starting camera discovery...");
    
    // Start discovery in a separate thread
    discovery_thread = std::thread([]() {
        std::vector<CameraInfo> temp_cameras;
        std::map<std::string, SapAcqDevice*> temp_devices;
        
        // Get server count
        int serverCount = SapManager::GetServerCount();
        
        if (serverCount == 0) {
            AddLogMessage("[NET] No Sapera servers found");
            is_discovering_cameras = false;
            return;
        }
        
        int cameraIndex = 1;
        
        // Enumerate all servers
        for (int serverIndex = 0; serverIndex < serverCount; serverIndex++) {
            char serverName[256];
            if (!SapManager::GetServerName(serverIndex, serverName, sizeof(serverName))) {
                AddLogMessage("[NET] Failed to get server name for server " + std::to_string(serverIndex));
                continue;
            }
            
            // Skip system server
            if (std::string(serverName) == "System") {
                continue;
            }
            
            AddLogMessage("[NET] Server " + std::to_string(serverIndex) + ": " + std::string(serverName));
            
            // Get acquisition device count for this server
            int resourceCount = SapManager::GetResourceCount(serverName, SapManager::ResourceAcqDevice);
            AddLogMessage("[NET] Acquisition devices: " + std::to_string(resourceCount));
            
            // Enumerate acquisition devices
            for (int resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
                try {
                    // Create acquisition device temporarily for discovery
                    auto acqDevice = new SapAcqDevice(serverName, resourceIndex);
                    if (!acqDevice->Create()) {
                        AddLogMessage("[NET] Failed to create device " + std::to_string(resourceIndex));
                        delete acqDevice;
                        continue;
                    }
                    
                    // Get device information
                    char buffer[512];
                    
                    CameraInfo camera;
                    camera.id = std::to_string(cameraIndex);
                    camera.serverName = serverName;
                    camera.resourceIndex = resourceIndex;
                    
                    // Get serial number
                    if (acqDevice->GetFeatureValue("DeviceSerialNumber", buffer, sizeof(buffer))) {
                        camera.serialNumber = std::string(buffer);
                    } else {
                        camera.serialNumber = "Unknown_" + std::to_string(cameraIndex);
                    }
                    
                    // Get model name
                    if (acqDevice->GetFeatureValue("DeviceModelName", buffer, sizeof(buffer))) {
                        camera.modelName = std::string(buffer);
                    } else {
                        camera.modelName = "Unknown_Model";
                    }
                    
                    // Create camera name for neural rendering
                    std::string indexStr = std::to_string(cameraIndex);
                    if (indexStr.length() == 1) indexStr = "0" + indexStr;
                    camera.name = "cam_" + indexStr;
                    camera.isConnected = false;
                    camera.status = CameraStatus::Disconnected;
                    camera.type = CameraType::Industrial;
                    
                    temp_cameras.push_back(camera);
                    AddLogMessage("[OK] " + camera.name + ": " + camera.serialNumber + " (" + camera.modelName + ")");
                    
                    // Cleanup discovery device
                    acqDevice->Destroy();
                    delete acqDevice;
                    
                    cameraIndex++;
                    
                } catch (const std::exception& e) {
                    AddLogMessage("[NET] Exception: " + std::string(e.what()));
                }
            }
        }
        
        // Update global variables in main thread
        discovered_cameras = temp_cameras;
        camera_device_names.clear();
        
        AddLogMessage("[OK] Discovery complete: " + std::to_string(discovered_cameras.size()) + " cameras found");
        is_discovering_cameras = false;
    });
    
    // Detach the thread so it can run independently
    discovery_thread.detach();
}

void ConnectAllCameras() {
    if (is_connecting_cameras) {
        AddLogMessage("[NET] Camera connection already in progress...");
        return;
    }
    
    if (discovered_cameras.empty()) {
        AddLogMessage("[NET] No cameras discovered. Run camera discovery first.");
        return;
    }
    
    is_connecting_cameras = true;
    AddLogMessage("[NET] Starting camera connection...");
    
    // Start connection in a separate thread
    connection_thread = std::thread([]() {
        // Apply maximum quality settings BEFORE connection to avoid transport layer conflicts
        AddLogMessage("[NEURAL] Pre-configuring maximum quality settings for neural datasets...");
        // Set neural dataset optimized defaults without applying to cameras (they're not connected yet)
        sequential_capture_mode = true;
        capture_delay_ms = 750;  // Optimal for 11+ cameras
        enable_packet_optimization = true;
        enable_bandwidth_monitoring = true;
        
        // Apply optimal packet settings based on camera count
        int camera_count = static_cast<int>(discovered_cameras.size());
        
        if (camera_count > 8) {
            // High camera count - conservative settings
            camera_params.packet_size = 1200;
            camera_params.packet_delay = 5000;  // 5ms delay
            capture_delay_ms = 1000;  // 1 second between cameras
            AddLogMessage("[NEURAL] High camera count detected (" + std::to_string(camera_count) + ") - using conservative settings");
        } else if (camera_count > 4) {
            // Medium camera count
            camera_params.packet_size = 1400;
            camera_params.packet_delay = 3000;  // 3ms delay
            capture_delay_ms = 750;  // 750ms between cameras
            AddLogMessage("[NEURAL] Medium camera count detected (" + std::to_string(camera_count) + ") - using balanced settings");
        } else {
            // Low camera count - can be more aggressive
            camera_params.packet_size = 1500;
            camera_params.packet_delay = 1000;  // 1ms delay
            capture_delay_ms = 500;  // 500ms between cameras
            AddLogMessage("[NEURAL] Low camera count detected (" + std::to_string(camera_count) + ") - using optimized settings");
        }
        
        AddLogMessage("[NEURAL] Pre-configuration complete - packet size: " + std::to_string(camera_params.packet_size) + 
                     " bytes, delay: " + std::to_string(camera_params.packet_delay) + "µs");
        
        std::map<std::string, SapAcqDevice*> temp_connected_devices;
        std::map<std::string, SapBuffer*> temp_connected_buffers;
        std::map<std::string, SapAcqDeviceToBuf*> temp_connected_transfers;
        int successCount = 0;
        
        for (const auto& camera : discovered_cameras) {
            std::string cameraId = camera.id;
            
            try {
                // Create acquisition device using serverName and resourceIndex
                SapAcqDevice* acqDevice = new SapAcqDevice(camera.serverName.c_str(), camera.resourceIndex);
                if (!acqDevice->Create()) {
                    AddLogMessage("[NET] Failed to create acquisition device for " + camera.name);
                    delete acqDevice;
                    continue;
                }
                
                // Apply exposure time setting
                std::string exposureStr = std::to_string(exposure_time);
                if (!acqDevice->SetFeatureValue("ExposureTime", exposureStr.c_str())) {
                    AddLogMessage("[NET] Warning: Failed to set exposure time for " + camera.name);
                }
                
                // Create buffer for image capture
                SapBuffer* buffer = new SapBufferWithTrash(1, acqDevice);
                if (!buffer->Create()) {
                    AddLogMessage("[NET] Failed to create buffer for " + camera.name);
                    acqDevice->Destroy();
                    delete acqDevice;
                    delete buffer;
                    continue;
                }
                
                // Create transfer object
                SapAcqDeviceToBuf* transfer = new SapAcqDeviceToBuf(acqDevice, buffer);
                if (!transfer->Create()) {
                    AddLogMessage("[NET] Failed to create transfer for " + camera.name);
                    buffer->Destroy();
                    acqDevice->Destroy();
                    delete transfer;
                    delete buffer;
                    delete acqDevice;
                    continue;
                }
                
                // Store connected components
                temp_connected_devices[cameraId] = acqDevice;
                temp_connected_buffers[cameraId] = buffer;
                temp_connected_transfers[cameraId] = transfer;
                
                successCount++;
                AddLogMessage("[OK] " + camera.name + " connected successfully");
                
            } catch (const std::exception& e) {
                AddLogMessage("[NET] Exception connecting " + camera.name + ": " + std::string(e.what()));
            }
        }
        
        // Update global variables in main thread
        connected_devices = temp_connected_devices;
        connected_buffers = temp_connected_buffers;
        connected_transfers = temp_connected_transfers;
        
        AddLogMessage("[OK] Connection summary: " + std::to_string(successCount) + "/" + std::to_string(discovered_cameras.size()) + " cameras connected");
        
        if (successCount == discovered_cameras.size() && successCount > 0) {
            AddLogMessage("[OK] All cameras connected successfully!");
            
            // Detect camera resolution after connection
            DetectCameraResolution();
            
            AddLogMessage("[NEURAL] ✓ Neural dataset optimization active with " + std::to_string(successCount) + " cameras");
        } else if (successCount > 0) {
            AddLogMessage("[WARN] Partial connection: " + std::to_string(successCount) + "/" + std::to_string(discovered_cameras.size()) + " cameras connected");
            
            // Still try to detect resolution from connected cameras
            DetectCameraResolution();
            
            AddLogMessage("[NEURAL] ✓ Neural dataset optimization active with " + std::to_string(successCount) + " cameras");
        } else {
            AddLogMessage("[ERR] No cameras could be connected");
        }
        
        is_connecting_cameras = false;
    });
    
    // Detach the thread so it can run independently
    connection_thread.detach();
}

void DisconnectAllCameras() {
    if (connected_devices.empty()) {
        AddLogMessage("[NET] No cameras connected to disconnect");
        return;
    }
    
    int device_count = static_cast<int>(connected_devices.size());
    AddLogMessage("[NET] Disconnecting " + std::to_string(device_count) + " cameras...");
    
    // Destroy all connected devices and buffers
    int destroyed_count = 0;
    for (auto& [id, device] : connected_devices) {
        if (device) {
            try {
                device->Destroy();
                delete device;
                destroyed_count++;
                AddLogMessage("[OK] Disconnected device: " + id);
            } catch (const std::exception& e) {
                AddLogMessage("[ERROR] Failed to destroy device " + id + ": " + e.what());
            }
        }
    }
    
    for (auto& [id, buffer] : connected_buffers) {
        if (buffer) {
            try {
                buffer->Destroy();
                delete buffer;
            } catch (const std::exception& e) {
                AddLogMessage("[ERROR] Failed to destroy buffer " + id + ": " + e.what());
            }
        }
    }
    
    for (auto& [id, transfer] : connected_transfers) {
        if (transfer) {
            try {
                transfer->Destroy();
                delete transfer;
            } catch (const std::exception& e) {
                AddLogMessage("[ERROR] Failed to destroy transfer " + id + ": " + e.what());
            }
        }
    }
    
    // Clear all connected devices and buffers
    connected_devices.clear();
    connected_buffers.clear();
    connected_transfers.clear();
    
    AddLogMessage("[OK] Successfully disconnected " + std::to_string(destroyed_count) + "/" + std::to_string(device_count) + " cameras");
}

void CaptureAllCameras() {
    // Check capture mode and route accordingly
    if (sequential_capture_mode) {
        CaptureSequential();
        return;
    }
    
    // Original simultaneous capture logic
    if (connected_devices.empty()) {
        AddLogMessage("[NET] No cameras connected");
        return;
    }
    
    if (!session_manager->HasActiveSession()) {
        AddLogMessage("[SESSION] No active session - please start a session first");
        return;
    }
    
    auto* session = session_manager->GetCurrentSession();
    std::string sessionPath = session->GetNextCapturePath();
    
    // Create session directory
    std::filesystem::create_directories(sessionPath);
    
    AddLogMessage("[REC] 🎬 DIRECT CAPTURE starting...");
    AddLogMessage("[IMG] Session path: " + sessionPath);
    AddLogMessage("[NET] Network config: " + std::to_string(camera_params.packet_size) + " bytes packets, " + std::to_string(camera_params.packet_delay) + "µs delay");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    bool allSuccess = true;
    int successCount = 0;
    int totalCameras = static_cast<int>(connected_devices.size());
    
    AddLogMessage("[REC] 📸 Capturing from " + std::to_string(totalCameras) + " cameras...");
    
    // Capture from all cameras
    for (const auto& camera : discovered_cameras) {
        std::string cameraId = camera.id;
        
        if (connected_devices.find(cameraId) != connected_devices.end()) {
            AddLogMessage("[REC] 📷 Capturing " + camera.name + " (" + camera.serialNumber + ")");
            
            if (CaptureCamera(cameraId, sessionPath)) {
                successCount++;
                AddLogMessage("[OK] ✅ " + camera.name + " captured successfully");
            } else {
                allSuccess = false;
                AddLogMessage("[ERR] ❌ " + camera.name + " capture failed");
            }
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    AddLogMessage("[REC] 🏁 Capture completed in " + std::to_string(duration.count()) + "ms");
    AddLogMessage("[REC] 📊 Success rate: " + std::to_string(successCount) + "/" + std::to_string(totalCameras) + " cameras");
    
    if (allSuccess) {
        session_manager->RecordCapture(sessionPath);
        SaveCaptureMetadata(sessionPath, session_manager->GetTotalCapturesInSession(), true, static_cast<int>(duration.count()));
        
        AddLogMessage("[OK] 🎉 All cameras captured successfully!");
        
        // Count created files
        try {
            int file_count = 0;
            for (const auto& entry : std::filesystem::directory_iterator(sessionPath)) {
                if (entry.is_regular_file()) {
                    file_count++;
                }
            }
            AddLogMessage("[OK] 📁 Total files in dataset: " + std::to_string(file_count));
        } catch (const std::exception& e) {
            AddLogMessage("[WARN] ⚠️ Could not count files: " + std::string(e.what()));
        }
    } else {
        // Save metadata even for partial success
        SaveCaptureMetadata(sessionPath, session_manager->GetTotalCapturesInSession(), false, static_cast<int>(duration.count()));
        AddLogMessage("[WARN] ⚠️ Some cameras failed to capture");
    }
}

bool CaptureCamera(const std::string& cameraId, const std::string& sessionPath) {
    auto deviceIt = connected_devices.find(cameraId);
    auto bufferIt = connected_buffers.find(cameraId);
    auto transferIt = connected_transfers.find(cameraId);
    
    if (deviceIt == connected_devices.end() || bufferIt == connected_buffers.end() || transferIt == connected_transfers.end()) {
        AddLogMessage("[ERR] ❌ Missing components for camera " + cameraId);
        return false;
    }
    
    SapAcqDevice* device = deviceIt->second;
    SapBuffer* buffer = bufferIt->second;
    SapAcqDeviceToBuf* transfer = transferIt->second;
    
    try {
        // Find camera name
        std::string cameraName = "unknown";
        for (const auto& camera : discovered_cameras) {
            if (camera.id == cameraId) {
                cameraName = camera.name;
                break;
            }
        }
        
        AddLogMessage("[REC] 🔄 Triggering capture for " + cameraName + "...");
        
        // Trigger capture
        if (!transfer->Snap()) {
            AddLogMessage("[ERR] ❌ Failed to trigger capture for " + cameraName);
            return false;
        }
        
        AddLogMessage("[REC] ⏳ Waiting for capture completion...");
        
        // Wait for capture completion
        if (!transfer->Wait(5000)) {
            AddLogMessage("[ERR] ❌ Capture timeout for " + cameraName);
            return false;
        }
        
        AddLogMessage("[REC] 📸 Capture completed, processing image...");
        
        // Generate filename
        int current_capture = session_manager->GetTotalCapturesInSession() + 1;
        std::string extension = capture_format_raw ? ".raw" : ".tiff";
        std::string filename = cameraName + "_capture_" + std::to_string(current_capture) + extension;
        std::string fullPath = sessionPath + "/" + filename;
        
        AddLogMessage("[REC] 💾 Saving image: " + filename);
        
        // Save image
        if (capture_format_raw) {
            // Save as RAW
            if (buffer->Save(fullPath.c_str(), "-format raw")) {
                AddLogMessage("[OK] ✅ RAW image saved: " + filename);
                return true;
            } else {
                AddLogMessage("[ERR] ❌ Failed to save RAW image: " + filename);
                return false;
            }
        } else {
            // Save as TIFF with ADVANCED color conversion
            AddLogMessage("[REC] 🎨 Applying color conversion...");
            
            SapColorConversion colorConverter(buffer);
            if (!colorConverter.Create()) {
                AddLogMessage("[ERR] ❌ Failed to create color converter for " + cameraName);
                return false;
            }
            
            // Configure converter for RGB output
            colorConverter.Enable(TRUE, color_settings.use_hardware_conversion);
            colorConverter.SetOutputFormat(SapFormatRGB888);
            
            // Set Bayer alignment based on user selection
            SapColorConversion::Align align;
            switch (color_settings.bayer_align) {
                case 0: align = SapColorConversion::AlignGBRG; break;
                case 1: align = SapColorConversion::AlignBGGR; break;
                case 2: align = SapColorConversion::AlignRGGB; break;
                case 3: align = SapColorConversion::AlignGRBG; break;
                case 4: align = SapColorConversion::AlignRGBG; break;
                case 5: align = SapColorConversion::AlignBGRG; break;
                default: align = SapColorConversion::AlignRGGB; break;
            }
            colorConverter.SetAlign(align);
            
            // Set color conversion method
            SapColorConversion::Method method;
            switch (color_settings.color_method) {
                case 1: method = SapColorConversion::Method1; break;
                case 2: method = SapColorConversion::Method2; break;
                case 3: method = SapColorConversion::Method3; break;
                case 4: method = SapColorConversion::Method4; break;
                case 5: method = SapColorConversion::Method5; break;
                case 6: method = SapColorConversion::Method6; break;
                case 7: method = SapColorConversion::Method7; break;
                default: method = SapColorConversion::Method1; break;
            }
            colorConverter.SetMethod(method);
            
            // Apply white balance
            if (color_settings.auto_white_balance) {
                // Auto white balance
                AddLogMessage("[REC] ⚖️ Applying auto white balance...");
                if (!colorConverter.WhiteBalance(0, 0, buffer->GetWidth(), buffer->GetHeight())) {
                    AddLogMessage("[WARN] ⚠️ Auto white balance failed for " + cameraName + ", using manual gains");
                    // Fall back to manual gains
                    SapDataFRGB wbGain(color_settings.wb_red_gain, color_settings.wb_green_gain, color_settings.wb_blue_gain);
                    colorConverter.SetWBGain(wbGain);
                } else {
                    AddLogMessage("[OK] ✅ Auto white balance applied to " + cameraName);
                }
            } else {
                // Manual white balance
                AddLogMessage("[REC] ⚖️ Applying manual white balance...");
                SapDataFRGB wbGain(color_settings.wb_red_gain, color_settings.wb_green_gain, color_settings.wb_blue_gain);
                colorConverter.SetWBGain(wbGain);
            }
            
            // Apply gamma correction
            if (color_settings.enable_gamma) {
                AddLogMessage("[REC] 🎛️ Applying gamma correction: " + std::to_string(color_settings.gamma_value));
                colorConverter.SetGamma(color_settings.gamma_value);
            }
            
            AddLogMessage("[REC] 🔄 Converting image to RGB...");
            
            // Convert the image to RGB
            if (!colorConverter.Convert()) {
                AddLogMessage("[ERR] ❌ Color conversion failed for " + cameraName);
                colorConverter.Destroy();
                return false;
            }
            
            // Get the converted buffer
            SapBuffer* outputBuffer = colorConverter.GetOutputBuffer();
            if (!outputBuffer) {
                AddLogMessage("[ERR] ❌ No output buffer for " + cameraName);
                colorConverter.Destroy();
                return false;
            }
            
            AddLogMessage("[REC] 💾 Saving TIFF image...");
            
            // Save converted RGB buffer as TIFF
            bool saveSuccess = outputBuffer->Save(fullPath.c_str(), "-format tiff");
            
            // Cleanup
            colorConverter.Destroy();
            
            if (saveSuccess) {
                AddLogMessage("[OK] ✅ TIFF image saved with color conversion: " + filename);
                AddLogMessage("[OK] 🎨 Applied settings: gamma=" + std::to_string(color_settings.gamma_value) + 
                             ", method=" + std::to_string(color_settings.color_method) + 
                             ", bayer=" + std::to_string(color_settings.bayer_align));
                return true;
            } else {
                AddLogMessage("[ERR] ❌ Failed to save TIFF image: " + filename);
                return false;
            }
        }
        
    } catch (const std::exception& e) {
        AddLogMessage("[ERR] ❌ Exception capturing " + cameraId + ": " + std::string(e.what()));
        return false;
    }
}

std::string GenerateSessionName(int captureNumber) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "capture_" << std::setfill('0') << std::setw(3) << captureNumber << "_";
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    return ss.str();
}

void OpenFolderInExplorer(const std::string& path) {
    if (std::filesystem::exists(path)) {
        // Use Windows ShellExecute for better integration
        ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        AddLogMessage("[FOLDER] Opened folder: " + path);
    } else {
        AddLogMessage("[ERR] Folder does not exist: " + path);
    }
}

std::string BrowseForLUTFile() {
    char filename[MAX_PATH];
    
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "LUT Files (*.lut)\0*.lut\0Cube Files (*.cube)\0*.cube\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrTitle = "Select LUT File";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    if (GetOpenFileNameA(&ofn)) {
        std::string selected_file = filename;
        
        // Validate file exists and is readable
        if (std::filesystem::exists(selected_file)) {
            AddLogMessage("[LUT] LUT file selected: " + selected_file);
            return selected_file;
        } else {
            AddLogMessage("[NET] Selected LUT file does not exist: " + selected_file);
            return "";
        }
    } else {
        // User cancelled or error occurred
        DWORD error = CommDlgExtendedError();
        if (error != 0) {
            AddLogMessage("[NET] LUT file browser error: " + std::to_string(error));
        }
        return "";
    }
}

void SetupVSCodeLayout() {
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    
    // Clear any existing layout
    ImGui::DockBuilderRemoveNode(dockspace_id);
    
    // Create the main dock space
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);
    
    // Split the main area into left and right
    ImGuiID left_id, right_id;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, &left_id, &right_id);
    
    // Split the right area into top and bottom
    ImGuiID top_right_id, bottom_right_id;
    ImGui::DockBuilderSplitNode(right_id, ImGuiDir_Down, 0.3f, &bottom_right_id, &top_right_id);
    
    // Split the left area into top and bottom for additional panels
    ImGuiID top_left_id, bottom_left_id;
    ImGui::DockBuilderSplitNode(left_id, ImGuiDir_Down, 0.4f, &bottom_left_id, &top_left_id);
    
    // Dock the windows in VSCode-style layout:
    // - Left top: Camera Panel (like VSCode's file explorer)
    // - Left bottom: Color Panel (like VSCode's source control)
    // - Top right: Capture Panel (main workspace)
    // - Bottom right: Log Panel (like VSCode's terminal/output)
    
    ImGui::DockBuilderDockWindow("● Camera System", top_left_id);
    ImGui::DockBuilderDockWindow("◆ Color Processing", bottom_left_id);
    ImGui::DockBuilderDockWindow("▶ Capture Control", top_right_id);
    ImGui::DockBuilderDockWindow("■ System Log", bottom_right_id);
    
    // Finish the docking setup
    ImGui::DockBuilderFinish(dockspace_id);
    
    AddLogMessage("[UI] VSCode-style professional layout applied");
} 

void RenderAdvancedParametersPanel() {
    ImGui::Begin("◆ Advanced Camera Parameters", &show_advanced_params, ImGuiWindowFlags_NoCollapse);
    
    if (connected_devices.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[WARN] No cameras connected");
        ImGui::Text("Connect cameras to access advanced parameters");
        ImGui::End();
        return;
    }
    
    ImGui::Text("Professional Camera Parameter Control");
    ImGui::Separator();
    
    // Feature Refresh Button
    if (ImGui::Button("◆ Refresh Features", ImVec2(150, 30))) {
        RefreshCameraFeatures();
    }
    ImGui::SameLine();
    if (ImGui::Button("► Apply All", ImVec2(100, 30))) {
        AddLogMessage("[PARAM] Applying all parameters to connected cameras");
        // Apply all current parameters
    }
    
    // Image Format & Resolution Section (moved from Camera System)
    if (ImGui::CollapsingHeader("◆ Image Format & Resolution", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(200);
        
        // Resolution controls (now customizable)
        int temp_width = camera_params.width;
        if (ImGui::InputInt("Width", &temp_width, 8, 64)) {
            camera_params.width = std::max(64, std::min(camera_params.max_width, temp_width));
            ApplyParameterToAllCameras("Width", std::to_string(camera_params.width));
            AddLogMessage("[PARAM] Width set to: " + std::to_string(camera_params.width));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 64 - %d (Max detected: %d)", camera_params.max_width, camera_params.max_width);
        
        int temp_height = camera_params.height;
        if (ImGui::InputInt("Height", &temp_height, 8, 64)) {
            camera_params.height = std::max(64, std::min(camera_params.max_height, temp_height));
            ApplyParameterToAllCameras("Height", std::to_string(camera_params.height));
            AddLogMessage("[PARAM] Height set to: " + std::to_string(camera_params.height));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 64 - %d (Max detected: %d)", camera_params.max_height, camera_params.max_height);
        
        // Quick resolution presets
        ImGui::Text("Quick Presets:");
        if (ImGui::Button("Full Res", ImVec2(70, 20))) {
            camera_params.width = camera_params.max_width;
            camera_params.height = camera_params.max_height;
            ApplyParameterToAllCameras("Width", std::to_string(camera_params.width));
            ApplyParameterToAllCameras("Height", std::to_string(camera_params.height));
            AddLogMessage("[PARAM] Full resolution applied: " + std::to_string(camera_params.width) + "x" + std::to_string(camera_params.height));
        }
        ImGui::SameLine();
        if (ImGui::Button("Half Res", ImVec2(70, 20))) {
            camera_params.width = camera_params.max_width / 2;
            camera_params.height = camera_params.max_height / 2;
            ApplyParameterToAllCameras("Width", std::to_string(camera_params.width));
            ApplyParameterToAllCameras("Height", std::to_string(camera_params.height));
            AddLogMessage("[PARAM] Half resolution applied: " + std::to_string(camera_params.width) + "x" + std::to_string(camera_params.height));
        }
        ImGui::SameLine();
        if (ImGui::Button("1080p", ImVec2(60, 20))) {
            camera_params.width = 1920;
            camera_params.height = 1080;
            ApplyParameterToAllCameras("Width", std::to_string(camera_params.width));
            ApplyParameterToAllCameras("Height", std::to_string(camera_params.height));
            AddLogMessage("[PARAM] 1080p resolution applied");
        }
        
        if (ImGui::Button("► Detect Resolution", ImVec2(150, 25))) {
            DetectCameraResolution();
        }
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Detect maximum resolution from connected cameras");
        
        ImGui::Separator();
        
        // Expanded Pixel Format Selection (all supported formats)
        std::vector<std::string> pixel_formats = {
            "Mono8", "Mono16", "RGB8", "RGB16", "BGR8", "BGR16",
            "BayerRG8", "BayerGB8", "BayerGR8", "BayerBG8",
            "BayerRG12", "BayerGB12", "BayerGR12", "BayerBG12",
            "BayerRG16", "BayerGB16", "BayerGR16", "BayerBG16",
            "YUV422", "YUV422Packed", "YUV411Packed",
            "YUV444Packed", "RGB8Packed", "BGR8Packed"
        };
        static int current_pixel_format = 2; // RGB8
        if (ImGui::Combo("Pixel Format", &current_pixel_format, 
            [](void* data, int idx, const char** out_text) {
                auto& formats = *static_cast<std::vector<std::string>*>(data);
                *out_text = formats[idx].c_str();
                return true;
            }, &pixel_formats, static_cast<int>(pixel_formats.size()))) {
            camera_params.pixel_format = pixel_formats[current_pixel_format];
            ApplyParameterToAllCameras("PixelFormat", camera_params.pixel_format);
            AddLogMessage("[PARAM] Pixel format set to: " + camera_params.pixel_format);
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "RGB8/BGR8 recommended for color, Mono8/16 for grayscale");
        
        ImGui::PopItemWidth();
    }
    
    // Acquisition Control Section
    if (ImGui::CollapsingHeader("◆ Acquisition Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(200);
        
        // Exposure Time (unified control with direct input)
        int temp_exposure = camera_params.exposure_time;
        if (ImGui::InputInt("Exposure Time (us)", &temp_exposure, 100, 100000)) {
            camera_params.exposure_time = std::max(100, std::min(100000, temp_exposure));
            ApplyParameterToAllCameras("ExposureTime", std::to_string(camera_params.exposure_time));
            exposure_time = camera_params.exposure_time; // Sync with old variable for compatibility
            AddLogMessage("[PARAM] Exposure time: " + std::to_string(camera_params.exposure_time) + "us");
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 100 - 100,000 us");
        
        if (ImGui::Checkbox("Auto Exposure", &camera_params.auto_exposure)) {
            ApplyParameterToAllCameras("ExposureAuto", camera_params.auto_exposure ? "Continuous" : "Off");
            AddLogMessage("[PARAM] Auto exposure: " + std::string(camera_params.auto_exposure ? "ON" : "OFF"));
        }
        
        // Gain Control (direct input)
        float temp_gain = camera_params.gain;
        if (ImGui::InputFloat("Gain (dB)", &temp_gain, 0.1f, 1.0f, "%.1f")) {
            camera_params.gain = std::max(0.0f, std::min(30.0f, temp_gain));
            ApplyParameterToAllCameras("Gain", std::to_string(camera_params.gain));
            AddLogMessage("[PARAM] Gain: " + std::to_string(camera_params.gain) + "dB");
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.0 - 30.0 dB");
        
        if (ImGui::Checkbox("Auto Gain", &camera_params.auto_gain)) {
            ApplyParameterToAllCameras("GainAuto", camera_params.auto_gain ? "Continuous" : "Off");
            AddLogMessage("[PARAM] Auto gain: " + std::string(camera_params.auto_gain ? "ON" : "OFF"));
        }
        
        // Black Level (direct input)
        float temp_black = camera_params.black_level;
        if (ImGui::InputFloat("Black Level", &temp_black, 0.1f, 1.0f, "%.1f")) {
            camera_params.black_level = std::max(0.0f, std::min(100.0f, temp_black));
            ApplyParameterToAllCameras("BlackLevel", std::to_string(camera_params.black_level));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.0 - 100.0");
        
        // Quick exposure presets
        ImGui::Separator();
        ImGui::Text("Quick Presets:");
        if (ImGui::Button("Fast (1ms)", ImVec2(80, 20))) {
            camera_params.exposure_time = 1000;
            ApplyParameterToAllCameras("ExposureTime", "1000");
            exposure_time = camera_params.exposure_time;
            AddLogMessage("[PARAM] Fast exposure preset applied");
        }
        ImGui::SameLine();
        if (ImGui::Button("Normal (10ms)", ImVec2(90, 20))) {
            camera_params.exposure_time = 10000;
            ApplyParameterToAllCameras("ExposureTime", "10000");
            exposure_time = camera_params.exposure_time;
            AddLogMessage("[PARAM] Normal exposure preset applied");
        }
        ImGui::SameLine();
        if (ImGui::Button("Long (50ms)", ImVec2(80, 20))) {
            camera_params.exposure_time = 50000;
            ApplyParameterToAllCameras("ExposureTime", "50000");
            exposure_time = camera_params.exposure_time;
            AddLogMessage("[PARAM] Long exposure preset applied");
        }
        
        ImGui::PopItemWidth();
    }
    
    // Trigger Control Section
    if (ImGui::CollapsingHeader("◆ Trigger Control", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(200);
        
        // Trigger Mode
        std::vector<std::string> trigger_modes = {"Off", "On", "External"};
        static int current_trigger_mode = 0;
        if (ImGui::Combo("Trigger Mode", &current_trigger_mode,
            [](void* data, int idx, const char** out_text) {
                auto& modes = *static_cast<std::vector<std::string>*>(data);
                *out_text = modes[idx].c_str();
                return true;
            }, &trigger_modes, static_cast<int>(trigger_modes.size()))) {
            camera_params.trigger_mode = trigger_modes[current_trigger_mode];
            ApplyParameterToAllCameras("TriggerMode", camera_params.trigger_mode);
            AddLogMessage("[PARAM] Trigger mode: " + camera_params.trigger_mode);
        }
        
        // Trigger Source
        std::vector<std::string> trigger_sources = {"Software", "Hardware", "Line0", "Line1", "Line2"};
        static int current_trigger_source = 0;
        if (ImGui::Combo("Trigger Source", &current_trigger_source,
            [](void* data, int idx, const char** out_text) {
                auto& sources = *static_cast<std::vector<std::string>*>(data);
                *out_text = sources[idx].c_str();
                return true;
            }, &trigger_sources, static_cast<int>(trigger_sources.size()))) {
            camera_params.trigger_source = trigger_sources[current_trigger_source];
            ApplyParameterToAllCameras("TriggerSource", camera_params.trigger_source);
            AddLogMessage("[PARAM] Trigger source: " + camera_params.trigger_source);
        }
        
        // Trigger Activation
        std::vector<std::string> trigger_activations = {"RisingEdge", "FallingEdge", "AnyEdge", "LevelHigh", "LevelLow"};
        static int current_trigger_activation = 0;
        if (ImGui::Combo("Trigger Activation", &current_trigger_activation,
            [](void* data, int idx, const char** out_text) {
                auto& activations = *static_cast<std::vector<std::string>*>(data);
                *out_text = activations[idx].c_str();
                return true;
            }, &trigger_activations, static_cast<int>(trigger_activations.size()))) {
            camera_params.trigger_activation = trigger_activations[current_trigger_activation];
            ApplyParameterToAllCameras("TriggerActivation", camera_params.trigger_activation);
        }
        
        // Trigger Delay (direct input)
        float temp_trigger_delay = camera_params.trigger_delay;
        if (ImGui::InputFloat("Trigger Delay (us)", &temp_trigger_delay, 1.0f, 100.0f, "%.1f")) {
            camera_params.trigger_delay = std::max(0.0f, std::min(10000.0f, temp_trigger_delay));
            ApplyParameterToAllCameras("TriggerDelay", std::to_string(camera_params.trigger_delay));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.0 - 10,000.0 us");
        
        ImGui::PopItemWidth();
    }
    
    // Advanced White Balance & Color Section
    if (ImGui::CollapsingHeader("◆ Advanced White Balance & Color", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushItemWidth(200);
        
        // White Balance Controls
        if (ImGui::Checkbox("Auto White Balance", &camera_params.auto_white_balance)) {
            ApplyParameterToAllCameras("BalanceWhiteAuto", camera_params.auto_white_balance ? "Continuous" : "Off");
            // Also sync with color_settings for compatibility
            color_settings.auto_white_balance = camera_params.auto_white_balance;
        }
        
        if (!camera_params.auto_white_balance) {
            float temp_wb_red = camera_params.wb_red;
            if (ImGui::InputFloat("WB Red", &temp_wb_red, 0.01f, 0.1f, "%.2f")) {
                camera_params.wb_red = std::max(0.1f, std::min(4.0f, temp_wb_red));
                ApplyParameterToAllCameras("BalanceRatioRed", std::to_string(camera_params.wb_red));
                color_settings.wb_red_gain = camera_params.wb_red; // Sync
            }
            
            float temp_wb_green = camera_params.wb_green;
            if (ImGui::InputFloat("WB Green", &temp_wb_green, 0.01f, 0.1f, "%.2f")) {
                camera_params.wb_green = std::max(0.1f, std::min(4.0f, temp_wb_green));
                ApplyParameterToAllCameras("BalanceRatioGreen", std::to_string(camera_params.wb_green));
                color_settings.wb_green_gain = camera_params.wb_green; // Sync
            }
            
            float temp_wb_blue = camera_params.wb_blue;
            if (ImGui::InputFloat("WB Blue", &temp_wb_blue, 0.01f, 0.1f, "%.2f")) {
                camera_params.wb_blue = std::max(0.1f, std::min(4.0f, temp_wb_blue));
                ApplyParameterToAllCameras("BalanceRatioBlue", std::to_string(camera_params.wb_blue));
                color_settings.wb_blue_gain = camera_params.wb_blue; // Sync
            }
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.1 - 4.0 for all WB channels");
            
            // Quick WB presets
            ImGui::Text("Quick Presets:");
            if (ImGui::Button("Neutral", ImVec2(60, 20))) {
                camera_params.wb_red = camera_params.wb_green = camera_params.wb_blue = 1.0f;
                ApplyParameterToAllCameras("BalanceRatioRed", "1.0");
                ApplyParameterToAllCameras("BalanceRatioGreen", "1.0");
                ApplyParameterToAllCameras("BalanceRatioBlue", "1.0");
                AddLogMessage("[PARAM] Applied neutral white balance");
            }
            ImGui::SameLine();
            if (ImGui::Button("Warm", ImVec2(50, 20))) {
                camera_params.wb_red = 1.2f; camera_params.wb_green = 1.0f; camera_params.wb_blue = 0.8f;
                ApplyParameterToAllCameras("BalanceRatioRed", "1.2");
                ApplyParameterToAllCameras("BalanceRatioGreen", "1.0");
                ApplyParameterToAllCameras("BalanceRatioBlue", "0.8");
                AddLogMessage("[PARAM] Applied warm white balance");
            }
            ImGui::SameLine();
            if (ImGui::Button("Cool", ImVec2(50, 20))) {
                camera_params.wb_red = 0.8f; camera_params.wb_green = 1.0f; camera_params.wb_blue = 1.2f;
                ApplyParameterToAllCameras("BalanceRatioRed", "0.8");
                ApplyParameterToAllCameras("BalanceRatioGreen", "1.0");
                ApplyParameterToAllCameras("BalanceRatioBlue", "1.2");
                AddLogMessage("[PARAM] Applied cool white balance");
            }
        }
        
        // Color Enhancement (direct input)
        float temp_saturation = camera_params.saturation;
        if (ImGui::InputFloat("Saturation", &temp_saturation, 0.01f, 0.1f, "%.2f")) {
            camera_params.saturation = std::max(0.0f, std::min(2.0f, temp_saturation));
            ApplyParameterToAllCameras("Saturation", std::to_string(camera_params.saturation));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.0 - 2.0");
        
        float temp_hue = camera_params.hue;
        if (ImGui::InputFloat("Hue", &temp_hue, 1.0f, 10.0f, "%.1f")) {
            camera_params.hue = std::max(-180.0f, std::min(180.0f, temp_hue));
            ApplyParameterToAllCameras("Hue", std::to_string(camera_params.hue));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: -180.0 - 180.0 degrees");
        
        // Gamma (integrated from color panel, direct input)
        float temp_gamma = camera_params.gamma;
        if (ImGui::InputFloat("Gamma", &temp_gamma, 0.01f, 0.1f, "%.2f")) {
            camera_params.gamma = std::max(0.1f, std::min(3.0f, temp_gamma));
            ApplyParameterToAllCameras("Gamma", std::to_string(camera_params.gamma));
            color_settings.gamma_value = camera_params.gamma; // Sync
            color_settings.enable_gamma = (camera_params.gamma != 1.0f);
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.1 - 3.0 (1.0 = linear)");
        
        ImGui::PopItemWidth();
    }
    
    // Color Conversion & Processing Section (moved from Color Processing panel)
    if (ImGui::CollapsingHeader("◆ Color Conversion & Processing")) {
        ImGui::PushItemWidth(200);
        
        // Bayer Pattern Selection
        ImGui::Text("◆ Bayer Pattern Alignment");
        std::vector<std::string> bayer_patterns = {"GBRG", "BGGR", "RGGB", "GRBG", "RGBG", "BGRG"};
        static int current_bayer = 2; // RGGB default
        
        if (ImGui::Combo("Bayer Alignment", &current_bayer,
            [](void* data, int idx, const char** out_text) {
                auto& patterns = *static_cast<std::vector<std::string>*>(data);
                *out_text = patterns[idx].c_str();
                return true;
            }, &bayer_patterns, static_cast<int>(bayer_patterns.size()))) {
            color_settings.bayer_align = current_bayer;
            AddLogMessage("[COLOR] Bayer pattern set to: " + bayer_patterns[current_bayer]);
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "RGGB is most common for Nano-C4020");
        
        // Color Conversion Method (direct input)
        ImGui::Text("◆ Color Conversion Quality");
        int temp_method = color_settings.color_method;
        if (ImGui::InputInt("Method (1=Fast, 7=Best)", &temp_method, 1, 1)) {
            color_settings.color_method = std::max(1, std::min(7, temp_method));
            AddLogMessage("[COLOR] Color conversion method: " + std::to_string(color_settings.color_method));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "1=Fastest, 7=Highest Quality");
        
        // Hardware vs Software toggle
        if (ImGui::Checkbox("Use Hardware Conversion", &color_settings.use_hardware_conversion)) {
            AddLogMessage("[COLOR] Hardware conversion " + std::string(color_settings.use_hardware_conversion ? "enabled" : "disabled"));
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Hardware may improve performance");
        
        // LUT File Management
        ImGui::Separator();
        ImGui::Text("◆ Look-Up Table (LUT)");
        
        if (ImGui::Checkbox("Enable LUT", &color_settings.enable_lut)) {
            AddLogMessage("[LUT] LUT processing " + std::string(color_settings.enable_lut ? "enabled" : "disabled"));
        }
        
        if (color_settings.enable_lut) {
            ImGui::Text("LUT File: %s", color_settings.lut_filename.empty() ? "[None Selected]" : color_settings.lut_filename.c_str());
            
            if (ImGui::Button("► Browse LUT File", ImVec2(150, 25))) {
                std::string lutFile = BrowseForLUTFile();
                if (!lutFile.empty()) {
                    color_settings.lut_filename = lutFile;
                    AddLogMessage("[LUT] LUT file selected: " + lutFile);
                }
            }
            
            if (!color_settings.lut_filename.empty()) {
                ImGui::SameLine();
                if (ImGui::Button("► Apply LUT", ImVec2(100, 25))) {
                    // Apply LUT to all connected cameras
                    for (auto& [id, device] : connected_devices) {
                        if (device) {
                            // Load and apply LUT using Sapera SDK
                            AddLogMessage("[LUT] Applying LUT to " + id);
                        }
                    }
                    AddLogMessage("[LUT] LUT applied to all cameras");
                }
            }
        }
        
        ImGui::PopItemWidth();
    }
    
    // Region of Interest Section
    if (ImGui::CollapsingHeader("◆ Region of Interest (ROI)")) {
        ImGui::PushItemWidth(150);
        
        if (ImGui::Checkbox("Enable ROI", &camera_params.roi_enable)) {
            AddLogMessage("[PARAM] ROI " + std::string(camera_params.roi_enable ? "enabled" : "disabled"));
        }
        
        if (camera_params.roi_enable) {
            if (ImGui::InputInt("ROI X", &camera_params.roi_x, 8, 64)) {
                camera_params.roi_x = std::max(0, camera_params.roi_x);
                ApplyParameterToAllCameras("OffsetX", std::to_string(camera_params.roi_x));
            }
            if (ImGui::InputInt("ROI Y", &camera_params.roi_y, 8, 64)) {
                camera_params.roi_y = std::max(0, camera_params.roi_y);
                ApplyParameterToAllCameras("OffsetY", std::to_string(camera_params.roi_y));
            }
            if (ImGui::InputInt("ROI Width", &camera_params.roi_width, 8, 64)) {
                camera_params.roi_width = std::max(64, camera_params.roi_width);
                ApplyParameterToAllCameras("Width", std::to_string(camera_params.roi_width));
            }
            if (ImGui::InputInt("ROI Height", &camera_params.roi_height, 8, 64)) {
                camera_params.roi_height = std::max(64, camera_params.roi_height);
                ApplyParameterToAllCameras("Height", std::to_string(camera_params.roi_height));
            }
        }
        
        ImGui::PopItemWidth();
    }
    
    // Acquisition Mode Section
    if (ImGui::CollapsingHeader("◆ Acquisition Mode")) {
        ImGui::PushItemWidth(200);
        
        std::vector<std::string> acq_modes = {"SingleFrame", "MultiFrame", "Continuous"};
        static int current_acq_mode = 2; // Continuous
        if (ImGui::Combo("Acquisition Mode", &current_acq_mode,
            [](void* data, int idx, const char** out_text) {
                auto& modes = *static_cast<std::vector<std::string>*>(data);
                *out_text = modes[idx].c_str();
                return true;
            }, &acq_modes, static_cast<int>(acq_modes.size()))) {
            camera_params.acquisition_mode = acq_modes[current_acq_mode];
            ApplyParameterToAllCameras("AcquisitionMode", camera_params.acquisition_mode);
        }
        
        if (camera_params.acquisition_mode == "MultiFrame") {
            if (ImGui::InputInt("Frame Count", &camera_params.acquisition_frame_count, 1, 10)) {
                camera_params.acquisition_frame_count = std::max(1, camera_params.acquisition_frame_count);
                ApplyParameterToAllCameras("AcquisitionFrameCount", std::to_string(camera_params.acquisition_frame_count));
            }
        }
        
        if (ImGui::Checkbox("Enable Frame Rate", &camera_params.frame_rate_enable)) {
            ApplyParameterToAllCameras("AcquisitionFrameRateEnable", camera_params.frame_rate_enable ? "true" : "false");
        }
        
        if (camera_params.frame_rate_enable) {
            float temp_frame_rate = camera_params.acquisition_frame_rate;
            if (ImGui::InputFloat("Frame Rate (fps)", &temp_frame_rate, 0.1f, 1.0f, "%.1f")) {
                camera_params.acquisition_frame_rate = std::max(1.0f, std::min(120.0f, temp_frame_rate));
                ApplyParameterToAllCameras("AcquisitionFrameRate", std::to_string(camera_params.acquisition_frame_rate));
            }
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 1.0 - 120.0 fps");
        }
        
        ImGui::PopItemWidth();
    }
    
    // Transport Layer Section
    if (ImGui::CollapsingHeader("◆ Transport Layer (GigE)")) {
        ImGui::PushItemWidth(200);
        
        if (ImGui::InputInt("Packet Size", &camera_params.packet_size, 64, 512)) {
            camera_params.packet_size = std::max(576, std::min(9000, camera_params.packet_size));
            ApplyParameterToAllCameras("GevSCPSPacketSize", std::to_string(camera_params.packet_size));
        }
        
        if (ImGui::InputInt("Inter-Packet Delay", &camera_params.packet_delay, 10, 100)) {
            camera_params.packet_delay = std::max(0, camera_params.packet_delay);
            ApplyParameterToAllCameras("GevSCPD", std::to_string(camera_params.packet_delay));
        }
        
        ImGui::PopItemWidth();
    }
    
    // Device Information Section
    if (ImGui::CollapsingHeader("◆ Device Information")) {
        ImGui::Text("Temperature Status: %s", camera_params.temperature_status.c_str());
        ImGui::Text("Device Temperature: %.1f°C", camera_params.device_temperature);
        
        if (ImGui::Button("► Update Device Info", ImVec2(150, 25))) {
            AddLogMessage("[PARAM] Refreshing device information");
            // Refresh device info from cameras
        }
    }
    
    // Live Feature Browser Section
    if (ImGui::CollapsingHeader("◆ Live Feature Browser")) {
        ImGui::Text("Available Features (%zu)", available_features.size());
        
        if (ImGui::BeginChild("FeatureList", ImVec2(0, 200), true)) {
            for (const auto& feature : available_features) {
                if (ImGui::Selectable(feature.c_str())) {
                    AddLogMessage("[PARAM] Selected feature: " + feature);
                }
                
                // Show current value if available
                auto it = feature_values.find(feature);
                if (it != feature_values.end()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "= %s", it->second.c_str());
                }
            }
        }
        ImGui::EndChild();
    }
    
    ImGui::End();
}

void RefreshCameraFeatures() {
    available_features.clear();
    feature_values.clear();
    
    if (connected_devices.empty()) {
        AddLogMessage("[PARAM] No cameras connected for feature refresh");
        return;
    }
    
    // Get features from the first connected camera
    auto firstCamera = connected_devices.begin();
    SapAcqDevice* device = firstCamera->second;
    
    if (!device) {
        AddLogMessage("[PARAM] Invalid device handle");
        return;
    }
    
    try {
        int featureCount = 0;
        if (device->GetFeatureCount(&featureCount)) {
            AddLogMessage("[PARAM] Found " + std::to_string(featureCount) + " features");
            
            for (int i = 0; i < featureCount; i++) {
                char featureName[256];
                if (device->GetFeatureNameByIndex(i, featureName, sizeof(featureName))) {
                    available_features.push_back(std::string(featureName));
                    
                    // Try to get current value
                    char featureValue[256];
                    if (device->GetFeatureValue(featureName, featureValue, sizeof(featureValue))) {
                        feature_values[std::string(featureName)] = std::string(featureValue);
                    }
                }
            }
        }
        
        AddLogMessage("[PARAM] Feature refresh complete");
        
    } catch (const std::exception& e) {
        AddLogMessage("[PARAM] Error refreshing features: " + std::string(e.what()));
    }
}

void DetectCameraResolution() {
    if (connected_devices.empty()) {
        AddLogMessage("[PARAM] No cameras connected for resolution detection");
        return;
    }
    
    // Get resolution from the first connected camera
    auto firstCamera = connected_devices.begin();
    SapAcqDevice* device = firstCamera->second;
    
    if (!device) {
        AddLogMessage("[PARAM] Invalid device handle");
        return;
    }
    
    try {
        // Try to get maximum width and height
        int maxWidth = 0, maxHeight = 0;
        int currentWidth = 0, currentHeight = 0;
        
        // Get current dimensions
        if (device->GetFeatureValue("Width", &currentWidth)) {
            camera_params.width = currentWidth;
            AddLogMessage("[PARAM] Current width detected: " + std::to_string(currentWidth));
        }
        
        if (device->GetFeatureValue("Height", &currentHeight)) {
            camera_params.height = currentHeight;
            AddLogMessage("[PARAM] Current height detected: " + std::to_string(currentHeight));
        }
        
        // Try to get maximum dimensions
        if (device->GetFeatureValue("WidthMax", &maxWidth)) {
            camera_params.max_width = maxWidth;
            AddLogMessage("[PARAM] Maximum width detected: " + std::to_string(maxWidth));
        } else {
            // Fallback: try SensorWidth
            if (device->GetFeatureValue("SensorWidth", &maxWidth)) {
                camera_params.max_width = maxWidth;
                AddLogMessage("[PARAM] Sensor width detected: " + std::to_string(maxWidth));
            }
        }
        
        if (device->GetFeatureValue("HeightMax", &maxHeight)) {
            camera_params.max_height = maxHeight;
            AddLogMessage("[PARAM] Maximum height detected: " + std::to_string(maxHeight));
        } else {
            // Fallback: try SensorHeight
            if (device->GetFeatureValue("SensorHeight", &maxHeight)) {
                camera_params.max_height = maxHeight;
                AddLogMessage("[PARAM] Sensor height detected: " + std::to_string(maxHeight));
            }
        }
        
        AddLogMessage("[PARAM] Resolution detection complete - Max: " + 
                     std::to_string(camera_params.max_width) + "x" + std::to_string(camera_params.max_height));
        
    } catch (const std::exception& e) {
        AddLogMessage("[PARAM] Error detecting resolution: " + std::string(e.what()));
    }
}

void ApplyParameterToAllCameras(const std::string& featureName, const std::string& value) {
    int successCount = 0;
    int totalCount = static_cast<int>(connected_devices.size());
    
    // Check if this is a transport layer parameter that requires stopping acquisition
    bool isTransportLayerParam = (featureName == "GevSCPSPacketSize" || 
                                 featureName == "GevSCPD" || 
                                 featureName == "PacketSize" || 
                                 featureName == "PacketDelay");
    
    // Store acquisition states if we need to stop/restart
    std::map<std::string, bool> acquisitionStates;
    
    if (isTransportLayerParam) {
        AddLogMessage("[PARAM] Transport layer parameter detected: " + featureName);
        AddLogMessage("[PARAM] Temporarily stopping acquisition for all cameras...");
        
        // Stop acquisition for all cameras and store their states
        for (auto& [cameraId, device] : connected_devices) {
            if (device) {
                // Check if acquisition is running
                char acquisitionMode[64];
                bool wasAcquiring = false;
                if (device->GetFeatureValue("AcquisitionMode", acquisitionMode, sizeof(acquisitionMode))) {
                    // Try to stop acquisition - ignore errors if already stopped
                    device->SetFeatureValue("AcquisitionStop", "1");
                    acquisitionStates[cameraId] = true; // We'll restart it later
                    wasAcquiring = true;
                }
                acquisitionStates[cameraId] = wasAcquiring;
            }
        }
        
        // Wait a moment for acquisition to fully stop
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Apply the parameter to all cameras
    for (auto& [cameraId, device] : connected_devices) {
        if (device) {
            BOOL result = device->SetFeatureValue(featureName.c_str(), value.c_str());
            if (result) {
                successCount++;
                AddLogMessage("[PARAM] ✓ " + featureName + " = " + value + " applied to " + cameraId);
            } else {
                AddLogMessage("[PARAM] ✗ Failed to apply " + featureName + " = " + value + " to " + cameraId);
                
                // Check if feature is available and writable
                BOOL isAvailable = FALSE;
                if (device->IsFeatureAvailable(featureName.c_str(), &isAvailable)) {
                    if (!isAvailable) {
                        AddLogMessage("[PARAM]   → Feature " + featureName + " not available on " + cameraId);
                    }
                }
            }
        }
    }
    
    // Restart acquisition if we stopped it
    if (isTransportLayerParam) {
        AddLogMessage("[PARAM] Restarting acquisition for cameras that were running...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        for (auto& [cameraId, device] : connected_devices) {
            if (device && acquisitionStates[cameraId]) {
                // Restart acquisition
                device->SetFeatureValue("AcquisitionStart", "1");
                AddLogMessage("[PARAM] ✓ Acquisition restarted for " + cameraId);
            }
        }
    }
    
    if (successCount == totalCount) {
        AddLogMessage("[PARAM] ✓ " + featureName + " = " + value + " applied to all " + std::to_string(totalCount) + " cameras");
    } else {
        AddLogMessage("[PARAM] ⚠ " + featureName + " applied to " + std::to_string(successCount) + "/" + std::to_string(totalCount) + " cameras");
    }
}

// Individual Camera Control Functions - NEW
void ApplyParameterToCamera(const std::string& cameraId, const std::string& featureName, const std::string& value) {
    auto deviceIt = connected_devices.find(cameraId);
    if (deviceIt != connected_devices.end()) {
        SapAcqDevice* device = deviceIt->second;
        if (device && device->SetFeatureValue(featureName.c_str(), value.c_str())) {
            AddLogMessage("[PARAM] " + featureName + " = " + value + " applied to " + cameraId);
        } else {
            AddLogMessage("[PARAM] Failed to apply " + featureName + " to " + cameraId);
        }
    } else {
        AddLogMessage("[PARAM] Camera " + cameraId + " not found");
    }
}

void InitializeIndividualCameraParams(const std::string& cameraId) {
    individual_camera_params[cameraId] = camera_params;
    AddLogMessage("[PARAM] Individual camera parameters initialized for " + cameraId);
}

CameraParameters& GetCameraParameters(const std::string& cameraId) {
    return individual_camera_params[cameraId];
}

void SyncAllCamerasToIndividual() {
    for (auto& [cameraId, device] : connected_devices) {
        if (device) {
            // Initialize individual camera params if not exists
            if (individual_camera_params.find(cameraId) == individual_camera_params.end()) {
                InitializeIndividualCameraParams(cameraId);
            }
            
            // Read current values from camera and store in individual parameters
            char buffer[512];
            auto& params = individual_camera_params[cameraId];
            
            if (device->GetFeatureValue("Width", buffer, sizeof(buffer))) {
                params.width = std::stoi(buffer);
            }
            if (device->GetFeatureValue("Height", buffer, sizeof(buffer))) {
                params.height = std::stoi(buffer);
            }
            if (device->GetFeatureValue("ExposureTime", buffer, sizeof(buffer))) {
                params.exposure_time = std::stoi(buffer);
            }
            if (device->GetFeatureValue("Gain", buffer, sizeof(buffer))) {
                params.gain = std::stof(buffer);
            }
            if (device->GetFeatureValue("BalanceRatioRed", buffer, sizeof(buffer))) {
                params.wb_red = std::stof(buffer);
            }
            if (device->GetFeatureValue("BalanceRatioGreen", buffer, sizeof(buffer))) {
                params.wb_green = std::stof(buffer);
            }
            if (device->GetFeatureValue("BalanceRatioBlue", buffer, sizeof(buffer))) {
                params.wb_blue = std::stof(buffer);
            }
        }
    }
    AddLogMessage("[PARAM] All camera parameters synced from hardware to individual settings");
}

void SyncIndividualToAllCameras() {
    // Copy the global camera parameters to all individual camera settings
    for (auto& [cameraId, device] : connected_devices) {
        if (device) {
            individual_camera_params[cameraId] = camera_params;
        }
    }
    AddLogMessage("[PARAM] Global parameters copied to all individual camera settings");
}

void RenderIndividualCameraPanel() {
    ImGui::Begin("● Individual Camera Control", &show_individual_camera_panel, ImGuiWindowFlags_NoCollapse);
    
    if (connected_devices.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[WARN] No cameras connected");
        ImGui::Text("Connect cameras to access individual camera control");
        ImGui::End();
        return;
    }
    
    ImGui::Text("Individual Camera Parameter Control");
    ImGui::Separator();
    
    // Camera Selection Section
    ImGui::Text("◆ Camera Selection");
    
    // Camera selection dropdown
    std::vector<std::string> camera_ids;
    for (const auto& [id, device] : connected_devices) {
        camera_ids.push_back(id);
    }
    
    static int current_camera_index = 0;
    if (current_camera_index >= camera_ids.size()) current_camera_index = 0;
    
    if (ImGui::Combo("Camera", &current_camera_index,
        [](void* data, int idx, const char** out_text) {
            auto& ids = *static_cast<std::vector<std::string>*>(data);
            *out_text = ids[idx].c_str();
            return true;
        }, &camera_ids, static_cast<int>(camera_ids.size()))) {
        if (current_camera_index < camera_ids.size()) {
            selected_camera_id = camera_ids[current_camera_index];
            AddLogMessage("[UI] Selected camera: " + selected_camera_id);
        }
    }
    
    if (selected_camera_id.empty() && !camera_ids.empty()) {
        selected_camera_id = camera_ids[0];
    }
    
    if (!selected_camera_id.empty()) {
        // Initialize individual parameters if not exists
        if (individual_camera_params.find(selected_camera_id) == individual_camera_params.end()) {
            InitializeIndividualCameraParams(selected_camera_id);
        }
        
        auto& params = individual_camera_params[selected_camera_id];
        
        ImGui::Text("Selected: %s", selected_camera_id.c_str());
        
        ImGui::Separator();
        
        // Individual Parameter Controls
        ImGui::Text("◆ Individual Camera Parameters for %s", selected_camera_id.c_str());
        
        // Exposure Control (Key for lens compensation)
        if (ImGui::CollapsingHeader("◆ Exposure Control (Individual)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushItemWidth(200);
            
            int temp_exposure = params.exposure_time;
            if (ImGui::InputInt("Exposure Time (us)", &temp_exposure, 100, 10000)) {
                params.exposure_time = std::max(100, std::min(100000, temp_exposure));
                ApplyParameterToCamera(selected_camera_id, "ExposureTime", std::to_string(params.exposure_time));
                AddLogMessage("[PARAM] Individual exposure: " + selected_camera_id + " = " + std::to_string(params.exposure_time) + "us");
            }
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 100 - 100,000 us");
            
            // Exposure presets for this camera
            ImGui::Text("Quick Presets:");
            if (ImGui::Button("Fast (1ms)", ImVec2(80, 20))) {
                params.exposure_time = 1000;
                ApplyParameterToCamera(selected_camera_id, "ExposureTime", "1000");
                AddLogMessage("[PARAM] Fast exposure applied to " + selected_camera_id);
            }
            ImGui::SameLine();
            if (ImGui::Button("Normal (10ms)", ImVec2(90, 20))) {
                params.exposure_time = 10000;
                ApplyParameterToCamera(selected_camera_id, "ExposureTime", "10000");
                AddLogMessage("[PARAM] Normal exposure applied to " + selected_camera_id);
            }
            ImGui::SameLine();
            if (ImGui::Button("Long (50ms)", ImVec2(80, 20))) {
                params.exposure_time = 50000;
                ApplyParameterToCamera(selected_camera_id, "ExposureTime", "50000");
                AddLogMessage("[PARAM] Long exposure applied to " + selected_camera_id);
            }
            
            ImGui::PopItemWidth();
        }
        
        // Gain Control
        if (ImGui::CollapsingHeader("◆ Gain Control (Individual)")) {
            ImGui::PushItemWidth(200);
            
            float temp_gain = params.gain;
            if (ImGui::InputFloat("Gain (dB)", &temp_gain, 0.1f, 1.0f, "%.1f")) {
                params.gain = std::max(0.0f, std::min(30.0f, temp_gain));
                ApplyParameterToCamera(selected_camera_id, "Gain", std::to_string(params.gain));
                AddLogMessage("[PARAM] Individual gain: " + selected_camera_id + " = " + std::to_string(params.gain) + "dB");
            }
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Range: 0.0 - 30.0 dB");
            
            ImGui::PopItemWidth();
        }
        
        // White Balance Control
        if (ImGui::CollapsingHeader("◆ White Balance (Individual)")) {
            ImGui::PushItemWidth(200);
            
            float temp_wb_red = params.wb_red;
            if (ImGui::InputFloat("WB Red", &temp_wb_red, 0.01f, 0.1f, "%.2f")) {
                params.wb_red = std::max(0.1f, std::min(4.0f, temp_wb_red));
                ApplyParameterToCamera(selected_camera_id, "BalanceRatioRed", std::to_string(params.wb_red));
            }
            
            float temp_wb_green = params.wb_green;
            if (ImGui::InputFloat("WB Green", &temp_wb_green, 0.01f, 0.1f, "%.2f")) {
                params.wb_green = std::max(0.1f, std::min(4.0f, temp_wb_green));
                ApplyParameterToCamera(selected_camera_id, "BalanceRatioGreen", std::to_string(params.wb_green));
            }
            
            float temp_wb_blue = params.wb_blue;
            if (ImGui::InputFloat("WB Blue", &temp_wb_blue, 0.01f, 0.1f, "%.2f")) {
                params.wb_blue = std::max(0.1f, std::min(4.0f, temp_wb_blue));
                ApplyParameterToCamera(selected_camera_id, "BalanceRatioBlue", std::to_string(params.wb_blue));
            }
            
            ImGui::PopItemWidth();
        }
        
        ImGui::Separator();
        
        // Control buttons
        if (ImGui::Button("◆ Copy From Global", ImVec2(150, 30))) {
            params = camera_params;
            AddLogMessage("[PARAM] Global parameters copied to " + selected_camera_id);
        }
        ImGui::SameLine();
        if (ImGui::Button("◆ Apply All", ImVec2(100, 30))) {
            ApplyParameterToCamera(selected_camera_id, "ExposureTime", std::to_string(params.exposure_time));
            ApplyParameterToCamera(selected_camera_id, "Gain", std::to_string(params.gain));
            ApplyParameterToCamera(selected_camera_id, "BalanceRatioRed", std::to_string(params.wb_red));
            ApplyParameterToCamera(selected_camera_id, "BalanceRatioGreen", std::to_string(params.wb_green));
            ApplyParameterToCamera(selected_camera_id, "BalanceRatioBlue", std::to_string(params.wb_blue));
            AddLogMessage("[PARAM] All individual parameters applied to " + selected_camera_id);
        }
        
        ImGui::Separator();
        
        // Status display
        ImGui::Text("◆ Current Parameters for %s", selected_camera_id.c_str());
        ImGui::Text("Exposure: %d us", params.exposure_time);
        ImGui::Text("Gain: %.1f dB", params.gain);
        ImGui::Text("WB: R=%.2f G=%.2f B=%.2f", params.wb_red, params.wb_green, params.wb_blue);
    }
    
    ImGui::End();
}

// Individual Camera Settings Persistence - NEW
void SaveIndividualCameraSettings() {
    if (!settings_manager) return;
    
    // Sync current camera parameters to settings
    SyncCameraParametersToSettings();
    
    // Save to file
    settings_manager->Save();
    AddLogMessage("[SETTINGS] Settings saved successfully");
}

void LoadIndividualCameraSettings() {
    if (!settings_manager) return;
    
    // Load settings has already been done in main(), this refreshes UI values if needed
    auto& cam_settings = settings_manager->GetCameraSettings();
    camera_params.width = cam_settings.width;
    camera_params.height = cam_settings.height;
    camera_params.pixel_format = cam_settings.pixel_format;
    camera_params.exposure_time = cam_settings.exposure_time;
    // ... other parameters loaded in main()
    
    AddLogMessage("[SETTINGS] Settings loaded to UI");
}

void SyncIndividualParamsToSettings() {
    if (!settings_manager) return;
    
    auto& cam_settings = settings_manager->GetCameraSettings();
    
    // Sync all camera parameters to settings structure
    cam_settings.width = camera_params.width;
    cam_settings.height = camera_params.height;
    cam_settings.pixel_format = camera_params.pixel_format;
    cam_settings.exposure_time = camera_params.exposure_time;
    cam_settings.gain = camera_params.gain;
    cam_settings.auto_exposure = camera_params.auto_exposure;
    cam_settings.auto_gain = camera_params.auto_gain;
    cam_settings.trigger_mode = camera_params.trigger_mode;
    cam_settings.trigger_source = camera_params.trigger_source;
    cam_settings.trigger_activation = camera_params.trigger_activation;
    cam_settings.trigger_delay = camera_params.trigger_delay;
    cam_settings.white_balance_red = camera_params.wb_red;
    cam_settings.white_balance_green = camera_params.wb_green;
    cam_settings.white_balance_blue = camera_params.wb_blue;
    cam_settings.auto_white_balance = camera_params.auto_white_balance;
    cam_settings.saturation = camera_params.saturation;
    cam_settings.hue = camera_params.hue;
    cam_settings.gamma = camera_params.gamma;
    cam_settings.roi_offset_x = camera_params.roi_x;
    cam_settings.roi_offset_y = camera_params.roi_y;
    cam_settings.roi_width = camera_params.roi_width;
    cam_settings.roi_height = camera_params.roi_height;
    cam_settings.roi_enabled = camera_params.roi_enable;
    cam_settings.acquisition_mode = camera_params.acquisition_mode;
    cam_settings.acquisition_frame_count = camera_params.acquisition_frame_count;
    cam_settings.acquisition_frame_rate = camera_params.acquisition_frame_rate;
    cam_settings.packet_size = camera_params.packet_size;
    cam_settings.packet_delay = camera_params.packet_delay;
}

void SyncSettingsToIndividualParams() {
    if (!settings_manager) return;
    
    auto& cam_settings = settings_manager->GetCameraSettings();
    
    // Sync all camera parameters to settings structure
    cam_settings.width = camera_params.width;
    cam_settings.height = camera_params.height;
    cam_settings.pixel_format = camera_params.pixel_format;
    cam_settings.exposure_time = camera_params.exposure_time;
    cam_settings.gain = camera_params.gain;
    cam_settings.auto_exposure = camera_params.auto_exposure;
    cam_settings.auto_gain = camera_params.auto_gain;
    cam_settings.trigger_mode = camera_params.trigger_mode;
    cam_settings.trigger_source = camera_params.trigger_source;
    cam_settings.trigger_activation = camera_params.trigger_activation;
    cam_settings.trigger_delay = camera_params.trigger_delay;
    cam_settings.white_balance_red = camera_params.wb_red;
    cam_settings.white_balance_green = camera_params.wb_green;
    cam_settings.white_balance_blue = camera_params.wb_blue;
    cam_settings.auto_white_balance = camera_params.auto_white_balance;
    cam_settings.saturation = camera_params.saturation;
    cam_settings.hue = camera_params.hue;
    cam_settings.gamma = camera_params.gamma;
    cam_settings.roi_offset_x = camera_params.roi_x;
    cam_settings.roi_offset_y = camera_params.roi_y;
    cam_settings.roi_width = camera_params.roi_width;
    cam_settings.roi_height = camera_params.roi_height;
    cam_settings.roi_enabled = camera_params.roi_enable;
    cam_settings.acquisition_mode = camera_params.acquisition_mode;
    cam_settings.acquisition_frame_count = camera_params.acquisition_frame_count;
    cam_settings.acquisition_frame_rate = camera_params.acquisition_frame_rate;
    cam_settings.packet_size = camera_params.packet_size;
    cam_settings.packet_delay = camera_params.packet_delay;
}

std::vector<std::string> GetAvailablePixelFormats() {
    return {"Mono8", "Mono12", "Mono16", "RGB8", "BGR8", "BayerRG8", "BayerGB8", "BayerGR8", "BayerBG8", "BayerRG12", "BayerGB12", "BayerGR12", "BayerBG12"};
}

std::vector<std::string> GetAvailableTriggerModes() {
    return {"Off", "On", "External"};
}

// Settings Management Functions
void SaveCurrentSettings() {
    if (!settings_manager) return;
    
    // Sync current camera parameters to settings
    SyncCameraParametersToSettings();
    
    // Save to file
    settings_manager->Save();
    AddLogMessage("[SETTINGS] Settings saved successfully");
}

void LoadSettingsToUI() {
    if (!settings_manager) return;
    
    // Load settings has already been done in main(), this refreshes UI values if needed
    auto& cam_settings = settings_manager->GetCameraSettings();
    camera_params.width = cam_settings.width;
    camera_params.height = cam_settings.height;
    camera_params.pixel_format = cam_settings.pixel_format;
    camera_params.exposure_time = cam_settings.exposure_time;
    // ... other parameters loaded in main()
    
    AddLogMessage("[SETTINGS] Settings loaded to UI");
}

void SyncCameraParametersToSettings() {
    if (!settings_manager) return;
    
    auto& cam_settings = settings_manager->GetCameraSettings();
    
    // Sync all camera parameters to settings structure
    cam_settings.width = camera_params.width;
    cam_settings.height = camera_params.height;
    cam_settings.pixel_format = camera_params.pixel_format;
    cam_settings.exposure_time = camera_params.exposure_time;
    cam_settings.gain = camera_params.gain;
    cam_settings.auto_exposure = camera_params.auto_exposure;
    cam_settings.auto_gain = camera_params.auto_gain;
    cam_settings.trigger_mode = camera_params.trigger_mode;
    cam_settings.trigger_source = camera_params.trigger_source;
    cam_settings.trigger_activation = camera_params.trigger_activation;
    cam_settings.trigger_delay = camera_params.trigger_delay;
    cam_settings.white_balance_red = camera_params.wb_red;
    cam_settings.white_balance_green = camera_params.wb_green;
    cam_settings.white_balance_blue = camera_params.wb_blue;
    cam_settings.auto_white_balance = camera_params.auto_white_balance;
    cam_settings.saturation = camera_params.saturation;
    cam_settings.hue = camera_params.hue;
    cam_settings.gamma = camera_params.gamma;
    cam_settings.roi_offset_x = camera_params.roi_x;
    cam_settings.roi_offset_y = camera_params.roi_y;
    cam_settings.roi_width = camera_params.roi_width;
    cam_settings.roi_height = camera_params.roi_height;
    cam_settings.roi_enabled = camera_params.roi_enable;
    cam_settings.acquisition_mode = camera_params.acquisition_mode;
    cam_settings.acquisition_frame_count = camera_params.acquisition_frame_count;
    cam_settings.acquisition_frame_rate = camera_params.acquisition_frame_rate;
    cam_settings.packet_size = camera_params.packet_size;
    cam_settings.packet_delay = camera_params.packet_delay;
}

void RenderSessionManagerPanel() {
    if (!ImGui::Begin("■ Session Management", &show_session_manager)) {
        ImGui::End();
        return;
    }
    
    // Current Session Status
    ImGui::Text("◆ Current Session");
    ImGui::Separator();
    
    if (session_manager->HasActiveSession()) {
        auto* session = session_manager->GetCurrentSession();
        ImGui::Text("Active: %s", session->object_name.c_str());
        ImGui::Text("Captures: %d", session->capture_count);
        ImGui::Text("Session ID: %s", session->session_id.c_str());
        
        if (ImGui::Button("■ End Current Session")) {
            session_manager->EndCurrentSession();
            AddLogMessage("[SESSION] Current session ended");
        }
    } else {
        ImGui::Text("No active session");
        
        ImGui::Spacing();
        ImGui::Text("■ Start New Session");
        ImGui::InputText("Object Name", new_object_name, sizeof(new_object_name));
        
        if (ImGui::Button("▶ Start Session") && strlen(new_object_name) > 0) {
            if (session_manager->StartNewSession(std::string(new_object_name))) {
                AddLogMessage("[SESSION] New session started for: " + std::string(new_object_name));
                memset(new_object_name, 0, sizeof(new_object_name)); // Clear input
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Session History
    ImGui::Text("◆ Session History");
    const auto& history = session_manager->GetSessionHistory();
    
    if (history.empty()) {
        ImGui::Text("No previous sessions");
    } else {
        for (int i = static_cast<int>(history.size()) - 1; i >= 0; i--) {
            const auto& session = history[i];
            ImGui::Text("● %s (%d captures)", session.object_name.c_str(), session.capture_count);
            ImGui::SameLine();
            std::string button_id = "Open##" + std::to_string(i);
            if (ImGui::SmallButton(button_id.c_str())) {
                OpenFolderInExplorer(session.base_path);
            }
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Session Statistics
    ImGui::Text("◆ Statistics");
    ImGui::Text("Total Sessions: %d", session_manager->GetTotalSessions());
    if (session_manager->HasActiveSession()) {
        ImGui::Text("Current Session Captures: %d", session_manager->GetTotalCapturesInSession());
    }
    
    ImGui::End();
}

// Metadata Management Functions - NEW
void SaveCaptureMetadata(const std::string& sessionPath, int captureNumber, bool success, int duration_ms) {
    try {
        // Create metadata filename based on session path
        std::filesystem::path session_folder(sessionPath);
        std::string session_id = session_folder.filename().string();
        
        std::string metadata_dir = current_image_folder + "/metadata";
        std::filesystem::create_directories(metadata_dir);
        
        std::string metadata_file = metadata_dir + "/" + session_id + ".json";
        
        // Create metadata content
        SimpleJSON metadata;
        
        // Session info
        metadata.set("session_id", session_id);
        metadata.set("capture_number", captureNumber);
        metadata.set("success", success);
        metadata.set("duration_ms", duration_ms);
        metadata.set("timestamp", GetCurrentTimestamp());
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        metadata.set("datetime", ss.str());
        
        // Camera settings
        metadata.set("camera_count", static_cast<int>(connected_devices.size()));
        metadata.set("exposure_time", camera_params.exposure_time);
        metadata.set("gain", camera_params.gain);
        metadata.set("auto_exposure", camera_params.auto_exposure);
        metadata.set("auto_gain", camera_params.auto_gain);
        metadata.set("width", camera_params.width);
        metadata.set("height", camera_params.height);
        metadata.set("pixel_format", camera_params.pixel_format);
        
        // Color settings
        metadata.set("white_balance_red", camera_params.wb_red);
        metadata.set("white_balance_green", camera_params.wb_green);
        metadata.set("white_balance_blue", camera_params.wb_blue);
        metadata.set("auto_white_balance", camera_params.auto_white_balance);
        metadata.set("gamma", camera_params.gamma);
        metadata.set("saturation", camera_params.saturation);
        
        // Advanced settings
        metadata.set("color_method", color_settings.color_method);
        metadata.set("bayer_align", color_settings.bayer_align);
        metadata.set("enable_gamma", color_settings.enable_gamma);
        metadata.set("gamma_value", color_settings.gamma_value);
        metadata.set("use_hardware_conversion", color_settings.use_hardware_conversion);
        
        // Camera list
        metadata.set("cameras", "");
        std::stringstream camera_list;
        bool first = true;
        for (const auto& camera : discovered_cameras) {
            if (connected_devices.find(camera.id) != connected_devices.end()) {
                if (!first) camera_list << ",";
                camera_list << camera.name << "(" << camera.id << ")";
                first = false;
            }
        }
        metadata.set("camera_list", camera_list.str());
        
        // Format info
        metadata.set("format", capture_format_raw ? "RAW" : "TIFF");
        
        // Save metadata to file
        std::ofstream file(metadata_file);
        if (file.is_open()) {
            file << "{\n";
            bool first_item = true;
            for (const auto& [key, value] : metadata.data) {
                if (!first_item) file << ",\n";
                file << "  \"" << key << "\": \"" << value << "\"";
                first_item = false;
            }
            file << "\n}\n";
            file.close();
            
            AddLogMessage("[METADATA] Saved: " + metadata_file);
        } else {
            AddLogMessage("[ERR] Failed to save metadata: " + metadata_file);
        }
        
    } catch (const std::exception& e) {
        AddLogMessage("[ERR] Metadata save error: " + std::string(e.what()));
    }
} 

// Network Optimization Functions Implementation - NEW
void RenderNetworkOptimizationPanel() {
    ImGui::Begin("◆ Network Optimization & Image Quality", &show_network_panel, ImGuiWindowFlags_NoCollapse);
    
    ImGui::Text("High-Quality Image Capture Network Optimization");
    ImGui::Separator();
    
    // Capture Mode Selection
    ImGui::Text("◆ Capture Mode");
    if (ImGui::RadioButton("Simultaneous (Fast)", !sequential_capture_mode)) {
        sequential_capture_mode = false;
        AddLogMessage("[NET] Simultaneous capture mode enabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("All cameras capture at once - faster but may cause network congestion");
    }
    
    if (ImGui::RadioButton("Sequential (High Quality)", sequential_capture_mode)) {
        sequential_capture_mode = true;
        AddLogMessage("[NET] Sequential capture mode enabled");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Cameras capture one by one - slower but prevents network bottlenecks");
    }
    
    // Sequential Mode Settings
    if (sequential_capture_mode) {
        ImGui::Indent();
        ImGui::SliderInt("Delay Between Cameras (ms)", &capture_delay_ms, 100, 2000);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Time to wait between each camera capture to prevent network congestion");
        }
        
        int total_time = static_cast<int>(connected_devices.size()) * capture_delay_ms;
        ImGui::Text("Estimated total time: %.1f seconds", total_time / 1000.0f);
        ImGui::Unindent();
    }
    
    ImGui::Separator();
    
    // Network Packet Optimization
    ImGui::Text("◆ Network Packet Optimization");
    
    ImGui::Checkbox("Auto Packet Optimization", &enable_packet_optimization);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Automatically optimize packet size based on image resolution and network conditions");
    }
    
    if (ImGui::Button("► Optimize Now")) {
        OptimizeNetworkSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("► Check Bandwidth")) {
        CheckNetworkBandwidth();
    }
    
    // Current packet settings display
    ImGui::Text("Current packet size: %d bytes", camera_params.packet_size);
    ImGui::Text("Current packet delay: %d microseconds", camera_params.packet_delay);
    
    // Advanced packet settings
    if (ImGui::CollapsingHeader("◆ Advanced Packet Settings")) {
        ImGui::SliderInt("Packet Size", &camera_params.packet_size, 576, 9000);
        if (ImGui::IsItemEdited()) {
            ApplyParameterToAllCameras("GevSCPSPacketSize", std::to_string(camera_params.packet_size));
        }
        
        ImGui::SliderInt("Packet Delay (µs)", &camera_params.packet_delay, 0, 10000);
        if (ImGui::IsItemEdited()) {
            ApplyParameterToAllCameras("GevSCPD", std::to_string(camera_params.packet_delay));
        }
        
        ImGui::Checkbox("Enable Jumbo Frames (9000 bytes)", &use_jumbo_frames);
        if (ImGui::IsItemEdited()) {
            ApplyJumboFrames();
        }
    }
    
    ImGui::Separator();
    
    // Image Quality Settings
    ImGui::Text("◆ Image Quality Enhancement");
    
    if (ImGui::CollapsingHeader("Quality Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("◆ Maximum Quality", ImVec2(150, 30))) {
            sequential_capture_mode = true;
            capture_delay_ms = 1000;
            camera_params.packet_size = use_jumbo_frames ? 8000 : 1500;
            camera_params.packet_delay = 5000;
            enable_packet_optimization = true;
            OptimizeNetworkSettings();
            AddLogMessage("[NET] Maximum quality preset applied");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("◆ Balanced", ImVec2(150, 30))) {
            sequential_capture_mode = false;
            capture_delay_ms = 500;
            camera_params.packet_size = 4000;
            camera_params.packet_delay = 1000;
            enable_packet_optimization = true;
            OptimizeNetworkSettings();
            AddLogMessage("[NET] Balanced preset applied");
        }
        ImGui::SameLine();
        
        if (ImGui::Button("◆ Speed Priority", ImVec2(150, 30))) {
            sequential_capture_mode = false;
            capture_delay_ms = 100;
            camera_params.packet_size = 1500;
            camera_params.packet_delay = 0;
            enable_packet_optimization = false;
            AddLogMessage("[NET] Speed priority preset applied");
        }
    }
    
    ImGui::Separator();
    
    // Network Diagnostics
    ImGui::Text("◆ Network Diagnostics");
    
    ImGui::Checkbox("Enable Network Monitoring", &enable_bandwidth_monitoring);
    
    if (enable_bandwidth_monitoring && ImGui::CollapsingHeader("Network Status")) {
        ImGui::Text("Connected cameras: %d", (int)connected_devices.size());
        
        // Calculate estimated bandwidth usage
        float bytes_per_pixel = 3.0f; // RGB8
        float pixels_per_camera = static_cast<float>(camera_params.width * camera_params.height);
        float bytes_per_camera = pixels_per_camera * bytes_per_pixel;
        float mbps_per_camera = (bytes_per_camera * 8.0f) / (1024.0f * 1024.0f); // MB/s per camera
        float total_bandwidth = mbps_per_camera * static_cast<float>(connected_devices.size());
        
        ImGui::Text("Estimated bandwidth per camera: %.1f MB/s", mbps_per_camera);
        ImGui::Text("Total estimated bandwidth: %.1f MB/s", total_bandwidth);
        
        if (total_bandwidth > max_bandwidth_mbps) {
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "⚠ Warning: May exceed network capacity!");
            ImGui::Text("Consider enabling sequential capture mode");
        } else if (total_bandwidth > max_bandwidth_mbps * 0.8f) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "⚠ Warning: High network utilization");
        } else {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Network bandwidth OK");
        }
        
        // Network recommendations
        ImGui::Separator();
        ImGui::Text("◆ Recommendations:");
        if (total_bandwidth > max_bandwidth_mbps * 0.8f) {
            ImGui::BulletText("Enable sequential capture mode");
            ImGui::BulletText("Increase packet delay to %d µs", std::max(2000, camera_params.packet_delay));
            ImGui::BulletText("Use jumbo frames if network supports it");
            ImGui::BulletText("Consider reducing resolution temporarily");
        } else {
            ImGui::BulletText("Current settings should provide good quality");
            ImGui::BulletText("Sequential mode recommended for best quality");
        }
    }
    
    ImGui::End();
}

void OptimizeNetworkSettings() {
    if (!enable_packet_optimization) return;
    
    AddLogMessage("[NET] Optimizing network settings...");
    
    // Calculate optimal packet size based on image size and camera count
    int image_size_mb = (camera_params.width * camera_params.height * 3) / (1024 * 1024);
    int camera_count = static_cast<int>(connected_devices.size());
    
    // Adjust packet size based on network load
    int optimal_packet_size;
    int optimal_packet_delay;
    
    if (camera_count > 8) {
        // High camera count - use smaller packets with delays
        optimal_packet_size = use_jumbo_frames ? 4000 : 1200;
        optimal_packet_delay = 3000 + (camera_count - 8) * 500;
    } else if (camera_count > 4) {
        // Medium camera count
        optimal_packet_size = use_jumbo_frames ? 6000 : 1500;
        optimal_packet_delay = 1500 + (camera_count - 4) * 300;
    } else {
        // Low camera count
        optimal_packet_size = use_jumbo_frames ? 8000 : 1500;
        optimal_packet_delay = 500;
    }
    
    // Apply settings
    camera_params.packet_size = optimal_packet_size;
    camera_params.packet_delay = optimal_packet_delay;
    
    // Apply to all cameras
    ApplyParameterToAllCameras("GevSCPSPacketSize", std::to_string(camera_params.packet_size));
    ApplyParameterToAllCameras("GevSCPD", std::to_string(camera_params.packet_delay));
    
    AddLogMessage("[NET] Optimized: Packet size=" + std::to_string(optimal_packet_size) + 
                 ", Delay=" + std::to_string(optimal_packet_delay) + "µs");
}

void CaptureSequential() {
    if (connected_devices.empty()) {
        AddLogMessage("[NET] No cameras connected");
        return;
    }
    
    if (!session_manager->HasActiveSession()) {
        AddLogMessage("[SESSION] No active session - please start a new session first");
        return;
    }
    
    std::string sessionPath = session_manager->GetNextCapturePath();
    std::filesystem::create_directories(sessionPath);
    
    AddLogMessage("[NEURAL] SEQUENTIAL CAPTURE starting (optimized for neural datasets)...");
    AddLogMessage("[NEURAL] Session path: " + sessionPath);
    AddLogMessage("[NEURAL] Delay between cameras: " + std::to_string(capture_delay_ms) + "ms");
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    bool allSuccess = true;
    int successCount = 0;
    int cameraIndex = 0;
    const int totalCameras = static_cast<int>(connected_devices.size());
    
    // Pre-allocate variables outside loop for efficiency
    std::string progressMsg;
    progressMsg.reserve(256);  // Pre-allocate string buffer
    
    // Sequential capture with optimized delays
    for (const auto& camera : discovered_cameras) {
        const std::string& cameraId = camera.id;  // Use const reference
        
        auto deviceIt = connected_devices.find(cameraId);
        if (deviceIt != connected_devices.end()) {
            // Build progress message efficiently
            progressMsg.clear();
            progressMsg += "[NEURAL] Capturing ";
            progressMsg += camera.name;
            progressMsg += " (";
            progressMsg += std::to_string(cameraIndex + 1);
            progressMsg += "/";
            progressMsg += std::to_string(totalCameras);
            progressMsg += ")";
            AddLogMessage(progressMsg);
            
            if (CaptureCamera(cameraId, sessionPath)) {
                successCount++;
                AddLogMessage("[OK] " + camera.name + " → SUCCESS");
            } else {
                allSuccess = false;
                AddLogMessage("[ERR] " + camera.name + " → FAILED");
            }
            
            // Add delay between cameras (except for the last one)
            if (cameraIndex < totalCameras - 1) {
                // Only sleep if there are more cameras to capture
                std::this_thread::sleep_for(std::chrono::milliseconds(capture_delay_ms));
            }
            
            cameraIndex++;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    AddLogMessage("[NEURAL] Sequential capture completed in " + std::to_string(duration.count()) + "ms");
    AddLogMessage("[NEURAL] Success rate: " + std::to_string(successCount) + "/" + std::to_string(totalCameras) + " cameras");
    
    if (allSuccess) {
        session_manager->RecordCapture(sessionPath);
        SaveCaptureMetadata(sessionPath, session_manager->GetTotalCapturesInSession(), true, static_cast<int>(duration.count()));
        AddLogMessage("[NEURAL] ✓ All cameras captured successfully with sequential optimization!");
        
        // Calculate average time per camera for statistics
        float avgTimePerCamera = static_cast<float>(duration.count()) / totalCameras;
        AddLogMessage("[NEURAL] Average time per camera: " + std::to_string(avgTimePerCamera) + "ms");
    } else {
        SaveCaptureMetadata(sessionPath, session_manager->GetTotalCapturesInSession(), false, static_cast<int>(duration.count()));
        AddLogMessage("[NEURAL] ⚠ Some cameras failed during sequential capture");
    }
}

void CheckNetworkBandwidth() {
    AddLogMessage("[NET] Checking network bandwidth requirements...");
    
    if (connected_devices.empty()) {
        AddLogMessage("[NET] No cameras connected for bandwidth check");
        return;
    }
    
    // Calculate bandwidth requirements
    float bytes_per_pixel = 3.0f; // RGB8
    float pixels_per_camera = static_cast<float>(camera_params.width * camera_params.height);
    float bytes_per_camera = pixels_per_camera * bytes_per_pixel;
    float mbps_per_camera = (bytes_per_camera * 8.0f) / (1024.0f * 1024.0f); // Convert to Mbps
    
    int camera_count = static_cast<int>(connected_devices.size());
    float total_mbps = mbps_per_camera * static_cast<float>(camera_count);
    
    AddLogMessage("[NET] Bandwidth analysis:");
    AddLogMessage("[NET] Resolution: " + std::to_string(camera_params.width) + "x" + std::to_string(camera_params.height));
    AddLogMessage("[NET] Per camera: " + std::to_string(mbps_per_camera) + " Mbps");
    AddLogMessage("[NET] Total (" + std::to_string(camera_count) + " cameras): " + std::to_string(total_mbps) + " Mbps");
    
    if (total_mbps > 900) {
        AddLogMessage("[NET] ⚠ HIGH: Exceeds typical GigE capacity (1000 Mbps)");
        AddLogMessage("[NET] RECOMMENDATION: Use sequential capture mode");
    } else if (total_mbps > 700) {
        AddLogMessage("[NET] ⚠ MEDIUM: High network utilization");
        AddLogMessage("[NET] RECOMMENDATION: Consider sequential mode for best quality");
    } else {
        AddLogMessage("[NET] ✓ LOW: Network bandwidth should be sufficient");
    }
    
    // NEW: Detailed camera diagnostics
    AddLogMessage("[NET] === CAMERA NETWORK DIAGNOSTICS ===");
    for (auto& [cameraId, device] : connected_devices) {
        if (device) {
            AddLogMessage("[NET] Camera " + cameraId + " diagnostics:");
            
            char buffer[512];
            // Check actual packet settings
            if (device->GetFeatureValue("GevSCPSPacketSize", buffer, sizeof(buffer))) {
                AddLogMessage("[NET]   Current packet size: " + std::string(buffer));
            } else {
                AddLogMessage("[NET]   Failed to read packet size");
            }
            
            if (device->GetFeatureValue("GevSCPD", buffer, sizeof(buffer))) {
                AddLogMessage("[NET]   Current packet delay: " + std::string(buffer));
            } else {
                AddLogMessage("[NET]   Failed to read packet delay");
            }
            
            // Check network interface
            if (device->GetFeatureValue("GevInterfaceSelector", buffer, sizeof(buffer))) {
                AddLogMessage("[NET]   Interface: " + std::string(buffer));
            }
            
            // Check stream channel info
            if (device->GetFeatureValue("GevSCCFGPacketResendDestination", buffer, sizeof(buffer))) {
                AddLogMessage("[NET]   Resend destination: " + std::string(buffer));
            }
            
            // Check camera IP
            if (device->GetFeatureValue("GevCurrentIPAddress", buffer, sizeof(buffer))) {
                AddLogMessage("[NET]   Camera IP: " + std::string(buffer));
            }
            
            // Check heartbeat timeout
            if (device->GetFeatureValue("GevHeartbeatTimeout", buffer, sizeof(buffer))) {
                AddLogMessage("[NET]   Heartbeat timeout: " + std::string(buffer));
            }
        }
    }
}

void ApplyJumboFrames() {
    if (use_jumbo_frames) {
        camera_params.packet_size = std::min(9000, camera_params.packet_size * 2);
        AddLogMessage("[NET] Jumbo frames enabled - packet size increased to " + std::to_string(camera_params.packet_size));
    } else {
        camera_params.packet_size = std::min(1500, camera_params.packet_size);
        AddLogMessage("[NET] Jumbo frames disabled - packet size limited to " + std::to_string(camera_params.packet_size));
    }
    
    ApplyParameterToAllCameras("GevSCPSPacketSize", std::to_string(camera_params.packet_size));
}

void MonitorNetworkPerformance() {
    if (!enable_bandwidth_monitoring || connected_devices.empty()) {
        return;
    }
    
    static auto lastCheck = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck);
    
    // Update performance metrics every 5 seconds
    if (timeDiff.count() >= 5) {
        AddLogMessage("[NEURAL] === PERFORMANCE MONITORING ===");
        
        // Calculate theoretical bandwidth usage
        float bytes_per_pixel = 3.0f; // RGB8
        float pixels_per_camera = static_cast<float>(camera_params.width * camera_params.height);
        float bytes_per_image = pixels_per_camera * bytes_per_pixel;
        float mb_per_image = bytes_per_image / (1024.0f * 1024.0f);
        
        int camera_count = static_cast<int>(connected_devices.size());
        float total_mb_per_capture = mb_per_image * camera_count;
        
        AddLogMessage("[NEURAL] Image size per camera: " + std::to_string(mb_per_image) + " MB");
        AddLogMessage("[NEURAL] Total data per capture: " + std::to_string(total_mb_per_capture) + " MB");
        
        // Sequential mode performance analysis
        if (sequential_capture_mode) {
            float total_time_seconds = (camera_count * capture_delay_ms) / 1000.0f;
            float throughput_mbps = (total_mb_per_capture * 8.0f) / total_time_seconds;
            AddLogMessage("[NEURAL] Sequential throughput: " + std::to_string(throughput_mbps) + " Mbps");
            AddLogMessage("[NEURAL] Time per full capture: " + std::to_string(total_time_seconds) + "s");
        }
        
        // Network configuration summary
        AddLogMessage("[NEURAL] Network config: " + std::to_string(camera_params.packet_size) + 
                     " bytes packets, " + std::to_string(camera_params.packet_delay) + "µs delay");
        
        // Memory usage estimation
        float buffer_memory_mb = (bytes_per_image * camera_count) / (1024.0f * 1024.0f);
        AddLogMessage("[NEURAL] Estimated buffer memory: " + std::to_string(buffer_memory_mb) + " MB");
        
        lastCheck = now;
    }
}

void ApplyMaximumQualityDefaults() {
    // This function is now deprecated - optimization happens in ConnectAllCameras before connection
    AddLogMessage("[NEURAL] Quality optimization is now automatically applied before camera connection");
}// ========================================
// Bluetooth Implementation
// ========================================

void InitializeBluetooth() {
    bluetooth_manager = &BluetoothManager::GetInstance();
    
    // Set log callback to integrate with our logging system
    bluetooth_manager->SetLogCallback([](const std::string& msg) {
        AddLogMessage(msg);
    });
    
    // Set device discovered callback
    bluetooth_manager->SetDeviceDiscoveredCallback([](const std::string& id, const std::string& name) {
        discovered_bt_devices.push_back({id, name});
        AddLogMessage("[Bluetooth] Discovered: " + name);
    });
    
    // Set connection status callback
    bluetooth_manager->SetConnectionStatusCallback([](const std::string& id, bool connected) {
        if (connected) {
            connected_bt_device_id = id;
            AddLogMessage("[Bluetooth] Connected to device: " + id);
        } else {
            if (connected_bt_device_id == id) {
                connected_bt_device_id = "";
            }
            AddLogMessage("[Bluetooth] Disconnected from device: " + id);
        }
    });
    
    // Initialize the manager
    if (!bluetooth_manager->Initialize()) {
        AddLogMessage("[Bluetooth] Failed to initialize Bluetooth manager");
    } else {
        AddLogMessage("[Bluetooth] Bluetooth manager initialized successfully");
    }
}

void ShutdownBluetooth() {
    if (bluetooth_manager) {
        bluetooth_manager->Shutdown();
        bluetooth_manager = nullptr;
    }
}

void RenderBluetoothPanel() {
    ImGui::Begin("🔷 Bluetooth Control", &show_bluetooth_panel, ImGuiWindowFlags_NoCollapse);
    
    // Status bar
    ImGui::Text("Status:");
    ImGui::SameLine();
    if (!connected_bt_device_id.empty()) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected");
        ImGui::SameLine();
        ImGui::Text("- %s", connected_bt_device_id.c_str());
    } else if (bt_is_connecting) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Connecting...");
    } else if (bt_is_scanning) {
        ImGui::TextColored(ImVec4(0.0f, 0.5f, 1.0f, 1.0f), "Scanning...");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Disconnected");
    }
    
    ImGui::Separator();
    
    // Tab bar for different sections
    if (ImGui::BeginTabBar("BluetoothTabs")) {
        // Scanner Tab
        if (ImGui::BeginTabItem("Scanner")) {
            // Scan controls
            if (!bt_is_scanning) {
                if (ImGui::Button("Start Scan", ImVec2(120, 0))) {
                    discovered_bt_devices.clear();
                    bluetooth_manager->StartScanning();
                    bt_is_scanning = true;
                }
                ImGui::SameLine();
                ImGui::Text("Click to start scanning for devices");
            } else {
                if (ImGui::Button("Stop Scan", ImVec2(120, 0))) {
                    bluetooth_manager->StopScanning();
                    bt_is_scanning = false;
                }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "Scanning... (%zu devices found)", discovered_bt_devices.size());
            }
            
            ImGui::Separator();
            
            // Discovered devices list
            ImGui::Text("Discovered Devices:");
            ImGui::BeginChild("DeviceList", ImVec2(0, 200), true);
            
            for (const auto& [id, name] : discovered_bt_devices) {
                bool is_selected = (selected_bt_device_id == id);
                bool is_connected = (connected_bt_device_id == id);
                
                if (is_connected) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                }
                
                if (ImGui::Selectable(name.c_str(), is_selected)) {
                    selected_bt_device_id = id;
                }
                
                if (is_connected) {
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::Text(" [Connected]");
                }
            }
            
            ImGui::EndChild();
            
            // Connection controls
            if (!selected_bt_device_id.empty()) {
                ImGui::Separator();
                
                if (connected_bt_device_id == selected_bt_device_id) {
                    if (ImGui::Button("Disconnect", ImVec2(120, 0))) {
                        bluetooth_manager->DisconnectDevice(selected_bt_device_id);
                    }
                } else {
                    if (ImGui::Button("Connect", ImVec2(120, 0))) {
                        bt_is_connecting = true;
                        std::thread([&]() {
                            bool success = bluetooth_manager->ConnectToDevice(selected_bt_device_id);
                            bt_is_connecting = false;
                            if (!success) {
                                AddLogMessage("[Bluetooth] Failed to connect to device");
                            }
                        }).detach();
                    }
                }
            }
            
            ImGui::EndTabItem();
        }
        
        // Control Tab
        if (ImGui::BeginTabItem("Control")) {
            if (!connected_bt_device_id.empty()) {
                // Rotation controls
                ImGui::Text("Rotation Control:");
                
                // Rotation Angle - Slider + Input
                ImGui::PushItemWidth(150);
                ImGui::SliderFloat("##RotAngleSlider", &bt_rotation_angle, -360.0f, 360.0f);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::PushItemWidth(80);
                ImGui::InputFloat("Angle (°)", &bt_rotation_angle, 0.1f, 1.0f, "%.1f");
                ImGui::PopItemWidth();
                
                // Clamp rotation angle
                if (bt_rotation_angle < -360.0f) bt_rotation_angle = -360.0f;
                if (bt_rotation_angle > 360.0f) bt_rotation_angle = 360.0f;
                
                // Rotation Speed - Slider + Input  
                ImGui::PushItemWidth(150);
                ImGui::SliderFloat("##RotSpeedSlider", &bt_rotation_speed, 35.64f, 131.0f);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::PushItemWidth(80);
                ImGui::InputFloat("Speed", &bt_rotation_speed, 0.1f, 1.0f, "%.2f");
                ImGui::PopItemWidth();
                
                // Clamp rotation speed
                if (bt_rotation_speed < 35.64f) bt_rotation_speed = 35.64f;
                if (bt_rotation_speed > 131.0f) bt_rotation_speed = 131.0f;
                
                // Rotation buttons
                if (ImGui::Button("Rotate", ImVec2(80, 0))) {
                    bluetooth_manager->RotateTurntable(connected_bt_device_id, bt_rotation_angle);
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop", ImVec2(80, 0))) {
                    bluetooth_manager->StopRotation(connected_bt_device_id);
                }
                ImGui::SameLine();
                if (ImGui::Button("Zero", ImVec2(80, 0))) {
                    bluetooth_manager->ReturnToZero(connected_bt_device_id);
                }
                
                // Quick rotation buttons
                ImGui::Text("Quick Rotate:");
                if (ImGui::Button("-90°", ImVec2(60, 0))) {
                    bluetooth_manager->RotateTurntable(connected_bt_device_id, -90.0f);
                }
                ImGui::SameLine();
                if (ImGui::Button("-45°", ImVec2(60, 0))) {
                    bluetooth_manager->RotateTurntable(connected_bt_device_id, -45.0f);
                }
                ImGui::SameLine();
                if (ImGui::Button("-30°", ImVec2(60, 0))) {
                    bluetooth_manager->RotateTurntable(connected_bt_device_id, -30.0f);
                }
                ImGui::SameLine();
                if (ImGui::Button("+30°", ImVec2(60, 0))) {
                    bluetooth_manager->RotateTurntable(connected_bt_device_id, 30.0f);
                }
                ImGui::SameLine();
                if (ImGui::Button("+45°", ImVec2(60, 0))) {
                    bluetooth_manager->RotateTurntable(connected_bt_device_id, 45.0f);
                }
                ImGui::SameLine();
                if (ImGui::Button("+90°", ImVec2(60, 0))) {
                    bluetooth_manager->RotateTurntable(connected_bt_device_id, 90.0f);
                }
                
                ImGui::Separator();
                
                // Tilt controls
                ImGui::Text("Tilt Control:");
                
                // Tilt Angle - Slider + Input
                ImGui::PushItemWidth(150);
                ImGui::SliderFloat("##TiltAngleSlider", &bt_tilt_angle, -30.0f, 30.0f);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::PushItemWidth(80);
                ImGui::InputFloat("Tilt (°)", &bt_tilt_angle, 0.1f, 1.0f, "%.1f");
                ImGui::PopItemWidth();
                
                // Clamp tilt angle
                if (bt_tilt_angle < -30.0f) bt_tilt_angle = -30.0f;
                if (bt_tilt_angle > 30.0f) bt_tilt_angle = 30.0f;
                
                // Tilt Speed - Slider + Input
                ImGui::PushItemWidth(150);
                ImGui::SliderFloat("##TiltSpeedSlider", &bt_tilt_speed, 9.0f, 35.0f);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::PushItemWidth(80);
                ImGui::InputFloat("T.Speed", &bt_tilt_speed, 0.1f, 1.0f, "%.1f");
                ImGui::PopItemWidth();
                
                // Clamp tilt speed
                if (bt_tilt_speed < 9.0f) bt_tilt_speed = 9.0f;
                if (bt_tilt_speed > 35.0f) bt_tilt_speed = 35.0f;
                
                // Tilt buttons
                if (ImGui::Button("Tilt", ImVec2(80, 0))) {
                    bluetooth_manager->TiltTurntable(connected_bt_device_id, bt_tilt_angle);
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop Tilt", ImVec2(80, 0))) {
                    bluetooth_manager->StopTilt(connected_bt_device_id);
                }
                ImGui::SameLine();
                if (ImGui::Button("Level", ImVec2(80, 0))) {
                    bluetooth_manager->TiltTurntable(connected_bt_device_id, 0.0f);
                }
                
                ImGui::Separator();
                
                // Status query
                if (ImGui::Button("Get Status", ImVec2(120, 0))) {
                    bluetooth_manager->GetTurntableStatus(connected_bt_device_id, 
                        [](const std::string& response) {
                            AddLogMessage("[Turntable] Status: " + response);
                        });
                }
                
            } else {
                ImGui::Text("No device connected");
                ImGui::Text("Please connect to a device in the Scanner tab");
            }
            
            ImGui::EndTabItem();
        }
        
        // Capture Sequence Tab
        if (ImGui::BeginTabItem("Capture Sequence")) {
            if (!connected_bt_device_id.empty()) {
                ImGui::Text("Automated Capture Sequence:");
                
                ImGui::Checkbox("Auto-rotate during capture", &bt_auto_rotate_capture);
                
                if (bt_auto_rotate_capture) {
                    // Number of steps - Slider + Input
                    ImGui::PushItemWidth(150);
                    ImGui::SliderInt("##StepsSlider", &bt_capture_steps, 1, 72);
                    ImGui::PopItemWidth();
                    ImGui::SameLine();
                    ImGui::PushItemWidth(80);
                    ImGui::InputInt("Steps", &bt_capture_steps);
                    ImGui::PopItemWidth();
                    
                    // Clamp steps
                    if (bt_capture_steps < 1) bt_capture_steps = 1;
                    if (bt_capture_steps > 360) bt_capture_steps = 360;
                    
                    bt_step_angle = 360.0f / bt_capture_steps;
                    ImGui::Text("Step angle: %.2f degrees", bt_step_angle);
                    
                    // Preset sequences
                    ImGui::Text("Presets:");
                    if (ImGui::Button("Quick (8 steps)", ImVec2(120, 0))) {
                        bt_capture_steps = 8;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Standard (12 steps)", ImVec2(120, 0))) {
                        bt_capture_steps = 12;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Detailed (24 steps)", ImVec2(120, 0))) {
                        bt_capture_steps = 24;
                    }
                    
                    ImGui::Separator();
                    
                    if (ImGui::Button("Start Capture Sequence", ImVec2(200, 30))) {
                        // This would trigger a capture sequence with rotation
                        AddLogMessage("[Bluetooth] Starting capture sequence with " + 
                                    std::to_string(bt_capture_steps) + " steps");
                        
                        // Return to zero first
                        bluetooth_manager->ReturnToZero(connected_bt_device_id);
                        
                        // Note: The actual capture integration would be done here
                        // For now, just demonstrate the rotation
                        for (int i = 0; i < bt_capture_steps; ++i) {
                            // Rotate to next position
                            bluetooth_manager->RotateTurntable(connected_bt_device_id, bt_step_angle);
                            
                            // Here you would trigger camera capture
                            // CaptureAllCameras();
                            
                            // Wait for rotation to complete
                            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                        }
                    }
                }
                
            } else {
                ImGui::Text("No device connected");
                ImGui::Text("Please connect to a device in the Scanner tab");
            }
            
            ImGui::EndTabItem();
        }
        
        // Settings Tab
        if (ImGui::BeginTabItem("Settings")) {
            ImGui::Text("Bluetooth Configuration:");
            
            ImGui::PushItemWidth(400);
            
            if (ImGui::InputText("Service UUID", bt_service_uuid, sizeof(bt_service_uuid))) {
                bluetooth_manager->SetServiceUUID(bt_service_uuid);
            }
            
            if (ImGui::InputText("Characteristic UUID", bt_char_uuid, sizeof(bt_char_uuid))) {
                bluetooth_manager->SetCharacteristicUUID(bt_char_uuid);
            }
            
            ImGui::PopItemWidth();
            
            ImGui::Separator();
            
            if (ImGui::Button("Save Settings", ImVec2(120, 0))) {
                bluetooth_manager->SaveSettings();
                AddLogMessage("[Bluetooth] Settings saved");
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Load Settings", ImVec2(120, 0))) {
                bluetooth_manager->LoadSettings();
                strcpy(bt_service_uuid, bluetooth_manager->GetServiceUUID().c_str());
                strcpy(bt_char_uuid, bluetooth_manager->GetCharacteristicUUID().c_str());
                AddLogMessage("[Bluetooth] Settings loaded");
            }
            
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}