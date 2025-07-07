#include "gui/NeuralCaptureGUI.hpp"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

// Forward declare the capture system classes we need
class NeuralRenderingCaptureSystem;
class ParameterController;

// Simplified version for GUI integration
class NeuralCaptureGUIApp {
private:
    std::unique_ptr<NeuralCaptureGUI> gui_;
    std::vector<std::pair<std::string, std::string>> log_messages_;
    bool running_ = true;
    
    // Current system state
    std::vector<CameraInfo> current_cameras_;
    CaptureSession current_session_;
    
public:
    NeuralCaptureGUIApp() {
        gui_ = std::make_unique<NeuralCaptureGUI>();
        
        // Initialize session
        current_session_.sessionName = "neural_capture_session";
        current_session_.format = "TIFF";
        current_session_.outputPath = "neural_dataset";
        current_session_.isActive = false;
        current_session_.captureCount = 0;
        
        AddLogMessage("Application started", "INFO");
    }
    
    ~NeuralCaptureGUIApp() = default;
    
    bool Initialize() {
        if (!gui_->Initialize()) {
            std::cerr << "Failed to initialize GUI" << std::endl;
            return false;
        }
        
        // Set up GUI callbacks
        SetupCallbacks();
        
        // Initialize capture system
        AddLogMessage("Initializing camera system...", "INFO");
        
        // Discover cameras on startup
        DiscoverCameras();
        
        AddLogMessage("GUI application initialized successfully", "INFO");
        return true;
    }
    
    void Run() {
        AddLogMessage("Starting main application loop", "INFO");
        
        while (running_ && !gui_->ShouldClose()) {
            // Update GUI
            gui_->Update();
            
            // Render GUI with current data
            gui_->Render(current_cameras_, current_session_, log_messages_);
            
            // Present frame
            gui_->Present();
            
            // Small delay to prevent excessive CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
        
        AddLogMessage("Application shutting down", "INFO");
    }
    
    void Shutdown() {
        AddLogMessage("Shutting down camera system", "INFO");
        
        if (gui_) {
            gui_->Shutdown();
        }
    }
    
private:
    void SetupCallbacks() {
        // Camera control callbacks
        if (gui_->GetCameraPanel()) {
            gui_->GetCameraPanel()->onDiscoverCameras = [this]() {
                DiscoverCameras();
            };
            
            gui_->GetCameraPanel()->onConnectAllCameras = [this]() {
                ConnectAllCameras();
            };
            
            gui_->GetCameraPanel()->onDisconnectAllCameras = [this]() {
                DisconnectAllCameras();
            };
        }
        
        // Parameter control callbacks
        if (gui_->GetParameterPanel()) {
            gui_->GetParameterPanel()->onSetParameter = [this](const std::string& param, const std::string& value) {
                SetParameter(param, value);
            };
            
            gui_->GetParameterPanel()->onGetParameters = [this]() -> std::vector<ParameterInfo> {
                return GetParameters();
            };
        }
        
        // Capture control callbacks
        if (gui_->GetCapturePanel()) {
            gui_->GetCapturePanel()->onStartCapture = [this]() {
                StartCapture();
            };
            
            gui_->GetCapturePanel()->onStopCapture = [this]() {
                StopCapture();
            };
            
            gui_->GetCapturePanel()->onResetCapture = [this]() {
                ResetCapture();
            };
            
            gui_->GetCapturePanel()->onSetCaptureFormat = [this](const std::string& format) {
                SetCaptureFormat(format);
            };
            
            gui_->GetCapturePanel()->onSetOutputPath = [this](const std::string& path) {
                SetOutputPath(path);
            };
            
            gui_->GetCapturePanel()->onSetSessionName = [this](const std::string& name) {
                SetSessionName(name);
            };
        }
    }
    
    void DiscoverCameras() {
        AddLogMessage("Discovering cameras...", "INFO");
        
        try {
            current_cameras_.clear();
            
            // Simulate discovering 12 cameras (matching your actual setup)
            std::vector<std::string> serial_numbers = {
                "S1128470", "S1160345", "S1160346", "S1160347", "S1160348", "S1160349",
                "S1160350", "S1160351", "S1160352", "S1160353", "S1160354", "S1160355"
            };
            
            for (int i = 0; i < 12; ++i) {
                CameraInfo camera;
                camera.serialNumber = serial_numbers[i];
                camera.userDefinedName = "Nano-C4020_" + std::to_string(i + 1);
                camera.serverName = "CameraLink_" + std::to_string(i);
                camera.isConnected = false;
                camera.isCapturing = false;
                camera.cameraIndex = i + 1;
                
                current_cameras_.push_back(camera);
            }
            
            AddLogMessage("Discovered " + std::to_string(current_cameras_.size()) + " cameras", "INFO");
        } catch (const std::exception& e) {
            AddLogMessage("Error discovering cameras: " + std::string(e.what()), "ERROR");
        }
    }
    
    void ConnectAllCameras() {
        AddLogMessage("Connecting all cameras...", "INFO");
        
        try {
            for (auto& camera : current_cameras_) {
                camera.isConnected = true;
                AddLogMessage("Connected camera: " + camera.serialNumber, "INFO");
            }
            
            AddLogMessage("All cameras connected successfully", "INFO");
        } catch (const std::exception& e) {
            AddLogMessage("Error connecting cameras: " + std::string(e.what()), "ERROR");
        }
    }
    
    void DisconnectAllCameras() {
        AddLogMessage("Disconnecting all cameras...", "INFO");
        
        for (auto& camera : current_cameras_) {
            camera.isConnected = false;
            camera.isCapturing = false;
        }
        
        AddLogMessage("All cameras disconnected", "INFO");
    }
    
    void SetParameter(const std::string& param, const std::string& value) {
        AddLogMessage("Setting parameter " + param + " = " + value, "INFO");
        
        try {
            AddLogMessage("Parameter " + param + " set to " + value, "INFO");
        } catch (const std::exception& e) {
            AddLogMessage("Error setting parameter: " + std::string(e.what()), "ERROR");
        }
    }
    
    std::vector<ParameterInfo> GetParameters() {
        std::vector<ParameterInfo> params;
        
        // Example parameters
        ParameterInfo exposure;
        exposure.name = "ExposureTime";
        exposure.description = "Camera exposure time";
        exposure.unit = "μs";
        exposure.minValue = 1000;
        exposure.maxValue = 100000;
        exposure.currentValue = 40000;
        exposure.defaultValue = 40000;
        exposure.isReadOnly = false;
        exposure.isSupported = true;
        params.push_back(exposure);
        
        ParameterInfo gain;
        gain.name = "Gain";
        gain.description = "Camera gain";
        gain.unit = "dB";
        gain.minValue = 0;
        gain.maxValue = 20;
        gain.currentValue = 1.0;
        gain.defaultValue = 1.0;
        gain.isReadOnly = false;
        gain.isSupported = true;
        params.push_back(gain);
        
        return params;
    }
    
    void StartCapture() {
        AddLogMessage("Starting capture session...", "INFO");
        
        try {
            current_session_.isActive = true;
            current_session_.timestamp = GetCurrentTimestamp();
            current_session_.captureCount++;
            
            AddLogMessage("Capture session started: " + current_session_.sessionName, "INFO");
        } catch (const std::exception& e) {
            AddLogMessage("Error starting capture: " + std::string(e.what()), "ERROR");
            current_session_.isActive = false;
        }
    }
    
    void StopCapture() {
        AddLogMessage("Stopping capture session...", "INFO");
        
        current_session_.isActive = false;
        
        AddLogMessage("Capture session stopped", "INFO");
    }
    
    void ResetCapture() {
        AddLogMessage("Resetting capture counter...", "INFO");
        
        current_session_.captureCount = 0;
        current_session_.isActive = false;
        
        AddLogMessage("Capture counter reset", "INFO");
    }
    
    void SetCaptureFormat(const std::string& format) {
        AddLogMessage("Setting capture format to: " + format, "INFO");
        
        current_session_.format = format;
        
        AddLogMessage("Capture format set to " + format, "INFO");
    }
    
    void SetOutputPath(const std::string& path) {
        AddLogMessage("Setting output path to: " + path, "INFO");
        
        current_session_.outputPath = path;
        
        AddLogMessage("Output path set to " + path, "INFO");
    }
    
    void SetSessionName(const std::string& name) {
        AddLogMessage("Setting session name to: " + name, "INFO");
        
        current_session_.sessionName = name;
        
        AddLogMessage("Session name set to " + name, "INFO");
    }
    
    void AddLogMessage(const std::string& message, const std::string& level) {
        std::string timestamp = GetCurrentTimestamp();
        std::string formatted_message = "[" + timestamp + "] " + level + ": " + message;
        
        log_messages_.emplace_back(formatted_message, level);
        
        // Keep only last 1000 messages to prevent memory issues
        if (log_messages_.size() > 1000) {
            log_messages_.erase(log_messages_.begin());
        }
        
        // Also print to console
        std::cout << formatted_message << std::endl;
    }
    
    std::string GetCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        
        return ss.str();
    }
};

int main() {
    std::cout << "🎬 Neural Rendering Capture System - GUI Version" << std::endl;
    std::cout << "=================================================" << std::endl;
    
    try {
        NeuralCaptureGUIApp app;
        
        if (!app.Initialize()) {
            std::cerr << "Failed to initialize application" << std::endl;
            return -1;
        }
        
        app.Run();
        app.Shutdown();
        
        std::cout << "Application terminated successfully" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Application error: " << e.what() << std::endl;
        return -1;
    }
} 