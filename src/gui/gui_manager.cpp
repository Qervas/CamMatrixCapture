#include "gui_manager.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace cam_matrix {

class GuiManager::Impl {
public:
    Impl() : running(false) {}
    bool running;
};

GuiManager::GuiManager() : pImpl(std::make_unique<Impl>()) {}

GuiManager::~GuiManager() = default;

bool GuiManager::initialize() {
    std::cout << "Initializing GUI..." << std::endl;
    return true;
}

void GuiManager::run() {
    if (!pImpl->running) {
        pImpl->running = true;
        std::cout << "GUI is running. Press Ctrl+C to exit." << std::endl;
        
        // Main loop
        while (pImpl->running) {
            // TODO: Add actual GUI implementation
            // For now, just keep the program running
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool GuiManager::isRunning() const {
    return pImpl->running;
}

} // namespace cam_matrix 