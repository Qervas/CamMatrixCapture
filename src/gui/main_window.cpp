#include "main_window.hpp"
#include "ui/pages/page.hpp"
#include "ui/pages/camera/camera_page.hpp"
#include "ui/pages/settings/settings_page.hpp"
#include "ui/pages/capture/capture_page.hpp"
#include "ui/pages/calibration/calibration_page.hpp"
#include "ui/pages/dataset/dataset_page.hpp"
#include "ui/pages/image_processing/image_processing_page.hpp"

#include <QStatusBar>
#include <QMenuBar>
#include <QMessageBox>
#include <QAction>
#include <QToolBar>
#include <QStackedWidget>
#include <QApplication>
#include <QMetaObject>

namespace cam_matrix {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , statusBar_(nullptr)
{
    setupUi();
    createMenuBar();
    createStatusBar();
    
    // Set up the stacked widget for pages
    pagesWidget_ = new QStackedWidget(this);
    setCentralWidget(pagesWidget_);
    
    // Add pages
    addPage(new ui::CameraPage(this));
    addPage(new ui::ImageProcessingPage(this));
    
    // Select the first page (Camera)
    pagesWidget_->setCurrentIndex(0);
    
    setWindowTitle(tr("Camera Matrix Capture"));
    resize(1024, 768);
}

void MainWindow::setupUi()
{
    // Set theme-aware styles for the main window
    QPalette systemPalette = QApplication::palette();
    bool isDarkTheme = systemPalette.color(QPalette::Window).lightness() < 128;
    
    // Set application style
    QString appStyle = QString(
        "QMainWindow { background-color: %1; color: %2; }"
    ).arg(
        isDarkTheme ? "#222222" : "#f5f5f5",
        isDarkTheme ? "#e0e0e0" : "#202020"
    );
    
    setStyleSheet(appStyle);
}

void MainWindow::createMenuBar()
{
    QMenuBar* menuBar = this->menuBar();
    
    // Pages menu
    QMenu* pagesMenu = menuBar->addMenu(tr("&Pages"));
    
    // Camera page action
    QAction* cameraPageAction = new QAction(tr("&Camera"), this);
    cameraPageAction->setStatusTip(tr("Switch to camera control page"));
    connect(cameraPageAction, &QAction::triggered, this, [this]() {
        pagesWidget_->setCurrentIndex(0);
    });
    pagesMenu->addAction(cameraPageAction);
    
    // Image processing page action
    QAction* imageProcessingPageAction = new QAction(tr("&Image Processing"), this);
    imageProcessingPageAction->setStatusTip(tr("Switch to image processing page"));
    connect(imageProcessingPageAction, &QAction::triggered, this, [this]() {
        pagesWidget_->setCurrentIndex(1);
    });
    pagesMenu->addAction(imageProcessingPageAction);
    
    // Help menu
    QMenu* helpMenu = menuBar->addMenu(tr("&Help"));
    
    // About action
    QAction* aboutAction = new QAction(tr("&About"), this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    helpMenu->addAction(aboutAction);
}

void MainWindow::createStatusBar()
{
    statusBar_ = new QStatusBar(this);
    setStatusBar(statusBar_);
    statusBar_->showMessage(tr("Ready"));
}

void MainWindow::addPage(ui::Page* page)
{
    if (!page) return;
    
    // Add to the stacked widget
    pagesWidget_->addWidget(page);
    
    // Connect signals
    connectPageSignals(page);
    
    // Initialize the page
    page->initialize();
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

void MainWindow::refreshCameras()
{
    // Find the camera page and refresh cameras
    for (int i = 0; i < pagesWidget_->count(); ++i) {
        auto* page = qobject_cast<ui::Page*>(pagesWidget_->widget(i));
        if (page && page->title() == "Cameras") {
            // Use meta object to invoke the method by name
            QMetaObject::invokeMethod(page, "refreshCameras", Qt::QueuedConnection);
            showStatusMessage("Refreshing camera list...");
            return;
        }
    }
}

void MainWindow::showStatusMessage(const QString& message, int timeout)
{
    statusBar_->showMessage(message, timeout);
}

} // namespace cam_matrix 