#include "gui_manager.hpp"
#include "main_window.hpp"
#include <iostream>

namespace cam_matrix {

class GuiManager::Impl {
public:
    Impl() : mainWindow(nullptr) {}
    MainWindow* mainWindow;
};

GuiManager::GuiManager() : pImpl(std::make_unique<Impl>()) {}

GuiManager::~GuiManager() = default;

bool GuiManager::initialize() {
    std::cout << "Initializing GUI..." << std::endl;
    pImpl->mainWindow = new MainWindow();
    return true;
}

MainWindow* GuiManager::getMainWindow() const {
    return pImpl->mainWindow;
}

} // namespace cam_matrix 