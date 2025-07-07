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

// Sapera SDK FIRST to avoid macro conflicts
#include "SapClassBasic.h"
#include "hardware/CameraTypes.hpp"

// Dear ImGui AFTER Sapera to avoid APIENTRY conflicts
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

using namespace SaperaCapturePro;

// Global variables for the GUI
static GLFWwindow* window = nullptr;
static bool show_camera_panel = true;
static bool show_capture_panel = true;
static bool show_log_panel = true;
static std::vector<std::string> log_messages;
static std::string current_image_folder = "neural_dataset";
static char image_folder_buffer[512];
static int exposure_time = 40000;
static bool capture_format_raw = false; // false = TIFF, true = RAW

// Camera system state
static std::vector<CameraInfo> discovered_cameras;
static std::map<std::string, SapAcqDevice*> connected_devices;
static std::map<std::string, SapBuffer*> connected_buffers;
static std::map<std::string, SapAcqDeviceToBuf*> connected_transfers;
static std::map<std::string, std::string> camera_device_names; // Store device names separately
static int capture_counter = 1;

// Function declarations
void AddLogMessage(const std::string& message);
std::string GetCurrentTimestamp();
void DiscoverCameras();
void ConnectAllCameras();
void CaptureAllCameras();
bool CaptureCamera(const std::string& cameraId, const std::string& sessionPath);
std::string GenerateSessionName(int captureNumber);
void OpenFolderInExplorer(const std::string& path);

// GUI functions
void RenderMainMenuBar();
void RenderCameraPanel();
void RenderCapturePanel();
void RenderLogPanel();

// GLFW error callback
static void GlfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main() {
    std::cout << "🎬 Neural Rendering Capture System - DIRECT INTEGRATION" << std::endl;
    std::cout << "=======================================================" << std::endl;
    
    // Initialize folder buffer
    std::strcpy(image_folder_buffer, current_image_folder.c_str());
    
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
    window = glfwCreateWindow(1600, 900, "Neural Rendering Capture System - DIRECT", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    AddLogMessage("✅ DIRECT GUI initialized successfully");
    
    // Create dataset directories
    std::filesystem::create_directories(current_image_folder + "/images");
    std::filesystem::create_directories(current_image_folder + "/metadata");
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render GUI
        RenderMainMenuBar();
        
        if (show_camera_panel) {
            RenderCameraPanel();
        }
        
        if (show_capture_panel) {
            RenderCapturePanel();
        }
        
        if (show_log_panel) {
            RenderLogPanel();
        }
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
        
        // Small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    // Cleanup
    AddLogMessage("🔌 Shutting down capture system");
    
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
    
    // Cleanup GUI
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    std::cout << "✅ DIRECT application terminated successfully" << std::endl;
    return 0;
}

void RenderMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("System")) {
            if (ImGui::MenuItem("Discover Cameras")) {
                DiscoverCameras();
            }
            if (ImGui::MenuItem("Connect All Cameras")) {
                ConnectAllCameras();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Camera Panel", NULL, &show_camera_panel);
            ImGui::MenuItem("Capture Panel", NULL, &show_capture_panel);
            ImGui::MenuItem("System Log", NULL, &show_log_panel);
            ImGui::EndMenu();
        }
        
        // Status
        ImGui::Text("Status: DIRECT Integration Ready");
        
        ImGui::EndMainMenuBar();
    }
}

void RenderCameraPanel() {
    ImGui::Begin("Camera System", &show_camera_panel);
    
    ImGui::Text("DIRECT Sapera SDK Camera Control");
    ImGui::Separator();
    
    // Control buttons
    if (ImGui::Button("🔍 Discover Cameras", ImVec2(200, 40))) {
        DiscoverCameras();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("🔌 Connect All", ImVec2(200, 40))) {
        ConnectAllCameras();
    }
    
    ImGui::Separator();
    
    // Exposure control
    if (ImGui::SliderInt("Exposure Time (μs)", &exposure_time, 1000, 100000)) {
        // Apply to all connected cameras
        for (auto& [id, device] : connected_devices) {
            if (device) {
                std::string exposureStr = std::to_string(exposure_time);
                device->SetFeatureValue("ExposureTime", exposureStr.c_str());
            }
        }
        AddLogMessage("⚙️ Exposure time set to " + std::to_string(exposure_time) + "μs");
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
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "🟢 Connected");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "🔴 Disconnected");
            }
        }
        
        ImGui::EndTable();
    }
    
    ImGui::Text("Connected: %d / %d cameras", (int)connected_devices.size(), (int)discovered_cameras.size());
    
    ImGui::End();
}

void RenderCapturePanel() {
    ImGui::Begin("Capture Control", &show_capture_panel);
    
    ImGui::Text("DIRECT Neural Rendering Capture");
    ImGui::Separator();
    
    // Session info
    ImGui::Text("Dataset: %s", current_image_folder.c_str());
    ImGui::Text("Next Capture: #%d", capture_counter);
    
    // Folder management
    ImGui::Text("Output Folder:");
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##ImageFolder", image_folder_buffer, sizeof(image_folder_buffer))) {
        current_image_folder = image_folder_buffer;
        AddLogMessage("📁 Dataset path changed to: " + current_image_folder);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("📂 Open")) {
        OpenFolderInExplorer(current_image_folder);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("📁 Create")) {
        try {
            std::filesystem::create_directories(current_image_folder + "/images");
            std::filesystem::create_directories(current_image_folder + "/metadata");
            AddLogMessage("📁 Created folder: " + current_image_folder);
        } catch (const std::exception& e) {
            AddLogMessage("❌ Error creating folder: " + std::string(e.what()));
        }
    }
    
    // Format selection
    ImGui::Text("Format:");
    ImGui::SameLine();
    if (ImGui::RadioButton("TIFF", !capture_format_raw)) {
        capture_format_raw = false;
        AddLogMessage("📷 Format set to TIFF");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("RAW", capture_format_raw)) {
        capture_format_raw = true;
        AddLogMessage("📷 Format set to RAW");
    }
    
    ImGui::Separator();
    
    // MAIN CAPTURE BUTTON
    if (ImGui::Button("🎬 CAPTURE ALL CAMERAS", ImVec2(300, 60))) {
        CaptureAllCameras();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("🔄 Reset Counter", ImVec2(150, 60))) {
        capture_counter = 1;
        AddLogMessage("🔄 Capture counter reset");
    }
    
    ImGui::Separator();
    
    // File size info
    if (!connected_devices.empty()) {
        ImGui::Text("Estimated file sizes:");
        if (!capture_format_raw) {
            ImGui::BulletText("Per camera: ~37 MB (TIFF)");
            ImGui::BulletText("Total (%d cameras): ~%d MB per capture", 
                (int)connected_devices.size(), (int)connected_devices.size() * 37);
        } else {
            ImGui::BulletText("Per camera: ~12 MB (RAW)");
            ImGui::BulletText("Total (%d cameras): ~%d MB per capture", 
                (int)connected_devices.size(), (int)connected_devices.size() * 12);
        }
    }
    
    ImGui::End();
}

void RenderLogPanel() {
    ImGui::Begin("System Log", &show_log_panel);
    
    ImGui::Text("DIRECT System Messages");
    ImGui::Separator();
    
    // Log controls
    if (ImGui::Button("Clear Log")) {
        log_messages.clear();
    }
    
    ImGui::SameLine();
    ImGui::Text("(%d messages)", (int)log_messages.size());
    
    ImGui::Separator();
    
    // Log messages
    ImGui::BeginChild("LogMessages", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    for (const auto& message : log_messages) {
        // Color code messages
        if (message.find("✅") != std::string::npos) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", message.c_str());
        } else if (message.find("❌") != std::string::npos) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", message.c_str());
        } else if (message.find("⚠️") != std::string::npos) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "%s", message.c_str());
        } else if (message.find("🎬") != std::string::npos) {
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "%s", message.c_str());
        } else {
            ImGui::Text("%s", message.c_str());
        }
    }
    
    // Auto-scroll
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
    
    ImGui::End();
}

void AddLogMessage(const std::string& message) {
    std::string timestamp = GetCurrentTimestamp();
    std::string formatted_message = "[" + timestamp + "] " + message;
    
    log_messages.push_back(formatted_message);
    
    // Keep only last 200 messages
    if (log_messages.size() > 200) {
        log_messages.erase(log_messages.begin());
    }
    
    // Also print to console
    std::cout << formatted_message << std::endl;
}

std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    
    return ss.str();
}

void DiscoverCameras() {
    AddLogMessage("🔍 Discovering DIRECT cameras...");
    
    discovered_cameras.clear();
    camera_device_names.clear();
    
    // Get server count
    int serverCount = SapManager::GetServerCount();
    AddLogMessage("Found " + std::to_string(serverCount) + " server(s)");
    
    if (serverCount == 0) {
        AddLogMessage("❌ No Sapera servers found");
        return;
    }
    
    int cameraIndex = 1;
    
    // Enumerate all servers
    for (int serverIndex = 0; serverIndex < serverCount; serverIndex++) {
        char serverName[256];
        if (!SapManager::GetServerName(serverIndex, serverName, sizeof(serverName))) {
            AddLogMessage("❌ Failed to get server name for server " + std::to_string(serverIndex));
            continue;
        }
        
        // Skip system server
        if (std::string(serverName) == "System") {
            continue;
        }
        
        AddLogMessage("🖥️ Server " + std::to_string(serverIndex) + ": " + std::string(serverName));
        
        // Get acquisition device count for this server
        int resourceCount = SapManager::GetResourceCount(serverName, SapManager::ResourceAcqDevice);
        AddLogMessage("  📸 Acquisition devices: " + std::to_string(resourceCount));
        
        // Enumerate acquisition devices
        for (int resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
            try {
                // Create acquisition device temporarily for discovery
                auto acqDevice = new SapAcqDevice(serverName, resourceIndex);
                if (!acqDevice->Create()) {
                    AddLogMessage("  ❌ Failed to create device " + std::to_string(resourceIndex));
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
                
                discovered_cameras.push_back(camera);
                AddLogMessage("  ✅ " + camera.name + ": " + camera.serialNumber + " (" + camera.modelName + ")");
                
                // Cleanup discovery device
                acqDevice->Destroy();
                delete acqDevice;
                
                cameraIndex++;
                
            } catch (const std::exception& e) {
                AddLogMessage("  ❌ Exception: " + std::string(e.what()));
            }
        }
    }
    
    AddLogMessage("✅ Discovery complete: " + std::to_string(discovered_cameras.size()) + " cameras found");
}

void ConnectAllCameras() {
    AddLogMessage("🔌 Connecting to DIRECT cameras...");
    
    if (discovered_cameras.empty()) {
        AddLogMessage("❌ No cameras discovered. Run camera discovery first.");
        return;
    }
    
    int successCount = 0;
    
    for (const auto& camera : discovered_cameras) {
        std::string cameraId = camera.id;
        
        try {
            // Create acquisition device using serverName and resourceIndex
            SapAcqDevice* acqDevice = new SapAcqDevice(camera.serverName.c_str(), camera.resourceIndex);
            if (!acqDevice->Create()) {
                AddLogMessage("❌ Failed to create acquisition device for " + camera.name);
                delete acqDevice;
                continue;
            }
            
            // Apply exposure time setting
            std::string exposureStr = std::to_string(exposure_time);
            if (!acqDevice->SetFeatureValue("ExposureTime", exposureStr.c_str())) {
                AddLogMessage("⚠️ Warning: Failed to set exposure time for " + camera.name);
            }
            
            // Create buffer for image capture
            SapBuffer* buffer = new SapBufferWithTrash(1, acqDevice);
            if (!buffer->Create()) {
                AddLogMessage("❌ Failed to create buffer for " + camera.name);
                acqDevice->Destroy();
                delete acqDevice;
                delete buffer;
                continue;
            }
            
            // Create transfer object
            SapAcqDeviceToBuf* transfer = new SapAcqDeviceToBuf(acqDevice, buffer);
            if (!transfer->Create()) {
                AddLogMessage("❌ Failed to create transfer for " + camera.name);
                buffer->Destroy();
                acqDevice->Destroy();
                delete transfer;
                delete buffer;
                delete acqDevice;
                continue;
            }
            
            // Store connected components
            connected_devices[cameraId] = acqDevice;
            connected_buffers[cameraId] = buffer;
            connected_transfers[cameraId] = transfer;
            
            successCount++;
            AddLogMessage("✅ " + camera.name + " connected successfully");
            
        } catch (const std::exception& e) {
            AddLogMessage("❌ Exception connecting " + camera.name + ": " + std::string(e.what()));
        }
    }
    
    AddLogMessage("🎯 Connection summary: " + std::to_string(successCount) + "/" + std::to_string(discovered_cameras.size()) + " cameras connected");
    
    if (successCount == discovered_cameras.size() && successCount > 0) {
        AddLogMessage("🎉 All cameras connected successfully!");
    } else if (successCount > 0) {
        AddLogMessage("⚠️ Partial connection: " + std::to_string(successCount) + " cameras connected");
    } else {
        AddLogMessage("❌ No cameras connected");
    }
}

void CaptureAllCameras() {
    if (connected_devices.empty()) {
        AddLogMessage("❌ No cameras connected");
        return;
    }
    
    std::string sessionName = GenerateSessionName(capture_counter);
    std::string sessionPath = current_image_folder + "/images/" + sessionName;
    
    // Create session directory
    std::filesystem::create_directories(sessionPath);
    
    AddLogMessage("🎬 DIRECT CAPTURE starting...");
    AddLogMessage("📁 Session path: " + sessionPath);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    bool allSuccess = true;
    int successCount = 0;
    
    // Capture from all cameras
    for (const auto& camera : discovered_cameras) {
        std::string cameraId = camera.id;
        
        if (connected_devices.find(cameraId) != connected_devices.end()) {
            if (CaptureCamera(cameraId, sessionPath)) {
                successCount++;
            } else {
                allSuccess = false;
            }
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    AddLogMessage("🎯 Capture completed in " + std::to_string(duration.count()) + "ms");
    AddLogMessage("✅ Success: " + std::to_string(successCount) + "/" + std::to_string(connected_devices.size()) + " cameras");
    
    if (allSuccess) {
        capture_counter++;
        AddLogMessage("🎉 All cameras captured successfully!");
        
        // Count created files
        try {
            int file_count = 0;
            std::string images_path = current_image_folder + "/images";
            if (std::filesystem::exists(images_path)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(images_path)) {
                    if (entry.is_regular_file()) {
                        file_count++;
                    }
                }
            }
            AddLogMessage("✅ Total files in dataset: " + std::to_string(file_count));
        } catch (const std::exception& e) {
            AddLogMessage("⚠️ Could not count files: " + std::string(e.what()));
        }
    } else {
        AddLogMessage("⚠️ Some cameras failed to capture");
    }
}

bool CaptureCamera(const std::string& cameraId, const std::string& sessionPath) {
    auto deviceIt = connected_devices.find(cameraId);
    auto bufferIt = connected_buffers.find(cameraId);
    auto transferIt = connected_transfers.find(cameraId);
    
    if (deviceIt == connected_devices.end() || bufferIt == connected_buffers.end() || transferIt == connected_transfers.end()) {
        return false;
    }
    
    SapAcqDevice* device = deviceIt->second;
    SapBuffer* buffer = bufferIt->second;
    SapAcqDeviceToBuf* transfer = transferIt->second;
    
    try {
        // Trigger capture
        if (!transfer->Snap()) {
            AddLogMessage("❌ Failed to trigger capture for " + cameraId);
            return false;
        }
        
        // Wait for capture completion
        if (!transfer->Wait(5000)) {
            AddLogMessage("❌ Capture timeout for " + cameraId);
            return false;
        }
        
        // Find camera name
        std::string cameraName = "unknown";
        for (const auto& camera : discovered_cameras) {
            if (camera.id == cameraId) {
                cameraName = camera.name;
                break;
            }
        }
        
        // Generate filename
        std::string extension = capture_format_raw ? ".raw" : ".tiff";
        std::string filename = cameraName + "_capture_" + std::to_string(capture_counter) + extension;
        std::string fullPath = sessionPath + "/" + filename;
        
        // Save image
        if (capture_format_raw) {
            // Save as RAW
            if (buffer->Save(fullPath.c_str(), "-format raw")) {
                return true;
            }
        } else {
            // Save as TIFF with color conversion (CORRECT API)
            SapColorConversion colorConverter(buffer);
            if (!colorConverter.Create()) {
                AddLogMessage("❌ Failed to create color converter for " + cameraId);
                return false;
            }
            
            // Configure converter for RGB output
            colorConverter.Enable(TRUE, FALSE);
            colorConverter.SetOutputFormat(SapFormatRGB888);
            colorConverter.SetAlign(SapColorConversion::AlignRGGB);
            
            // Convert the image to RGB
            if (!colorConverter.Convert()) {
                AddLogMessage("❌ Color conversion failed for " + cameraId);
                colorConverter.Destroy();
                return false;
            }
            
            // Get converted RGB buffer
            SapBuffer* outputBuffer = colorConverter.GetOutputBuffer();
            if (!outputBuffer) {
                AddLogMessage("❌ No output buffer for " + cameraId);
                colorConverter.Destroy();
                return false;
            }
            
            // Save converted RGB buffer as TIFF
            bool saveSuccess = outputBuffer->Save(fullPath.c_str(), "-format tiff");
            
            // Clean up converter
            colorConverter.Destroy();
            
            return saveSuccess;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        AddLogMessage("❌ Exception capturing " + cameraId + ": " + std::string(e.what()));
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
#ifdef _WIN32
        std::string command = "explorer \"" + path + "\"";
        system(command.c_str());
#endif
        AddLogMessage("📂 Opened folder: " + path);
    } else {
        AddLogMessage("❌ Folder does not exist: " + path);
    }
} 