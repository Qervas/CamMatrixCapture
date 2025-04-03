#pragma once

#include <memory>
#include <string>
#include <functional>

namespace cam_matrix {

class GuiManager {
public:
    GuiManager();
    ~GuiManager();

    // Delete copy constructor and assignment operator
    GuiManager(const GuiManager&) = delete;
    GuiManager& operator=(const GuiManager&) = delete;

    // Initialize the GUI
    bool initialize();
    
    // Run the main GUI loop
    void run();
    
    // Check if GUI is running
    bool isRunning() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl; // PIMPL idiom for better compilation times
};

} // namespace cam_matrix