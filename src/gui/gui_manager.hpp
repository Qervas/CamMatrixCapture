#pragma once

#include <memory>
#include <string>
#include <functional>
#include <QObject>

namespace cam_matrix {

class MainWindow;

class GuiManager : public QObject {
    Q_OBJECT

public:
    GuiManager();
    ~GuiManager();

    // Delete copy constructor and assignment operator
    GuiManager(const GuiManager&) = delete;
    GuiManager& operator=(const GuiManager&) = delete;

    // Initialize the GUI
    bool initialize();
    
    // Get the main window
    MainWindow* getMainWindow() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl; // PIMPL idiom for better compilation times
};

} // namespace cam_matrix