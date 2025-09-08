// Main entry point for the modular Camera Matrix Capture application
// Following Google C++ Style Guide

#include <iostream>
#include <winrt/base.h>
#include "gui/Application.hpp"

// Entry point
int main(int argc, char* argv[]) {
  // Initialize WinRT apartment for Bluetooth support
  winrt::init_apartment();
  
  try {
    // Create application instance
    SaperaCapturePro::Application app;
    
    // Initialize all subsystems
    if (!app.Initialize()) {
      std::cerr << "Failed to initialize application" << std::endl;
      return -1;
    }
    
    // Run main application loop
    app.Run();
    
    // Clean shutdown handled by destructor
    return 0;
    
  } catch (const std::exception& e) {
    std::cerr << "Application error: " << e.what() << std::endl;
    return -1;
  } catch (...) {
    std::cerr << "Unknown error occurred" << std::endl;
    return -1;
  }
}