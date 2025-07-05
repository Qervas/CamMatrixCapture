/**
 * Minimal test - camera connection and image capture
 */

#include <iostream>
#include <algorithm>
#include "simple_interface.cpp"

using namespace SaperaCapturePro;

int main() {
    std::cout << "🔍 Camera Connection & Capture Test" << std::endl;
    std::cout << "====================================" << std::endl;
    
    try {
        SimpleSaperaInterface interface;
        
        // Discover cameras
        auto cameras = interface.discoverCameras();
        
        std::cout << "\n📷 Found " << cameras.size() << " cameras:" << std::endl;
        for (const auto& cam : cameras) {
            std::cout << "  - " << cam.id << ": " << cam.serialNumber 
                     << " (" << cam.modelName << ")" 
                     << " [" << (cam.isConnected ? "Connected" : "Disconnected") << "]" << std::endl;
        }
        
        if (cameras.empty()) {
            std::cout << "No cameras to test with" << std::endl;
            return 0;
        }
        
        // Test connecting to first camera
        const auto& firstCamera = cameras[0];
        std::cout << "\n🔗 Testing connection to camera: " << firstCamera.id << std::endl;
        
        if (interface.connectCamera(firstCamera.id)) {
            std::cout << "✅ Connection successful!" << std::endl;
            
            // Check capture readiness
            std::cout << "🔍 Checking capture readiness..." << std::endl;
            if (interface.isCaptureReady(firstCamera.id)) {
                std::cout << "✅ Camera is ready for capture" << std::endl;
                
                // Test image capture without saving
                std::cout << "\n📸 Testing image capture (no save)..." << std::endl;
                if (interface.captureImage(firstCamera.id)) {
                    std::cout << "✅ Image capture successful!" << std::endl;
                    
                    // Test image capture with saving
                    std::cout << "\n💾 Testing image capture with save..." << std::endl;
                    std::string filename = "test_capture_cam" + firstCamera.id + ".tiff";
                    if (interface.captureImage(firstCamera.id, filename)) {
                        std::cout << "✅ Image captured and saved to: " << filename << std::endl;
                    } else {
                        std::cout << "❌ Image capture with save failed" << std::endl;
                    }
                    
                } else {
                    std::cout << "❌ Image capture failed" << std::endl;
                }
                
            } else {
                std::cout << "❌ Camera is not ready for capture" << std::endl;
            }
            
            // Test disconnection
            std::cout << "\n🔌 Testing disconnection..." << std::endl;
            if (interface.disconnectCamera(firstCamera.id)) {
                std::cout << "✅ Disconnection successful!" << std::endl;
            } else {
                std::cout << "❌ Disconnection failed" << std::endl;
            }
            
        } else {
            std::cout << "❌ Connection failed" << std::endl;
        }
        
        // Test multiple camera capture
        if (cameras.size() >= 2) {
            std::cout << "\n🔗 Testing multiple camera capture..." << std::endl;
            
            // Connect to first 2 cameras
            std::vector<std::string> testCameras;
            for (size_t i = 0; i < std::min(size_t(2), cameras.size()); i++) {
                std::cout << "Connecting to camera " << cameras[i].id << "..." << std::endl;
                if (interface.connectCamera(cameras[i].id)) {
                    std::cout << "✅ Connected" << std::endl;
                    testCameras.push_back(cameras[i].id);
                } else {
                    std::cout << "❌ Failed" << std::endl;
                }
            }
            
            // Capture from all connected cameras
            std::cout << "\n📸 Capturing from " << testCameras.size() << " cameras..." << std::endl;
            for (const auto& cameraId : testCameras) {
                std::cout << "Capturing from camera " << cameraId << "..." << std::endl;
                std::string filename = "multi_capture_cam" + cameraId + ".tiff";
                if (interface.captureImage(cameraId, filename)) {
                    std::cout << "✅ Captured and saved: " << filename << std::endl;
                } else {
                    std::cout << "❌ Capture failed for camera " << cameraId << std::endl;
                }
            }
            
            auto connectedIds = interface.getConnectedCameraIds();
            std::cout << "📊 Total connected: " << connectedIds.size() << " cameras" << std::endl;
        }
        
        std::cout << "\n✅ Capture test completed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "💥 Exception: " << e.what() << std::endl;
        return 1;
    }
} 