#include "main_window.hpp"
#include "ui/pages/page.hpp"
#include "ui/pages/camera/camera_page.hpp"
#include "ui/pages/settings/settings_page.hpp"
#include "ui/pages/capture/capture_page.hpp"
#include "ui/pages/calibration/calibration_page.hpp"
#include "ui/pages/dataset/dataset_page.hpp"

#include <QStatusBar>
#include <QMenuBar>
#include <QMessageBox>

namespace cam_matrix {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , statusBar_(nullptr)
{
    setupUi();
    createStatusBar();
    
    auto cameraPage = new ui::CameraPage(this);
    setCentralWidget(cameraPage);
    connectPageSignals(cameraPage);
    cameraPage->initialize();
    
    setWindowTitle(tr("Camera Matrix Capture"));
    resize(1024, 768);
}

void MainWindow::setupUi()
{
    // Empty - we no longer need the tab widget
    // We'll directly set the cameraPage as central widget
}

void MainWindow::createStatusBar()
{
    statusBar_ = new QStatusBar(this);
    setStatusBar(statusBar_);
    statusBar_->showMessage(tr("Ready"));
}

void MainWindow::connectPageSignals(ui::Page* page)
{
    if (!page) return;
    
    connect(page, &ui::Page::statusChanged,
            this, &MainWindow::onPageStatusChanged);
    connect(page, &ui::Page::error,
            this, &MainWindow::onPageError);
}

void MainWindow::onPageStatusChanged(const QString& message)
{
    statusBar_->showMessage(message);
}

void MainWindow::onPageError(const QString& message)
{
    QMessageBox::critical(this, tr("Error"), message);
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About Camera Matrix Capture"),
                      tr("Camera Matrix Capture v1.0.0\n"
                         "A professional tool for synchronized "
                         "multi-camera capture and calibration."));
}

} // namespace cam_matrix 