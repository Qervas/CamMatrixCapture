/**
 * main.cpp - Simple Working SaperaCapture Pro Application
 * Based on proven simple_interface.cpp approach
 */

#include "hardware/CameraTypes.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <map>

// Include Sapera headers directly
#include "SapClassBasic.h"

using namespace SaperaCapturePro;

struct ConnectedCamera {
    CameraInfo info;
    SapAcqDevice* acqDevice = nullptr;
    SapBuffer* buffer = nullptr;
    SapAcqDeviceToBuf* transfer = nullptr;
    bool connected = false;
    bool captureReady = false;
};

class SimpleCameraSystem {
private:
    std::vector<CameraInfo> discoveredCameras_;
    std::map<std::string, ConnectedCamera> connectedCameras_;

public:
    std::vector<CameraInfo> discoverCameras() {
        discoveredCameras_.clear();
        
        std::cout << "🔍 Discovering cameras..." << std::endl;
        
        // Get server count
        int serverCount = SapManager::GetServerCount();
        std::cout << "Found " << serverCount << " server(s)" << std::endl;
        
        if (serverCount == 0) {
            std::cout << "❌ No Sapera servers found" << std::endl;
            return discoveredCameras_;
        }
        
        // Enumerate all servers
        for (int serverIndex = 0; serverIndex < serverCount; serverIndex++) {
            char serverName[256];
            if (!SapManager::GetServerName(serverIndex, serverName, sizeof(serverName))) {
                std::cout << "❌ Failed to get server name" << std::endl;
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
                    camera.id = std::to_string(discoveredCameras_.size() + 1);
                    camera.serverName = serverName;
                    camera.resourceIndex = resourceIndex;
                    
                    // Get serial number
                    if (acqDevice->GetFeatureValue("DeviceSerialNumber", buffer, sizeof(buffer))) {
                        camera.serialNumber = std::string(buffer);
                    } else {
                        camera.serialNumber = "Unknown_" + std::to_string(resourceIndex);
                    }
                    
                    // Get model name
                    if (acqDevice->GetFeatureValue("DeviceModelName", buffer, sizeof(buffer))) {
                        camera.modelName = std::string(buffer);
                    } else {
                        camera.modelName = "Unknown_Model";
                    }
                    
                    camera.name = camera.modelName + "_" + camera.id;
                    camera.isConnected = false;
                    camera.status = CameraStatus::Disconnected;
                    camera.type = CameraType::Industrial;
                    
                    discoveredCameras_.push_back(camera);
                    std::cout << "  ✅ Camera " << resourceIndex << ": " << camera.serialNumber 
                             << " (" << camera.modelName << ")" << std::endl;
                    
                    // Cleanup discovery device
                    acqDevice->Destroy();
                    delete acqDevice;
                    
                } catch (const std::exception& e) {
                    std::cout << "  ❌ Exception: " << e.what() << std::endl;
                }
            }
        }
        
        std::cout << "✅ Discovery complete: " << discoveredCameras_.size() << " cameras found" << std::endl;
        return discoveredCameras_;
    }
    
    bool connectCamera(const std::string& cameraId) {
        std::cout << "🔗 Connecting to camera: " << cameraId << std::endl;
        
        // Check if already connected
        if (connectedCameras_.find(cameraId) != connectedCameras_.end()) {
            std::cout << "ℹ️ Camera " << cameraId << " already connected" << std::endl;
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
            std::cout << "❌ Camera " << cameraId << " not found in discovered cameras" << std::endl;
            return false;
        }
        
        try {
            // Create acquisition device for this camera
            auto acqDevice = new SapAcqDevice(cameraInfo->serverName.c_str(), cameraInfo->resourceIndex);
            if (!acqDevice->Create()) {
                std::cout << "❌ Failed to create acquisition device for camera " << cameraId << std::endl;
                delete acqDevice;
                return false;
            }
            
            // Create buffer for image capture
            auto buffer = new SapBufferWithTrash(1, acqDevice);
            if (!buffer->Create()) {
                std::cout << "❌ Failed to create buffer for camera " << cameraId << std::endl;
                acqDevice->Destroy();
                delete acqDevice;
                delete buffer;
                return false;
            }
            
            // Create transfer object
            auto transfer = new SapAcqDeviceToBuf(acqDevice, buffer);
            if (!transfer->Create()) {
                std::cout << "❌ Failed to create transfer for camera " << cameraId << std::endl;
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
            
            connectedCameras_[cameraId] = connectedCam;
            
            // Update the discovered camera status
            cameraInfo->isConnected = true;
            cameraInfo->status = CameraStatus::Ready;
            
            std::cout << "✅ Successfully connected to camera " << cameraId 
                     << " (" << cameraInfo->serialNumber << ")" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception connecting to camera " << cameraId << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    bool captureImage(const std::string& cameraId) {
        std::cout << "📸 Capturing image from camera: " << cameraId << std::endl;
        
        auto it = connectedCameras_.find(cameraId);
        if (it == connectedCameras_.end()) {
            std::cout << "❌ Camera " << cameraId << " not connected" << std::endl;
            return false;
        }
        
        ConnectedCamera& cam = it->second;
        if (!cam.captureReady) {
            std::cout << "❌ Camera " << cameraId << " not ready for capture" << std::endl;
            return false;
        }
        
        try {
            // Perform single image capture
            if (!cam.transfer->Grab()) {
                std::cout << "❌ Failed to grab image from camera " << cameraId << std::endl;
                return false;
            }
            
            // Get image dimensions
            int width = cam.buffer->GetWidth();
            int height = cam.buffer->GetHeight();
            int pixelDepth = cam.buffer->GetPixelDepth();
            
            std::cout << "✅ Image captured successfully!" << std::endl;
            std::cout << "   📏 Dimensions: " << width << "x" << height << std::endl;
            std::cout << "   🎨 Pixel depth: " << pixelDepth << " bits" << std::endl;
            
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception during capture: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool disconnectCamera(const std::string& cameraId) {
        std::cout << "🔌 Disconnecting camera: " << cameraId << std::endl;
        
        auto it = connectedCameras_.find(cameraId);
        if (it == connectedCameras_.end()) {
            std::cout << "ℹ️ Camera " << cameraId << " not connected" << std::endl;
            return true;
        }
        
        ConnectedCamera& cam = it->second;
        
        try {
            // Clean up Sapera objects
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
            
            // Remove from connected cameras
            connectedCameras_.erase(it);
            
            // Update discovered camera status
            for (auto& camera : discoveredCameras_) {
                if (camera.id == cameraId) {
                    camera.isConnected = false;
                    camera.status = CameraStatus::Disconnected;
                    break;
                }
            }
            
            std::cout << "✅ Camera " << cameraId << " disconnected successfully" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Exception disconnecting camera " << cameraId << ": " << e.what() << std::endl;
            return false;
        }
    }
    
    void printCameraList() {
        std::cout << "\n=== Camera List ===" << std::endl;
        if (discoveredCameras_.empty()) {
            std::cout << "No cameras discovered" << std::endl;
            return;
        }
        
        for (const auto& camera : discoveredCameras_) {
            std::cout << "📸 Camera " << camera.id << ": " << camera.name << std::endl;
            std::cout << "   Serial: " << camera.serialNumber << std::endl;
            std::cout << "   Model: " << camera.modelName << std::endl;
            std::cout << "   Server: " << camera.serverName << std::endl;
            std::cout << "   Status: " << (camera.isConnected ? "🟢 Connected" : "🔴 Disconnected") << std::endl;
            std::cout << std::endl;
        }
    }
    
    ~SimpleCameraSystem() {
        // Clean up all connected cameras
        for (auto& [cameraId, cam] : connectedCameras_) {
            disconnectCamera(cameraId);
        }
    }
};

void printBanner() {
    std::cout << R"(
🚀 ================================= 🚀
   SaperaCapture Pro - Simple & Reliable
   Camera Discovery & Capture System
🚀 ================================= 🚀
)" << std::endl;
}

int main() {
    printBanner();
    
    try {
        SimpleCameraSystem cameraSystem;
        
        // 1. Discover cameras
        std::cout << "Step 1: Camera Discovery" << std::endl;
        auto cameras = cameraSystem.discoverCameras();
        
        if (cameras.empty()) {
            std::cout << "❌ No cameras found. Check your Sapera installation and camera connections." << std::endl;
            return 1;
        }
        
        cameraSystem.printCameraList();
        
        // 2. Connect to first camera
        std::cout << "\nStep 2: Camera Connection" << std::endl;
        const std::string firstCameraId = cameras[0].id;
        
        if (!cameraSystem.connectCamera(firstCameraId)) {
            std::cout << "❌ Failed to connect to camera " << firstCameraId << std::endl;
            return 1;
        }
        
        // 3. Capture an image
        std::cout << "\nStep 3: Image Capture" << std::endl;
        if (cameraSystem.captureImage(firstCameraId)) {
            std::cout << "🎉 Image capture successful!" << std::endl;
        } else {
            std::cout << "❌ Image capture failed" << std::endl;
        }
        
        // 4. Disconnect camera
        std::cout << "\nStep 4: Camera Disconnection" << std::endl;
        if (cameraSystem.disconnectCamera(firstCameraId)) {
            std::cout << "✅ Camera disconnected successfully" << std::endl;
        }
        
        std::cout << "\n🎉 Application completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Unhandled exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "❌ Unknown error occurred" << std::endl;
        return 1;
    }
    
    return 0;
} 