#include "camera_page.hpp"
#include "ui/widgets/camera_control_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>

namespace cam_matrix::ui {

CameraPage::CameraPage(QWidget* parent)
    : Page(parent)
    , cameraList_(nullptr)
    , cameraControl_(nullptr)
    , refreshButton_(nullptr)
{
}

CameraPage::~CameraPage() = default;

void CameraPage::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    // Create splitter for camera list and controls
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter);

    // Left panel: Camera list
    auto* leftWidget = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);

    cameraList_ = new QListWidget(leftWidget);
    leftLayout->addWidget(cameraList_);

    // Refresh button
    auto* buttonLayout = new QHBoxLayout;
    refreshButton_ = new QPushButton(tr("Refresh"), leftWidget);
    buttonLayout->addWidget(refreshButton_);
    buttonLayout->addStretch();
    leftLayout->addLayout(buttonLayout);

    // Right panel: Camera controls
    cameraControl_ = new CameraControlWidget(splitter);
    cameraControl_->setEnabled(false); // Disabled until a camera is selected

    // Add some dummy cameras
    cameraList_->addItem("Camera 0: USB Webcam");
    cameraList_->addItem("Camera 1: IP Camera");
    cameraList_->addItem("Camera 2: DSLR");
}

void CameraPage::createConnections() {
    connect(refreshButton_, &QPushButton::clicked,
            this, &CameraPage::onRefreshCameras);

    connect(cameraList_, &QListWidget::currentRowChanged,
            this, &CameraPage::onCameraSelected);

    if (cameraControl_) {
        connect(cameraControl_, &CameraControlWidget::statusChanged,
                this, &CameraPage::onCameraStatusChanged);
    }
}

void CameraPage::initialize() {
    Page::initialize();
    loadSettings();
}

void CameraPage::cleanup() {
    saveSettings();
    Page::cleanup();
}

void CameraPage::loadSettings()
{
    // In a real implementation, we would load camera settings from storage
}

void CameraPage::saveSettings()
{
    // In a real implementation, we would save camera settings to storage
}

void CameraPage::onRefreshCameras()
{
    emit statusChanged(tr("Refreshing camera list..."));

    // In a real implementation, we would detect connected cameras
    // For now, just simulate a refresh

    emit statusChanged(tr("Camera list refreshed"));
}

void CameraPage::onCameraSelected(int index)
{
    if (index >= 0) {
        cameraControl_->setEnabled(true);
        cameraControl_->setCameraIndex(index);
        emit statusChanged(tr("Camera %1 selected").arg(index));
    } else {
        cameraControl_->setEnabled(false);
    }
}

void CameraPage::onCameraStatusChanged(const QString& status)
{
    emit statusChanged(status);
}

} // namespace cam_matrix::ui
