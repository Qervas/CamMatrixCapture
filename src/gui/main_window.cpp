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
#include <QTabWidget>
#include <QIcon>
#include <QTabBar>
#include <QStyleOption>
#include <QPainter>
#include <QFontDatabase>

namespace cam_matrix {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , statusBar_(nullptr)
{
    setupUi();
    createStatusBar();
    
    // Set up the tabbed widget for pages
    tabWidget_ = new QTabWidget(this);
    tabWidget_->setTabPosition(QTabWidget::North);
    tabWidget_->setDocumentMode(true);
    tabWidget_->setMovable(false);
    tabWidget_->setTabsClosable(false);
    
    // Apply modern styling to tabs
    QString tabStyle = QString(
        "QTabWidget::pane { border: none; background: transparent; }"
        "QTabWidget::tab-bar { alignment: center; }"
        "QTabBar::tab { padding: 12px 20px; margin: 0px; }"
        "QTabBar::tab:selected { border-bottom: 2px solid #007AFF; }"
        "QTabBar::tab:hover:!selected { background-color: rgba(0, 122, 255, 0.1); }"
    );
    tabWidget_->setStyleSheet(tabStyle);
    setCentralWidget(tabWidget_);
    
    // Add pages
    addPage(new ui::CameraPage(this), "Camera", "camera");
    addPage(new ui::ImageProcessingPage(this), "Processing", "image");
    
    // Set window properties
    setWindowTitle(tr("Camera Matrix Capture"));
    resize(1280, 800);
    setMinimumSize(960, 640);
}

void MainWindow::loadFonts()
{
    // Set application font to a sleek, modern system font
    QFont appFont("-apple-system, BlinkMacSystemFont, Segoe UI, Roboto, Helvetica, Arial", 10);
    QApplication::setFont(appFont);
}

void MainWindow::setupUi()
{
    // Load modern system fonts
    loadFonts();
    
    // Get system palette to determine if we're in dark or light mode
    QPalette systemPalette = QApplication::palette();
    bool isDarkTheme = systemPalette.color(QPalette::Window).lightness() < 128;
    
    // Modern color palette
    QString bgColor = isDarkTheme ? "#1C1C1E" : "#F2F2F7";
    QString textColor = isDarkTheme ? "#FFFFFF" : "#000000";
    QString accentColor = "#007AFF"; // Apple blue
    
    // Create a clean, modern style
    setStyleSheet(QString(
        "QMainWindow { background: %1; color: %2; }"
        "QStatusBar { background: %1; color: %2; border-top: 1px solid rgba(60, 60, 60, 0.3); }"
        "QMenuBar { background: %1; color: %2; border-bottom: 1px solid rgba(60, 60, 60, 0.3); }"
        "QMenuBar::item { padding: 6px 12px; }"
        "QMenuBar::item:selected { background: rgba(0, 122, 255, 0.1); border-radius: 4px; }"
        "QMenu { background: %1; color: %2; border: 1px solid rgba(60, 60, 60, 0.3); border-radius: 5px; }"
        "QMenu::item { padding: 6px 24px 6px 12px; }"
        "QMenu::item:selected { background: rgba(0, 122, 255, 0.1); }"
        "QPushButton { background: %3; color: white; border: none; border-radius: 6px; padding: 8px 16px; font-weight: medium; }"
        "QPushButton:hover { background: #0069D9; }"
        "QPushButton:pressed { background: #0062CC; }"
        "QPushButton:disabled { background: #A0A0A0; }"
        "QGroupBox { font-weight: bold; border: 1px solid rgba(60, 60, 60, 0.3); border-radius: 5px; margin-top: 10px; padding-top: 10px; color: %2; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
    ).arg(bgColor, textColor, accentColor));
    
    // Create a modern toolbar instead of menu
    QToolBar* toolbar = new QToolBar(this);
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(22, 22));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    
    // Create actions with modern icons
    QAction* refreshAction = toolbar->addAction(QIcon::fromTheme("view-refresh"), tr("Refresh"));
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshCameras);
    
    QAction* aboutAction = toolbar->addAction(QIcon::fromTheme("help-about"), tr("About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    
    toolbar->addSeparator();
    
    // Add the toolbar to the top of the window
    addToolBar(Qt::TopToolBarArea, toolbar);
}

void MainWindow::createStatusBar()
{
    statusBar_ = new QStatusBar(this);
    statusBar_->setSizeGripEnabled(false);
    setStatusBar(statusBar_);
    statusBar_->showMessage(tr("Ready"));
}

void MainWindow::addPage(ui::Page* page, const QString& title, const QString& iconName)
{
    if (!page) return;
    
    // Add to the tabbed widget with icon
    QIcon icon = QIcon::fromTheme(iconName);
    tabWidget_->addTab(page, icon, title);
    
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
    for (int i = 0; i < tabWidget_->count(); ++i) {
        auto* page = qobject_cast<ui::Page*>(tabWidget_->widget(i));
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