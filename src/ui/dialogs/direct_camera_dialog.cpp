#include "ui/dialogs/direct_camera_dialog.hpp"
#include "ui/widgets/direct_camera_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QIcon>
#include <QFile>

namespace cam_matrix::ui {

DirectCameraDialog::DirectCameraDialog(QWidget* parent)
    : QDialog(parent)
    , cameraWidget_(nullptr)
    , closeButton_(nullptr)
    , statusBar_(nullptr)
{
    // Set dialog properties
    setWindowTitle(tr("Direct Camera Access"));
    setWindowIcon(QIcon::fromTheme("camera-photo"));
    resize(800, 600);
    
    // Create layout
    auto* mainLayout = new QVBoxLayout(this);
    
    // Create camera widget
    cameraWidget_ = new DirectCameraWidget(this);
    mainLayout->addWidget(cameraWidget_);
    
    // Create button layout
    auto* buttonLayout = new QHBoxLayout();
    closeButton_ = new QPushButton(tr("Close"), this);
    auto* refreshMainButton = new QPushButton(tr("Send to Main View"), this);
    buttonLayout->addWidget(refreshMainButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton_);
    
    mainLayout->addLayout(buttonLayout);
    
    // Add status bar
    statusBar_ = new QStatusBar(this);
    mainLayout->addWidget(statusBar_);
    
    // Connect signals
    connect(closeButton_, &QPushButton::clicked, this, &QDialog::accept);
    connect(refreshMainButton, &QPushButton::clicked, this, [this]() {
        emit refreshMainCameraView();
        statusBar_->showMessage(tr("Camera information sent to main view"), 3000);
    });
    
    connect(cameraWidget_, &DirectCameraWidget::statusChanged,
            this, &DirectCameraDialog::onStatusChanged);
    
    connect(cameraWidget_, &DirectCameraWidget::error,
            this, &DirectCameraDialog::onError);
            
    // Connect camera detected signal
    connect(cameraWidget_, &DirectCameraWidget::cameraDetected,
            this, &DirectCameraDialog::cameraDetected);
    
    // Automatically emit a signal when cameras are found during initialization
    connect(cameraWidget_, &DirectCameraWidget::camerasFound,
            this, &DirectCameraDialog::camerasFound);
}

void DirectCameraDialog::onStatusChanged(const QString& status) {
    statusBar_->showMessage(status, 5000);
    setWindowTitle(tr("Direct Camera Access - %1").arg(status));
}

void DirectCameraDialog::onError(const QString& error) {
    statusBar_->showMessage(error, 5000);
    QMessageBox::warning(this, tr("Camera Error"), error);
    setWindowTitle(tr("Direct Camera Access - Error: %1").arg(error));
}

void DirectCameraDialog::refreshCameras() {
    if (cameraWidget_) {
        cameraWidget_->refreshCameras();
    }
}

} // namespace cam_matrix::ui 