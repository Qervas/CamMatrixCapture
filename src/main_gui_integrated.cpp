#include "gui/NeuralCaptureGUI.hpp"
#include <iostream>

int main() {
    std::cout << "🎬 Neural Rendering Capture System - INTEGRATED GUI" << std::endl;
    std::cout << "====================================================" << std::endl;
    std::cout << "Direct Sapera SDK Integration - No Workarounds!" << std::endl;
    std::cout << std::endl;
    
    try {
        NeuralCaptureGUI app;
        
        if (!app.Initialize()) {
            std::cerr << "❌ Failed to initialize INTEGRATED application" << std::endl;
            return -1;
        }
        
        app.Run();
        app.Shutdown();
        
        std::cout << "✅ INTEGRATED application terminated successfully" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ INTEGRATED application error: " << e.what() << std::endl;
        return -1;
    }
} 