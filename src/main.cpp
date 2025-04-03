#include "gui/main_window.hpp"
#include <QApplication>
#include <iostream>
#include <stdexcept>

int main(int argc, char *argv[]) {
    try {
        QApplication app(argc, argv);
        
        cam_matrix::MainWindow mainWindow;
        mainWindow.show();
        
        return app.exec();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 