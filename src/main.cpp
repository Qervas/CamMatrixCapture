#include "camera_manager.hpp"
#include "gui_manager.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>

void printError(const std::string& message) {
    std::cerr << "\033[1;31mERROR: " << message << "\033[0m" << std::endl;
}

void printInfo(const std::string& message) {
    std::cout << "\033[1;34mINFO: " << message << "\033[0m" << std::endl;
}

int main() {
    try {
        printInfo("Starting Camera Matrix Capture application...");

        // Initialize camera manager
        printInfo("Initializing camera manager...");
        CameraManager cameraManager;
        if (!cameraManager.initializeCameras(4)) { // Assuming 4 cameras for now
            printError("Failed to initialize cameras");
            std::cout << "\nPress Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
        printInfo("Camera manager initialized successfully");

        // Initialize GUI manager
        printInfo("Initializing GUI manager...");
        GUIManager guiManager;
        if (!guiManager.initialize("Camera Matrix Capture", 1280, 720)) {
            printError("Failed to initialize GUI");
            std::cout << "\nPress Enter to exit..." << std::endl;
            std::cin.get();
            return 1;
        }
        printInfo("GUI manager initialized successfully");

        // Set up preview callback
        printInfo("Setting up preview callback...");
        guiManager.setPreviewCallback([&cameraManager](const std::vector<cv::Mat>& images) {
            try {
                printInfo("Preview callback triggered");
                // This will be called when we need to update the preview
                // The actual preview update will be handled by the GUI manager
            } catch (const std::exception& e) {
                printError("Error in preview callback: " + std::string(e.what()));
            }
        });

        // Set up capture callback
        printInfo("Setting up capture callback...");
        guiManager.setCaptureCallback([&cameraManager]() {
            try {
                printInfo("Capture callback triggered");
                std::vector<cv::Mat> images;
                if (cameraManager.captureAll(images)) {
                    printInfo("Successfully captured " + std::to_string(images.size()) + " images");
                    // TODO: Save images to disk with proper naming
                } else {
                    printError("Failed to capture images");
                }
            } catch (const std::exception& e) {
                printError("Error in capture callback: " + std::string(e.what()));
            }
        });

        printInfo("Starting main application loop...");
        // Main application loop
        guiManager.run();
        printInfo("Application loop ended normally");

    } catch (const std::exception& e) {
        printError("Unhandled exception: " + std::string(e.what()));
        std::cout << "\nPress Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    } catch (...) {
        printError("Unknown exception occurred");
        std::cout << "\nPress Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    printInfo("Application terminated successfully");
    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();
    return 0;
} 