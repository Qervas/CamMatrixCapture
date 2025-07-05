/**
 * Simple Camera Discovery and Listing Test
 * Based on the working single-file approach
 */

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>

// Include Sapera headers directly
#include "sapclassbasic.h"

struct SimpleCameraInfo {
    std::string serverName;
    int resourceIndex;
    std::string serialNumber;
    std::string modelName;
    std::string deviceVersion;
    bool isConnected = false;
    int width = 0;
    int height = 0;
    SapAcqDevice* acqDevice = nullptr;
};

class SimpleCameraDiscovery {
private:
    std::vector<SimpleCameraInfo> discoveredCameras_;

public:
    bool discoverCameras() {
        std::cout << "🔍 Discovering cameras using Sapera SDK..." << std::endl;
        
        discoveredCameras_.clear();
        
        // Get server count
        int serverCount = SapManager::GetServerCount();
        std::cout << "Found " << serverCount << " server(s)" << std::endl;
        
        if (serverCount == 0) {
            std::cout << "⚠️  No Sapera servers found. Check Sapera installation." << std::endl;
            return false;
        }
        
        // Enumerate all servers
        for (int serverIndex = 0; serverIndex < serverCount; serverIndex++) {
            char serverName[256];
            if (!SapManager::GetServerName(serverIndex, serverName, sizeof(serverName))) {
                std::cout << "❌ Failed to get server name for index " << serverIndex << std::endl;
                continue;
            }
            
            std::cout << "\nServer " << serverIndex << ": " << serverName << std::endl;
            
            // Get acquisition device count for this server
            int resourceCount = SapManager::GetResourceCount(serverName, SapManager::ResourceAcqDevice);
            std::cout << "  Acquisition devices: " << resourceCount << std::endl;
            
            // Enumerate acquisition devices
            for (int resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++) {
                SimpleCameraInfo cameraInfo = initializeCamera(serverName, resourceIndex);
                if (cameraInfo.isConnected) {
                    discoveredCameras_.push_back(cameraInfo);
                    std::cout << "  ✅ Camera " << resourceIndex << ": " << cameraInfo.serialNumber 
                             << " (" << cameraInfo.modelName << ")" << std::endl;
                } else {
                    std::cout << "  ❌ Failed to initialize camera " << resourceIndex << std::endl;
                }
            }
        }
        
        std::cout << "\n📊 Discovery Summary: " << discoveredCameras_.size() << " camera(s) found" << std::endl;
        return !discoveredCameras_.empty();
    }
    
    SimpleCameraInfo initializeCamera(const char* serverName, int resourceIndex) {
        SimpleCameraInfo info;
        info.serverName = serverName;
        info.resourceIndex = resourceIndex;
        
        try {
            // Create acquisition device
            auto acqDevice = new SapAcqDevice(serverName, resourceIndex);
            if (!acqDevice->Create()) {
                std::cout << "    ❌ Failed to create acquisition device" << std::endl;
                delete acqDevice;
                return info;
            }
            
            // Get device information
            char buffer[512];
            
            // Get serial number
            if (acqDevice->GetFeatureValue("DeviceSerialNumber", buffer, sizeof(buffer))) {
                info.serialNumber = std::string(buffer);
            } else {
                std::cout << "    ⚠️  Could not read serial number" << std::endl;
                info.serialNumber = "Unknown_" + std::to_string(resourceIndex);
            }
            
            // Get model name
            if (acqDevice->GetFeatureValue("DeviceModelName", buffer, sizeof(buffer))) {
                info.modelName = std::string(buffer);
            } else {
                info.modelName = "Unknown_Model";
            }
            
            // Get device version
            if (acqDevice->GetFeatureValue("DeviceVersion", buffer, sizeof(buffer))) {
                info.deviceVersion = std::string(buffer);
            } else {
                info.deviceVersion = "Unknown_Version";
            }
            
            // Get image dimensions
            if (acqDevice->GetFeatureValue("Width", buffer, sizeof(buffer))) {
                info.width = std::atoi(buffer);
            }
            if (acqDevice->GetFeatureValue("Height", buffer, sizeof(buffer))) {
                info.height = std::atoi(buffer);
            }
            
            info.acqDevice = acqDevice;
            info.isConnected = true;
            
            std::cout << "    📋 Details: " << info.serialNumber 
                     << ", " << info.modelName 
                     << ", " << info.width << "x" << info.height << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "    💥 Exception during camera initialization: " << e.what() << std::endl;
        }
        
        return info;
    }
    
    void listCameras() {
        if (discoveredCameras_.empty()) {
            std::cout << "No cameras available. Run discovery first." << std::endl;
            return;
        }
        
        std::cout << "\n📷 Available Cameras:" << std::endl;
        std::cout << "===========================================" << std::endl;
        
        for (size_t i = 0; i < discoveredCameras_.size(); i++) {
            const auto& camera = discoveredCameras_[i];
            
            std::cout << "\nCamera #" << (i + 1) << ":" << std::endl;
            std::cout << "  Server: " << camera.serverName << std::endl;
            std::cout << "  Resource Index: " << camera.resourceIndex << std::endl;
            std::cout << "  Serial Number: " << camera.serialNumber << std::endl;
            std::cout << "  Model: " << camera.modelName << std::endl;
            std::cout << "  Version: " << camera.deviceVersion << std::endl;
            std::cout << "  Resolution: " << camera.width << " x " << camera.height << std::endl;
            std::cout << "  Status: " << (camera.isConnected ? "Connected ✅" : "Disconnected ❌") << std::endl;
            
            // Try to get additional camera features
            if (camera.acqDevice && camera.isConnected) {
                printCameraFeatures(camera);
            }
        }
    }
    
    void printCameraFeatures(const SimpleCameraInfo& camera) {
        if (!camera.acqDevice) return;
        
        std::cout << "  Additional Features:" << std::endl;
        
        char buffer[256];
        
        // Exposure capabilities
        if (camera.acqDevice->GetFeatureValue("ExposureTime", buffer, sizeof(buffer))) {
            std::cout << "    Current Exposure: " << buffer << " μs" << std::endl;
        }
        
        // Gain capabilities
        if (camera.acqDevice->GetFeatureValue("Gain", buffer, sizeof(buffer))) {
            std::cout << "    Current Gain: " << buffer << std::endl;
        }
        
        // Pixel format
        if (camera.acqDevice->GetFeatureValue("PixelFormat", buffer, sizeof(buffer))) {
            std::cout << "    Pixel Format: " << buffer << std::endl;
        }
        
        // Device temperature (if available)
        if (camera.acqDevice->GetFeatureValue("DeviceTemperature", buffer, sizeof(buffer))) {
            std::cout << "    Temperature: " << buffer << "°C" << std::endl;
        }
        
        // Acquisition mode
        if (camera.acqDevice->GetFeatureValue("AcquisitionMode", buffer, sizeof(buffer))) {
            std::cout << "    Acquisition Mode: " << buffer << std::endl;
        }
    }
    
    void listCamerasJSON() {
        std::cout << "{\n";
        std::cout << "  \"status\": \"success\",\n";
        std::cout << "  \"cameras\": [\n";
        
        for (size_t i = 0; i < discoveredCameras_.size(); i++) {
            const auto& camera = discoveredCameras_[i];
            
            std::cout << "    {\n";
            std::cout << "      \"id\": \"" << (i + 1) << "\",\n";
            std::cout << "      \"serial\": \"" << camera.serialNumber << "\",\n";
            std::cout << "      \"model\": \"" << camera.modelName << "\",\n";
            std::cout << "      \"version\": \"" << camera.deviceVersion << "\",\n";
            std::cout << "      \"server\": \"" << camera.serverName << "\",\n";
            std::cout << "      \"resource_index\": " << camera.resourceIndex << ",\n";
            std::cout << "      \"resolution\": {\n";
            std::cout << "        \"width\": " << camera.width << ",\n";
            std::cout << "        \"height\": " << camera.height << "\n";
            std::cout << "      },\n";
            std::cout << "      \"connected\": " << (camera.isConnected ? "true" : "false") << "\n";
            std::cout << "    }";
            
            if (i < discoveredCameras_.size() - 1) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        
        std::cout << "  ],\n";
        std::cout << "  \"total_cameras\": " << discoveredCameras_.size() << "\n";
        std::cout << "}\n";
    }
    
    void testSingleCapture() {
        if (discoveredCameras_.empty()) {
            std::cout << "No cameras available for capture test." << std::endl;
            return;
        }
        
        auto& camera = discoveredCameras_[0];
        if (!camera.acqDevice) {
            std::cout << "Camera not properly initialized." << std::endl;
            return;
        }
        
        std::cout << "\n🧪 Testing capture from first camera: " << camera.serialNumber << std::endl;
        
        try {
            // Create buffer
            auto buffer = new SapBufferWithTrash(1, camera.acqDevice);
            if (!buffer->Create()) {
                std::cout << "❌ Failed to create buffer" << std::endl;
                delete buffer;
                return;
            }
            
            // Create transfer
            auto transfer = new SapAcqDeviceToBuf(camera.acqDevice, buffer);
            if (!transfer->Create()) {
                std::cout << "❌ Failed to create transfer" << std::endl;
                buffer->Destroy();
                delete buffer;
                delete transfer;
                return;
            }
            
            std::cout << "📸 Capturing image..." << std::endl;
            
            // Snap a single frame
            if (!transfer->Snap()) {
                std::cout << "❌ Snap failed" << std::endl;
            } else {
                // Wait for capture
                if (transfer->Wait(5000)) {
                    std::cout << "✅ Capture successful!" << std::endl;
                    
                    // Try to save the image
                    std::string filename = "test_capture_" + camera.serialNumber + ".tiff";
                    if (buffer->Save(filename.c_str(), "-format tiff")) {
                        std::cout << "💾 Image saved: " << filename << std::endl;
                    } else {
                        std::cout << "⚠️  Capture successful but save failed" << std::endl;
                    }
                } else {
                    std::cout << "❌ Capture timeout" << std::endl;
                    transfer->Abort();
                }
            }
            
            // Cleanup
            transfer->Destroy();
            buffer->Destroy();
            delete transfer;
            delete buffer;
            
        } catch (const std::exception& e) {
            std::cout << "💥 Exception during capture test: " << e.what() << std::endl;
        }
    }
    
    void cleanup() {
        std::cout << "\n🧹 Cleaning up cameras..." << std::endl;
        
        for (auto& camera : discoveredCameras_) {
            if (camera.acqDevice) {
                camera.acqDevice->Destroy();
                delete camera.acqDevice;
                camera.acqDevice = nullptr;
            }
        }
        
        discoveredCameras_.clear();
        std::cout << "✅ Cleanup complete" << std::endl;
    }
    
    const std::vector<SimpleCameraInfo>& getCameras() const {
        return discoveredCameras_;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "🎬 Simple Camera Discovery Test" << std::endl;
    std::cout << "Based on working single-file approach" << std::endl;
    std::cout << "====================================" << std::endl;
    
    bool jsonOutput = false;
    bool testCapture = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--json") {
            jsonOutput = true;
        } else if (arg == "--test-capture") {
            testCapture = true;
        }
    }
    
    SimpleCameraDiscovery discovery;
    
    try {
        // Discover cameras
        if (!discovery.discoverCameras()) {
            if (jsonOutput) {
                std::cout << R"({"status": "error", "message": "No cameras found"})" << std::endl;
            } else {
                std::cout << "❌ No cameras found or discovery failed" << std::endl;
            }
            return 1;
        }
        
        // Output results
        if (jsonOutput) {
            discovery.listCamerasJSON();
        } else {
            discovery.listCameras();
            
            if (testCapture) {
                discovery.testSingleCapture();
            }
            
            // Interactive mode
            if (!testCapture) {
                std::cout << "\n🎮 Interactive Commands:" << std::endl;
                std::cout << "  Press 'c' to test capture from first camera" << std::endl;
                std::cout << "  Press 'q' to quit" << std::endl;
                
                char cmd;
                while (std::cin >> cmd) {
                    if (cmd == 'q') {
                        break;
                    } else if (cmd == 'c') {
                        discovery.testSingleCapture();
                        std::cout << "\nPress 'c' for another capture or 'q' to quit: ";
                    } else {
                        std::cout << "Unknown command. Press 'c' for capture or 'q' to quit: ";
                    }
                }
            }
        }
        
        // Cleanup
        discovery.cleanup();
        
    } catch (const std::exception& e) {
        std::cout << "💥 Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 