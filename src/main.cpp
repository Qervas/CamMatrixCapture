#include "gui/gui_manager.hpp"
#include "gui/main_window.hpp"
#include <QApplication>
#include <iostream>
#include <stdexcept>

int main(int argc, char *argv[]) {
    try {
        QApplication app(argc, argv);

        // Set application info
        QApplication::setOrganizationName("YourCompany");
        QApplication::setApplicationName("Camera Matrix Capture");

        // Create GUI manager
        cam_matrix::GuiManager guiManager;

        // Initialize GUI
        if (!guiManager.initialize()) {
            throw std::runtime_error("Failed to initialize GUI");
        }

        // Show the main window
        auto mainWindow = guiManager.getMainWindow();
        if (mainWindow) {
            mainWindow->show();
        } else {
            throw std::runtime_error("Failed to create main window");
        }

        // Run the application
        return app.exec();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
