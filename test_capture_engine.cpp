/**
 * Test program for simplified CaptureEngine
 */

#include <iostream>
#include "src/core/CaptureEngine.hpp"
#include "src/config/ConfigurationManager.hpp"
#include "src/utils/Logger.hpp"

using namespace SaperaCapturePro;

int main() {
    std::cout << "🎬 Testing Simplified CaptureEngine" << std::endl;
    std::cout << "====================================" << std::endl;
    
    try {
        // Initialize logger
        Logger::setLevel(Logger::LogLevel::Info);
        
        // Create configuration manager
        ConfigurationManager configManager;
        configManager.loadConfiguration("config/system.json");
        
        // Create and initialize capture engine
        CaptureEngine engine;
        
        std::cout << "🔧 Initializing CaptureEngine..." << std::endl;
        if (!engine.initialize(configManager)) {
            std::cout << "❌ Failed to initialize CaptureEngine" << std::endl;
            return 1;
        }
        
        std::cout << "✅ CaptureEngine initialized successfully" << std::endl;
        
        // Get available cameras
        std::cout << "\n🔍 Discovering cameras..." << std::endl;
        auto cameras = engine.getAvailableCameras();
        
        if (cameras.empty()) {
            std::cout << "⚠️  No cameras found" << std::endl;
            return 1;
        }
        
        std::cout << "📷 Found " << cameras.size() << " camera(s):" << std::endl;
        for (const auto& camera : cameras) {
            std::cout << "  - " << camera.id << ": " << camera.name 
                     << " (Serial: " << camera.serialNumber << ")" << std::endl;
        }
        
        // Test connecting to first camera
        if (!cameras.empty()) {
            const auto& firstCamera = cameras[0];
            std::cout << "\n🔗 Connecting to camera: " << firstCamera.id << std::endl;
            
            if (engine.connectCamera(firstCamera.id)) {
                std::cout << "✅ Successfully connected to camera: " << firstCamera.id << std::endl;
                
                // Test capture
                std::cout << "\n📸 Testing image capture..." << std::endl;
                ImageBuffer buffer;
                if (engine.captureImageFromCamera(firstCamera.id, buffer)) {
                    std::cout << "✅ Image captured successfully!" << std::endl;
                    std::cout << "   Size: " << buffer.width << "x" << buffer.height << std::endl;
                    std::cout << "   Format: " << static_cast<int>(buffer.pixelFormat) << std::endl;
                } else {
                    std::cout << "❌ Image capture failed" << std::endl;
                }
                
                // Disconnect
                std::cout << "\n🔌 Disconnecting camera..." << std::endl;
                engine.disconnectCamera(firstCamera.id);
                
            } else {
                std::cout << "❌ Failed to connect to camera: " << firstCamera.id << std::endl;
            }
        }
        
        std::cout << "\n✅ Test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "💥 Exception: " << e.what() << std::endl;
        return 1;
    }
} 