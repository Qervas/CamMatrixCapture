// Main entry point for the modular Camera Matrix Capture application
// Following Google C++ Style Guide

#include <iostream>
#include <winrt/base.h>
#include "gui/Application.hpp"

// Entry point
int main(int argc, char* argv[]) {
  // Initialize WinRT apartment for Bluetooth support
  winrt::init_apartment();
  
  int result = 0;
  
  try {
    // Create application instance
    SaperaCapturePro::Application app;
    
    // Initialize all subsystems
    if (!app.Initialize()) {
      std::cerr << "Failed to initialize application" << std::endl;
      result = -1;
    } else {
      // Run main application loop
      app.Run();
      
      // Explicitly call shutdown before destructor
      app.Shutdown();
    }
    
  } catch (const std::exception& e) {
    std::cerr << "Application error: " << e.what() << std::endl;
    result = -1;
  } catch (...) {
    std::cerr << "Unknown error occurred" << std::endl;
    result = -1;
  }
  
  // Uninitialize WinRT to ensure clean shutdown
  winrt::uninit_apartment();
  
  return result;
}