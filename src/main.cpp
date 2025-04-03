#include "gui/gui_manager.hpp"
#include <iostream>
#include <stdexcept>

int main() {
    try {
        cam_matrix::GuiManager gui;
        
        if (!gui.initialize()) {
            std::cerr << "Failed to initialize GUI" << std::endl;
            return 1;
        }
        
        gui.run();
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 