#include "gui/gui_manager.hpp"
#include "gui/main_window.hpp"
#include <QApplication>
#include <iostream>
#include <stdexcept>
#include <QTimer>

int main(int argc, char *argv[]) {
    try {
        QApplication app(argc, argv);

        // Set application info
        QApplication::setOrganizationName("YourCompany");
        QApplication::setApplicationName("Camera Matrix Capture");
        
        // Apply stylesheet for better appearance
        app.setStyle("Fusion");

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
            
            // Add a short delay before scanning for cameras to ensure UI is fully initialized
            QTimer::singleShot(500, [mainWindow]() {
                // Trigger camera refresh after UI is shown
                QMetaObject::invokeMethod(mainWindow, "refreshCameras", Qt::QueuedConnection);
            });
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
