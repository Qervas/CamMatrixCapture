/**
 * main.cpp - Neural Rendering Multi-Camera Capture System
 * Supports 12 Nano-C4020 cameras for high-quality dataset generation
 */

#include "hardware/CameraTypes.hpp"
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

// Include Sapera headers directly
#include "SapClassBasic.h"

using namespace SaperaCapturePro;

enum class CaptureFormat {
    TIFF,
    RAW
};

struct CaptureSession {
    std::string sessionName;
    int captureNumber;
    CaptureFormat format;
    std::string outputPath;
    std::chrono::system_clock::time_point timestamp;
};

struct ConnectedCamera {
    CameraInfo info;
    SapAcqDevice* acqDevice = nullptr;
    SapBuffer* buffer = nullptr;
    SapAcqDeviceToBuf* transfer = nullptr;
    bool connected = false;
    bool captureReady = false;
    int cameraIndex = 0; // For neural rendering dataset naming (cam_01, cam_02, etc.)
};

// Enhanced camera parameter controller using Sapera SDK
class ParameterController {
private:
    std::map<std::string, ConnectedCamera*> cameras_;
    
    // Common parameter definitions
    struct ParameterInfo {
        std::string name;
        std::string description;
        double minValue;
        double maxValue;
        double defaultValue;
        std::string unit;
        bool isReadOnly;
    };
    
    // Parameter definitions for Nano-C4020 cameras
    std::map<std::string, ParameterInfo> parameterDefinitions_ = {
        {"ExposureTime", {"ExposureTime", "Exposure time", 1000, 100000, 40000, "μs", false}},
        {"Gain", {"Gain", "Analog gain", 1.0, 10.0, 1.0, "dB", false}},
        {"BlackLevel", {"BlackLevel", "Black level offset", 0, 255, 0, "counts", false}},
        {"Gamma", {"Gamma", "Gamma correction", 0.1, 3.0, 1.0, "", false}},
        {"OffsetX", {"OffsetX", "Horizontal offset", 0, 1024, 0, "pixels", false}},
        {"OffsetY", {"OffsetY", "Vertical offset", 0, 768, 0, "pixels", false}},
        {"Width", {"Width", "Image width", 64, 4112, 4112, "pixels", false}},
        {"Height", {"Height", "Image height", 64, 3008, 3008, "pixels", false}},
        {"PixelFormat", {"PixelFormat", "Pixel format", 0, 0, 0, "", false}},
        {"TriggerMode", {"TriggerMode", "Trigger mode", 0, 0, 0, "", false}},
        {"TriggerSource", {"TriggerSource", "Trigger source", 0, 0, 0, "", false}},
        {"AcquisitionMode", {"AcquisitionMode", "Acquisition mode", 0, 0, 0, "", false}},
        {"DeviceTemperature", {"DeviceTemperature", "Device temperature", -40, 85, 25, "°C", true}},
        {"DeviceSerialNumber", {"DeviceSerialNumber", "Device serial number", 0, 0, 0, "", true}},
        {"DeviceModelName", {"DeviceModelName", "Device model name", 0, 0, 0, "", true}},
        {"DeviceVendorName", {"DeviceVendorName", "Device vendor name", 0, 0, 0, "", true}},
        {"DeviceVersion", {"DeviceVersion", "Device version", 0, 0, 0, "", true}},
        {"SensorWidth", {"SensorWidth", "Sensor width", 0, 0, 0, "pixels", true}},
        {"SensorHeight", {"SensorHeight", "Sensor height", 0, 0, 0, "pixels", true}},
        {"AcquisitionFrameRate", {"AcquisitionFrameRate", "Frame rate", 0.1, 30.0, 1.0, "fps", false}},
        {"WhiteBalanceRed", {"WhiteBalanceRed", "White balance red", 0.1, 4.0, 1.0, "", false}},
        {"WhiteBalanceGreen", {"WhiteBalanceGreen", "White balance green", 0.1, 4.0, 1.0, "", false}},
        {"WhiteBalanceBlue", {"WhiteBalanceBlue", "White balance blue", 0.1, 4.0, 1.0, "", false}}
    };
    
public:
    void setCameras(std::map<std::string, ConnectedCamera>& cameras) {
        cameras_.clear();
        for (auto& [id, camera] : cameras) {
            cameras_[id] = &camera;
        }
    }
    
    // Get parameter value from a specific camera
    bool getParameter(const std::string& cameraId, const std::string& paramName, std::string& value) {
        auto it = cameras_.find(cameraId);
        if (it == cameras_.end() || !it->second->acqDevice) {
            return false;
        }
        
        try {
            char buffer[512];
            if (it->second->acqDevice->GetFeatureValue(paramName.c_str(), buffer, sizeof(buffer))) {
                value = buffer;
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Exception getting parameter " << paramName << ": " << e.what() << std::endl;
        }
        return false;
    }
    
    // Set parameter value on a specific camera
    bool setParameter(const std::string& cameraId, const std::string& paramName, const std::string& value) {
        auto it = cameras_.find(cameraId);
        if (it == cameras_.end() || !it->second->acqDevice) {
            return false;
        }
        
        try {
            if (it->second->acqDevice->SetFeatureValue(paramName.c_str(), value.c_str())) {
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "❌ Exception setting parameter " << paramName << ": " << e.what() << std::endl;
        }
        return false;
    }
    
    // Set parameter on all cameras
    bool setParameterAll(const std::string& paramName, const std::string& value) {
        bool success = true;
        int successCount = 0;
        
        for (auto& [cameraId, camera] : cameras_) {
            if (setParameter(cameraId, paramName, value)) {
                successCount++;
            } else {
                success = false;
            }
        }
        
        std::cout << "📝 Parameter '" << paramName << "' set to '" << value << "' on " 
                  << successCount << "/" << cameras_.size() << " cameras" << std::endl;
        return success;
    }
    
    // Get parameter info
    bool getParameterInfo(const std::string& paramName, ParameterInfo& info) {
        auto it = parameterDefinitions_.find(paramName);
        if (it != parameterDefinitions_.end()) {
            info = it->second;
            return true;
        }
        return false;
    }
    
    // List all available parameters
    void listParameters() {
        std::cout << "\n=== Available Camera Parameters ===" << std::endl;
        std::cout << "Parameter Name           | Description                | Range/Options        | Unit | RW" << std::endl;
        std::cout << "-------------------------|----------------------------|---------------------|------|----" << std::endl;
        
        for (const auto& [name, info] : parameterDefinitions_) {
            std::string range;
            if (info.name == "PixelFormat" || info.name == "TriggerMode" || 
                info.name == "TriggerSource" || info.name == "AcquisitionMode") {
                range = "Enum";
            } else if (info.isReadOnly) {
                range = "Read-only";
            } else {
                range = std::to_string(info.minValue) + " - " + std::to_string(info.maxValue);
            }
            
            std::cout << std::setw(24) << std::left << info.name << " | "
                      << std::setw(26) << std::left << info.description << " | "
                      << std::setw(19) << std::left << range << " | "
                      << std::setw(4) << std::left << info.unit << " | "
                      << (info.isReadOnly ? "R" : "RW") << std::endl;
        }
        std::cout << std::endl;
    }
    
    // Get parameter status from all cameras
    void getParameterStatus(const std::string& paramName) {
        std::cout << "\n=== Parameter Status: " << paramName << " ===" << std::endl;
        
        ParameterInfo info;
        if (getParameterInfo(paramName, info)) {
            std::cout << "Description: " << info.description << std::endl;
            if (!info.isReadOnly) {
                std::cout << "Range: " << info.minValue << " - " << info.maxValue << " " << info.unit << std::endl;
            }
            std::cout << std::endl;
        }
        
        for (const auto& [cameraId, camera] : cameras_) {
            std::string value;
            if (getParameter(cameraId, paramName, value)) {
                std::cout << "📸 " << cameraId << ": " << value << std::endl;
            } else {
                std::cout << "📸 " << cameraId << ": ❌ Failed to read" << std::endl;
            }
        }
        std::cout << std::endl;
    }
    
    // Advanced parameter operations
    bool setROI(int offsetX, int offsetY, int width, int height) {
        std::cout << "📐 Setting ROI: " << offsetX << "," << offsetY << " " << width << "x" << height << std::endl;
        
        bool success = true;
        success &= setParameterAll("OffsetX", std::to_string(offsetX));
        success &= setParameterAll("OffsetY", std::to_string(offsetY));
        success &= setParameterAll("Width", std::to_string(width));
        success &= setParameterAll("Height", std::to_string(height));
        
        if (success) {
            std::cout << "✅ ROI set successfully" << std::endl;
        } else {
            std::cout << "❌ ROI setting failed on some cameras" << std::endl;
        }
        return success;
    }
    
    bool setWhiteBalance(double red, double green, double blue) {
        std::cout << "⚪ Setting white balance: R=" << red << " G=" << green << " B=" << blue << std::endl;
        
        bool success = true;
        success &= setParameterAll("WhiteBalanceRed", std::to_string(red));
        success &= setParameterAll("WhiteBalanceGreen", std::to_string(green));
        success &= setParameterAll("WhiteBalanceBlue", std::to_string(blue));
        
        if (success) {
            std::cout << "✅ White balance set successfully" << std::endl;
        } else {
            std::cout << "❌ White balance setting failed on some cameras" << std::endl;
        }
        return success;
    }
    
    void showCameraInfo() {
        std::cout << "\n=== Camera Information ===" << std::endl;
        
        for (const auto& [cameraId, camera] : cameras_) {
            std::cout << "📸 " << cameraId << ":" << std::endl;
            
            std::vector<std::string> infoParams = {
                "DeviceSerialNumber", "DeviceModelName", "DeviceVendorName", 
                "DeviceVersion", "DeviceTemperature", "SensorWidth", "SensorHeight"
            };
            
            for (const auto& param : infoParams) {
                std::string value;
                if (getParameter(cameraId, param, value)) {
                    std::cout << "  " << param << ": " << value << std::endl;
                }
            }
            std::cout << std::endl;
        }
    }
    
    // Show current camera settings
    void showCurrentSettings() {
        std::cout << "\n=== Current Camera Settings ===" << std::endl;
        
        std::vector<std::string> settingParams = {
            "ExposureTime", "Gain", "BlackLevel", "Gamma", "OffsetX", "OffsetY", 
            "Width", "Height", "PixelFormat", "TriggerMode", "AcquisitionFrameRate"
        };
        
        for (const auto& param : settingParams) {
            std::cout << std::setw(20) << std::left << param << ": ";
            
            std::string firstValue;
            bool allSame = true;
            bool first = true;
            
            for (const auto& [cameraId, camera] : cameras_) {
                std::string value;
                if (getParameter(cameraId, param, value)) {
                    if (first) {
                        firstValue = value;
                        first = false;
                    } else if (value != firstValue) {
                        allSame = false;
                        break;
                    }
                }
            }
            
            if (allSame && !first) {
                std::cout << firstValue;
                ParameterInfo info;
                if (getParameterInfo(param, info)) {
                    std::cout << " " << info.unit;
                }
            } else {
                std::cout << "❌ Inconsistent values";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
};

class NeuralRenderingCaptureSystem {
private:
    std::vector<CameraInfo> discoveredCameras_;
    std::map<std::string, ConnectedCamera> connectedCameras_;
    std::string datasetPath_;
    CaptureFormat currentFormat_;
    int captureCounter_;
    std::string currentSessionName_;
    int exposureTime_; // Exposure time in microseconds
    ParameterController parameterController_; // Enhanced parameter control

public:
    NeuralRenderingCaptureSystem(const std::string& datasetPath = "neural_dataset") 
        : datasetPath_(datasetPath), currentFormat_(CaptureFormat::TIFF), captureCounter_(1), exposureTime_(40000) {
        
        // Ensure dataset directory structure exists
        std::filesystem::create_directories(datasetPath_);
        std::filesystem::create_directories(datasetPath_ + "/images");
        std::filesystem::create_directories(datasetPath_ + "/metadata");
        
        std::cout << "📁 Neural dataset initialized: " << datasetPath_ << std::endl;
        std::cout << "⏱️ Default exposure time: " << exposureTime_ << "μs" << std::endl;
    }

    std::vector<CameraInfo> discoverCameras() {
        discoveredCameras_.clear();
        
        std::cout << "🔍 Discovering cameras for neural rendering..." << std::endl;
        
        // Get server count
        int serverCount = SapManager::GetServerCount();
        std::cout << "Found " << serverCount << " server(s)" << std::endl;
        
        if (serverCount == 0) {
            std::cout << "❌ No Sapera servers found" << std::endl;
            return discoveredCameras_;
        }
        
        int cameraIndex = 1;
        
        // Enumerate all servers
        for (int serverIndex = 0; serverIndex < serverCount; serverIndex++) {
            char serverName[256];
            if (!SapManager::GetServerName(serverIndex, serverName, sizeof(serverName))) {
                std::cout << "❌ Failed to get server name for server " << serverIndex << std::endl;
                continue;
            }
            
            // Skip system server
            if (std::string(serverName) == "System") {
                continue;
            }
            
            std::cout << "🖥️ Server " << serverIndex << ": " << serverName << std::endl;
            
            // Get acquisition device count for this server
            int resourceCount = SapManager::GetResourceCount(serverName, SapManager::ResourceAcqDevice);
            std::cout << "  📸 Acquisition devices: " << resourceCount << std::endl;
            
            // Enumerate acquisition devices
            for (int resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
                try {
                    // Create acquisition device temporarily for discovery
                    auto acqDevice = new SapAcqDevice(serverName, resourceIndex);
                    if (!acqDevice->Create()) {
                        std::cout << "  ❌ Failed to create device " << resourceIndex << std::endl;
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
                    camera.name = "cam_" + std::string(2 - std::to_string(cameraIndex).length(), '0') + std::to_string(cameraIndex);
                    camera.isConnected = false;
                    camera.status = CameraStatus::Disconnected;
                    camera.type = CameraType::Industrial;
                    
                    discoveredCameras_.push_back(camera);
                    std::cout << "  ✅ " << camera.name << ": " << camera.serialNumber 
                             << " (" << camera.modelName << ")" << std::endl;
                    
                    // Cleanup discovery device
                    acqDevice->Destroy();
                    delete acqDevice;
                    
                    cameraIndex++;
                    
                } catch (const std::exception& e) {
                    std::cout << "  ❌ Exception: " << e.what() << std::endl;
                }
            }
        }
        
        std::cout << "✅ Discovery complete: " << discoveredCameras_.size() << " cameras found" << std::endl;
        return discoveredCameras_;
    }
    
    bool connectAllCameras() {
        if (discoveredCameras_.empty()) {
            std::cerr << "❌ No cameras discovered. Run camera discovery first." << std::endl;
            return false;
        }
        
        std::cout << "🔌 Connecting to all discovered cameras..." << std::endl;
        
        int connectedCount = 0;
        for (const auto& camera : discoveredCameras_) {
            if (connectCamera(camera.id)) {
                connectedCount++;
            }
        }
        
        // Initialize parameter controller with connected cameras
        parameterController_.setCameras(connectedCameras_);
        
        std::cout << "✅ Connected " << connectedCount << "/" << discoveredCameras_.size() << " cameras" << std::endl;
        return connectedCount > 0;
    }
    
    bool connectCamera(const std::string& cameraId) {
        // Check if already connected
        if (connectedCameras_.find(cameraId) != connectedCameras_.end()) {
            return true;
        }
        
        // Find camera info
        CameraInfo* cameraInfo = nullptr;
        for (auto& cam : discoveredCameras_) {
            if (cam.id == cameraId) {
                cameraInfo = &cam;
                break;
            }
        }
        
        if (!cameraInfo) {
            return false;
        }
        
        try {
            // Create acquisition device for this camera
            auto acqDevice = new SapAcqDevice(cameraInfo->serverName.c_str(), cameraInfo->resourceIndex);
            if (!acqDevice->Create()) {
                delete acqDevice;
                return false;
            }
            
            // Apply exposure time setting
            if (!applyExposureTime(acqDevice, exposureTime_)) {
                std::cout << "⚠️ Warning: Failed to set exposure time for " << cameraInfo->name << std::endl;
            }
            
            // Create buffer for image capture
            auto buffer = new SapBufferWithTrash(1, acqDevice);
            if (!buffer->Create()) {
                acqDevice->Destroy();
                delete acqDevice;
                delete buffer;
                return false;
            }
            
            // Create transfer object
            auto transfer = new SapAcqDeviceToBuf(acqDevice, buffer);
            if (!transfer->Create()) {
                buffer->Destroy();
                acqDevice->Destroy();
                delete transfer;
                delete buffer;
                delete acqDevice;
                return false;
            }
            
            // Store connected camera with capture objects
            ConnectedCamera connectedCam;
            connectedCam.info = *cameraInfo;
            connectedCam.info.isConnected = true;
            connectedCam.info.status = CameraStatus::Ready;
            connectedCam.acqDevice = acqDevice;
            connectedCam.buffer = buffer;
            connectedCam.transfer = transfer;
            connectedCam.connected = true;
            connectedCam.captureReady = true;
            connectedCam.cameraIndex = std::stoi(cameraId);
            
            connectedCameras_[cameraId] = connectedCam;
            
            // Update the discovered camera status
            cameraInfo->isConnected = true;
            cameraInfo->status = CameraStatus::Ready;
            
            return true;
            
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    // Apply exposure time to a camera
    bool applyExposureTime(SapAcqDevice* acqDevice, int exposureTimeUs) {
        if (!acqDevice) {
            return false;
        }
        
        try {
            // Set exposure time in microseconds
            std::string exposureStr = std::to_string(exposureTimeUs);
            if (!acqDevice->SetFeatureValue("ExposureTime", exposureStr.c_str())) {
                // Try alternative parameter name
                if (!acqDevice->SetFeatureValue("ExposureTimeAbs", exposureStr.c_str())) {
                    return false;
                }
            }
            
            // Wait for parameter to take effect
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            return true;
            
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    // Set exposure time for all connected cameras
    bool setExposureTime(int exposureTimeUs) {
        if (exposureTimeUs < 1000 || exposureTimeUs > 100000) {
            std::cout << "❌ Invalid exposure time. Must be between 1000-100000 μs" << std::endl;
            return false;
        }
        
        exposureTime_ = exposureTimeUs;
        std::cout << "⏱️ Setting exposure time to " << exposureTime_ << "μs for all cameras..." << std::endl;
        
        int successCount = 0;
        for (auto& [cameraId, cam] : connectedCameras_) {
            if (applyExposureTime(cam.acqDevice, exposureTime_)) {
                successCount++;
                std::cout << "  ✅ " << cam.info.name << ": " << exposureTime_ << "μs" << std::endl;
            } else {
                std::cout << "  ❌ " << cam.info.name << ": Failed to set exposure" << std::endl;
            }
        }
        
        std::cout << "🎯 Updated " << successCount << "/" << connectedCameras_.size() << " cameras" << std::endl;
        return successCount == connectedCameras_.size();
    }
    
    // Get current exposure time
    int getExposureTime() const {
        return exposureTime_;
    }
    
    bool captureAllCameras() {
        if (connectedCameras_.empty()) {
            std::cout << "❌ No cameras connected" << std::endl;
            return false;
        }
        
        currentSessionName_ = generateSessionName(captureCounter_);
        
        std::cout << "📸 Starting capture session #" << captureCounter_ << std::endl;
        std::cout << "🎬 Session: " << currentSessionName_ << std::endl;
        
        // Create session directory
        std::string sessionPath = datasetPath_ + "/images/" + currentSessionName_;
        std::filesystem::create_directories(sessionPath);
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Capture all cameras simultaneously
        std::vector<std::thread> captureThreads;
        std::vector<bool> captureResults(connectedCameras_.size(), false);
        
        int threadIndex = 0;
        for (auto& [cameraId, cam] : connectedCameras_) {
            captureThreads.emplace_back([&, threadIndex, cameraId]() {
                captureResults[threadIndex] = captureSingleCamera(cameraId, sessionPath);
            });
            threadIndex++;
        }
        
        // Wait for all captures to complete
        for (auto& thread : captureThreads) {
            thread.join();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Check results
        int successCount = 0;
        for (bool result : captureResults) {
            if (result) successCount++;
        }
        
        std::cout << "🎯 Capture complete: " << successCount << "/" << connectedCameras_.size() 
                  << " cameras (" << duration.count() << "ms)" << std::endl;
        
        // Save metadata
        saveSessionMetadata(currentSessionName_, captureCounter_, successCount == connectedCameras_.size());
        
        if (successCount == connectedCameras_.size()) {
            captureCounter_++;
            std::cout << "✅ All cameras captured successfully! Ready for next capture." << std::endl;
        }
        
        return successCount == connectedCameras_.size();
    }
    
    bool captureSingleCamera(const std::string& cameraId, const std::string& sessionPath) {
        auto it = connectedCameras_.find(cameraId);
        if (it == connectedCameras_.end() || !it->second.captureReady) {
            return false;
        }
        
        ConnectedCamera& cam = it->second;
        
        try {
            // 1. Snap (trigger single capture) - NOT Grab!
            if (!cam.transfer->Snap()) {
                std::cerr << "❌ Snap failed for " << cam.info.name << std::endl;
                return false;
            }
            
            // 2. Wait for capture to complete - CRITICAL!
            if (!cam.transfer->Wait(10000)) {  // 10 second timeout
                std::cerr << "❌ Wait timeout for " << cam.info.name << std::endl;
                cam.transfer->Abort();
                return false;
            }
            
            // 3. Generate filename (already has correct extension based on format)
            std::string filename = generateImageFilename(cam.info.name, captureCounter_);
            std::string fullPath = sessionPath + "/" + filename;
            
            bool saveSuccess = false;
            
            if (currentFormat_ == CaptureFormat::RAW) {
                // RAW FORMAT: Use Sapera SDK's FileFormatRAW directly
                // This saves the original sensor data without any processing
                saveSuccess = cam.buffer->Save(fullPath.c_str(), "-format raw");
                
                if (saveSuccess) {
                    std::cout << "💾 Saved RAW format: " << fullPath << std::endl;
                } else {
                    std::cerr << "❌ Failed to save RAW format: " << fullPath << std::endl;
                }
                
            } else {
                // TIFF FORMAT: Apply color conversion to get RGB image
                
                // Create color converter for TIFF format
                SapColorConversion colorConverter(cam.buffer);
                if (!colorConverter.Create()) {
                    std::cerr << "❌ Failed to create color converter for " << cam.info.name << std::endl;
                    return false;
                }
                
                // Configure converter for RGB output
                colorConverter.Enable(TRUE, FALSE);
                colorConverter.SetOutputFormat(SapFormatRGB888);
                colorConverter.SetAlign(SapColorConversion::AlignRGGB);
                
                // Convert the image to RGB
                if (!colorConverter.Convert()) {
                    std::cerr << "❌ Color conversion failed for " << cam.info.name << std::endl;
                    colorConverter.Destroy();
                    return false;
                }
                
                // Get converted RGB buffer
                SapBuffer* outputBuffer = colorConverter.GetOutputBuffer();
                if (!outputBuffer) {
                    std::cerr << "❌ No output buffer for " << cam.info.name << std::endl;
                    colorConverter.Destroy();
                    return false;
                }
                
                // Save converted RGB buffer as TIFF
                saveSuccess = outputBuffer->Save(fullPath.c_str(), "-format tiff");
                
                std::cout << "💾 Saved TIFF (RGB converted): " << fullPath << std::endl;
                
                // Clean up converter
                colorConverter.Destroy();
            }
            
            if (!saveSuccess) {
                std::cerr << "❌ Failed to save " << fullPath << std::endl;
                return false;
            }
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Exception in " << cam.info.name << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    void setFormat(CaptureFormat format) {
        currentFormat_ = format;
        std::cout << "📷 Format set to: " << (format == CaptureFormat::TIFF ? "TIFF" : "RAW") << std::endl;
    }
    
    void setDatasetPath(const std::string& path) {
        datasetPath_ = path;
        std::cout << "📁 Dataset path set to: " << datasetPath_ << std::endl;
    }
    
    void resetCaptureCounter() {
        captureCounter_ = 1;
        std::cout << "🔄 Capture counter reset to 1" << std::endl;
    }
    
    void printCameraStatus() {
        std::cout << "\n=== Multi-Camera Neural Rendering System Status ===" << std::endl;
        std::cout << "📁 Dataset: " << datasetPath_ << std::endl;
        std::cout << "📷 Format: " << (currentFormat_ == CaptureFormat::TIFF ? "TIFF" : "RAW") << std::endl;
        std::cout << "⏱️ Exposure: " << exposureTime_ << "μs" << std::endl;
        std::cout << "🎯 Cameras: " << connectedCameras_.size() << "/" << discoveredCameras_.size() << " connected" << std::endl;
        std::cout << "📸 Next capture: #" << captureCounter_ << std::endl;
        
        if (discoveredCameras_.empty()) {
            std::cout << "No cameras discovered" << std::endl;
            return;
        }
        
        for (const auto& camera : discoveredCameras_) {
            std::cout << "📸 " << camera.name << " (" << camera.serialNumber << "): " 
                      << (camera.isConnected ? "🟢 Ready" : "🔴 Disconnected") << std::endl;
        }
    }
    
    void runInteractiveSession() {
        std::cout << "\n🎬 Starting Interactive Neural Rendering Capture Session" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  capture              - Capture all cameras (current position)" << std::endl;
        std::cout << "  format tiff|raw      - Switch capture format" << std::endl;
        std::cout << "  exposure <time>      - Set exposure time in microseconds (1000-100000)" << std::endl;
        std::cout << "  reset                - Reset capture counter to 1" << std::endl;
        std::cout << "  status               - Show system status" << std::endl;
        std::cout << "  param list           - List all available parameters" << std::endl;
        std::cout << "  param get <name>     - Get parameter value from all cameras" << std::endl;
        std::cout << "  param set <name> <value> - Set parameter value on all cameras" << std::endl;
        std::cout << "  param info <name>    - Show parameter information" << std::endl;
        std::cout << "  gain <value>         - Set gain on all cameras (1.0-10.0)" << std::endl;
        std::cout << "  roi <x> <y> <w> <h>  - Set region of interest" << std::endl;
        std::cout << "  wb <r> <g> <b>       - Set white balance (0.1-4.0)" << std::endl;
        std::cout << "  show settings        - Show current camera settings" << std::endl;
        std::cout << "  show info            - Show camera hardware information" << std::endl;
        std::cout << "  help                 - Show this help" << std::endl;
        std::cout << "  quit                 - Exit application" << std::endl;
        std::cout << std::endl;
        std::cout << "💡 Workflow: Manually position object → type 'capture' → repeat" << std::endl;
        std::cout << std::endl;
        
        std::string command;
        while (true) {
            std::cout << "neural_capture> ";
            std::getline(std::cin, command);
            
            if (command.empty()) continue;
            
            std::istringstream iss(command);
            std::string cmd;
            iss >> cmd;
            
            if (cmd == "quit" || cmd == "exit") {
                break;
            } else if (cmd == "capture") {
                std::cout << "📸 Capturing all cameras at current object position..." << std::endl;
                captureAllCameras();
            } else if (cmd == "format") {
                std::string format;
                if (iss >> format) {
                    if (format == "tiff") {
                        setFormat(CaptureFormat::TIFF);
                    } else if (format == "raw") {
                        setFormat(CaptureFormat::RAW);
                    } else {
                        std::cout << "❌ Invalid format. Use 'tiff' or 'raw'" << std::endl;
                    }
                } else {
                    std::cout << "❌ Usage: format <tiff|raw>" << std::endl;
                }
            } else if (cmd == "exposure") {
                int newExposure;
                if (iss >> newExposure) {
                    setExposureTime(newExposure);
                } else {
                    std::cout << "⏱️ Current exposure time: " << getExposureTime() << "μs" << std::endl;
                    std::cout << "❌ Usage: exposure <time_in_microseconds>" << std::endl;
                    std::cout << "   Example: exposure 40000" << std::endl;
                }
            } else if (cmd == "reset") {
                resetCaptureCounter();
            } else if (cmd == "status") {
                printCameraStatus();
            } else if (cmd == "param") {
                std::string subCmd;
                if (iss >> subCmd) {
                    if (subCmd == "list") {
                        parameterController_.listParameters();
                    } else if (subCmd == "get") {
                        std::string paramName;
                        if (iss >> paramName) {
                            parameterController_.getParameterStatus(paramName);
                        } else {
                            std::cout << "❌ Usage: param get <parameter_name>" << std::endl;
                        }
                    } else if (subCmd == "set") {
                        std::string paramName, value;
                        if (iss >> paramName >> value) {
                            parameterController_.setParameterAll(paramName, value);
                        } else {
                            std::cout << "❌ Usage: param set <parameter_name> <value>" << std::endl;
                        }
                    } else if (subCmd == "info") {
                        std::string paramName;
                        if (iss >> paramName) {
                            parameterController_.getParameterStatus(paramName);
                        } else {
                            std::cout << "❌ Usage: param info <parameter_name>" << std::endl;
                        }
                    } else {
                        std::cout << "❌ Unknown param command. Use: list, get, set, info" << std::endl;
                    }
                } else {
                    std::cout << "❌ Usage: param <list|get|set|info>" << std::endl;
                }
            } else if (cmd == "gain") {
                double gainValue;
                if (iss >> gainValue) {
                    if (gainValue >= 1.0 && gainValue <= 10.0) {
                        parameterController_.setParameterAll("Gain", std::to_string(gainValue));
                    } else {
                        std::cout << "❌ Gain must be between 1.0 and 10.0" << std::endl;
                    }
                } else {
                    parameterController_.getParameterStatus("Gain");
                }
            } else if (cmd == "roi") {
                int x, y, w, h;
                if (iss >> x >> y >> w >> h) {
                    parameterController_.setROI(x, y, w, h);
                } else {
                    std::cout << "❌ Usage: roi <x> <y> <width> <height>" << std::endl;
                    std::cout << "   Example: roi 0 0 4112 3008" << std::endl;
                }
            } else if (cmd == "wb") {
                double r, g, b;
                if (iss >> r >> g >> b) {
                    parameterController_.setWhiteBalance(r, g, b);
                } else {
                    std::cout << "❌ Usage: wb <red> <green> <blue>" << std::endl;
                    std::cout << "   Example: wb 1.2 1.0 1.5" << std::endl;
                }
            } else if (cmd == "show") {
                std::string what;
                if (iss >> what) {
                    if (what == "settings") {
                        parameterController_.showCurrentSettings();
                    } else if (what == "info") {
                        parameterController_.showCameraInfo();
                    } else {
                        std::cout << "❌ Usage: show <settings|info>" << std::endl;
                    }
                } else {
                    std::cout << "❌ Usage: show <settings|info>" << std::endl;
                }
            } else if (cmd == "help") {
                std::cout << "Available commands:" << std::endl;
                std::cout << "  capture              - Capture all cameras (current position)" << std::endl;
                std::cout << "  format tiff|raw      - Switch capture format" << std::endl;
                std::cout << "  exposure <time>      - Set exposure time in microseconds (1000-100000)" << std::endl;
                std::cout << "  reset                - Reset capture counter to 1" << std::endl;
                std::cout << "  status               - Show system status" << std::endl;
                std::cout << "  param list           - List all available parameters" << std::endl;
                std::cout << "  param get <name>     - Get parameter value from all cameras" << std::endl;
                std::cout << "  param set <name> <value> - Set parameter value on all cameras" << std::endl;
                std::cout << "  param info <name>    - Show parameter information" << std::endl;
                std::cout << "  gain <value>         - Set gain on all cameras (1.0-10.0)" << std::endl;
                std::cout << "  roi <x> <y> <w> <h>  - Set region of interest" << std::endl;
                std::cout << "  wb <r> <g> <b>       - Set white balance (0.1-4.0)" << std::endl;
                std::cout << "  show settings        - Show current camera settings" << std::endl;
                std::cout << "  show info            - Show camera hardware information" << std::endl;
                std::cout << "  help                 - Show this help" << std::endl;
                std::cout << "  quit                 - Exit application" << std::endl;
                std::cout << std::endl;
                std::cout << "💡 Workflow: Manually position object → type 'capture' → repeat" << std::endl;
                std::cout << "💡 Parameter Tips:" << std::endl;
                std::cout << "   - Use 'param list' to see all available parameters" << std::endl;
                std::cout << "   - Use 'show settings' to see current camera configuration" << std::endl;
                std::cout << "   - Exposure: 40000μs default (40ms)" << std::endl;
                std::cout << "   - Gain: 1.0-10.0 (1.0 = no gain)" << std::endl;
                std::cout << "   - ROI: Full sensor is 4112x3008" << std::endl;
            } else {
                std::cout << "❌ Unknown command. Type 'help' for available commands." << std::endl;
            }
        }
    }
    
    ~NeuralRenderingCaptureSystem() {
        // Clean up all connected cameras
        for (auto& [cameraId, cam] : connectedCameras_) {
            if (cam.transfer) {
                cam.transfer->Destroy();
                delete cam.transfer;
            }
            if (cam.buffer) {
                cam.buffer->Destroy();
                delete cam.buffer;
            }
            if (cam.acqDevice) {
                cam.acqDevice->Destroy();
                delete cam.acqDevice;
            }
        }
        connectedCameras_.clear();
    }

private:
    std::string generateSessionName(int captureNumber) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        std::ostringstream oss;
        oss << "capture_" << std::setfill('0') << std::setw(3) << captureNumber 
            << "_" << std::put_time(&tm, "%Y%m%d_%H%M%S");
        return oss.str();
    }
    
    std::string generateImageFilename(const std::string& cameraName, int captureNumber) {
        std::ostringstream oss;
        oss << cameraName << "_capture_" << std::setfill('0') << std::setw(3) << captureNumber;
        
        // Add appropriate extension based on format (Sapera SDK standard)
        if (currentFormat_ == CaptureFormat::RAW) {
            oss << ".raw";   // Sapera SDK RAW format
        } else {
            oss << ".tiff";  // Color-processed TIFF
        }
        
        return oss.str();
    }
    
    void saveSessionMetadata(const std::string& sessionName, int captureNumber, bool success) {
        std::string metadataPath = datasetPath_ + "/metadata/" + sessionName + ".json";
        
        // Create simple metadata file
        std::ofstream metadataFile(metadataPath);
        if (metadataFile.is_open()) {
            metadataFile << "{\n";
            metadataFile << "  \"session_name\": \"" << sessionName << "\",\n";
            metadataFile << "  \"capture_number\": " << captureNumber << ",\n";
            metadataFile << "  \"capture_success\": " << (success ? "true" : "false") << ",\n";
            metadataFile << "  \"camera_count\": " << connectedCameras_.size() << ",\n";
            metadataFile << "  \"format\": \"" << (currentFormat_ == CaptureFormat::TIFF ? "TIFF" : "RAW") << "\",\n";
            metadataFile << "  \"timestamp\": \"" << std::time(nullptr) << "\",\n";
            metadataFile << "  \"cameras\": [\n";
            
            bool first = true;
            for (const auto& [cameraId, cam] : connectedCameras_) {
                if (!first) metadataFile << ",\n";
                metadataFile << "    {\n";
                metadataFile << "      \"id\": \"" << cameraId << "\",\n";
                metadataFile << "      \"name\": \"" << cam.info.name << "\",\n";
                metadataFile << "      \"serial\": \"" << cam.info.serialNumber << "\",\n";
                metadataFile << "      \"server\": \"" << cam.info.serverName << "\"\n";
                metadataFile << "    }";
                first = false;
            }
            
            metadataFile << "\n  ]\n";
            metadataFile << "}\n";
            metadataFile.close();
        }
    }
};

void printBanner() {
    std::cout << R"(
🎬 ================================================= 🎬
   Neural Rendering Multi-Camera Capture System
   12 x Nano-C4020 Cameras • 4112x3008 Resolution
   Manual Rotation Workflow
🎬 ================================================= 🎬
)" << std::endl;
}

int main() {
    printBanner();
    
    try {
        NeuralRenderingCaptureSystem captureSystem("neural_dataset");
        
        // 1. Discover cameras
        std::cout << "Step 1: Camera Discovery" << std::endl;
        auto cameras = captureSystem.discoverCameras();
        
        if (cameras.empty()) {
            std::cout << "❌ No cameras found. Check your Sapera installation and camera connections." << std::endl;
            return 1;
        }
        
        // 2. Connect to all cameras
        std::cout << "\nStep 2: Multi-Camera Connection" << std::endl;
        if (!captureSystem.connectAllCameras()) {
            std::cout << "⚠️ Some cameras failed to connect. Continuing with available cameras..." << std::endl;
        }
        
        // 3. Show status
        captureSystem.printCameraStatus();
        
        // 4. Start interactive session
        std::cout << "\nStep 3: Interactive Capture Session" << std::endl;
        captureSystem.runInteractiveSession();
        
        std::cout << "\n🎉 Neural rendering capture session completed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Unhandled exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "❌ Unknown error occurred" << std::endl;
        return 1;
    }
    
    return 0;
} 