#include "main_window.hpp"
#include <QMessageBox>

namespace cam_matrix {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , centralWidget(nullptr)
    , mainLayout(nullptr)
    , captureButton(nullptr)
    , settingsButton(nullptr)
    , statusLabel(nullptr)
    , statusBar(nullptr)
{
    setupUi();
    createConnections();
    
    // Set window properties
    setWindowTitle("Camera Matrix Capture");
    resize(800, 600);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    // Create central widget and main layout
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Create buttons
    captureButton = new QPushButton("Capture", this);
    captureButton->setMinimumHeight(40);
    
    settingsButton = new QPushButton("Settings", this);
    settingsButton->setMinimumHeight(40);
    
    // Create status label
    statusLabel = new QLabel("Ready", this);
    statusLabel->setAlignment(Qt::AlignCenter);
    
    // Add widgets to layout
    mainLayout->addWidget(captureButton);
    mainLayout->addWidget(settingsButton);
    mainLayout->addWidget(statusLabel);
    mainLayout->addStretch();
    
    // Create status bar
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    statusBar->showMessage("Application started");
}

void MainWindow::createConnections()
{
    connect(captureButton, &QPushButton::clicked, this, &MainWindow::onCaptureButtonClicked);
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::onSettingsButtonClicked);
}

void MainWindow::onCaptureButtonClicked()
{
    statusBar->showMessage("Capture button clicked");
    // TODO: Implement capture functionality
}

void MainWindow::onSettingsButtonClicked()
{
    statusBar->showMessage("Settings button clicked");
    // TODO: Implement settings dialog
}

} // namespace cam_matrix 