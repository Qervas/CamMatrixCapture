#include "camera_page.hpp"
#include "ui/widgets/camera_control_widget.hpp"
#include "ui/widgets/video_display_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>

namespace cam_matrix::ui {

CameraPage::CameraPage(QWidget* parent)
    : Page(parent)
    , cameraList_(nullptr)
    , cameraControl_(nullptr)
    , refreshButton_(nullptr)
    , connectButton_(nullptr)
    , disconnectButton_(nullptr)
    , videoDisplay_(nullptr)
    , selectedCameraIndex_(-1)
{
    cameraManager_ = std::make_shared<core::CameraManager>();
}

CameraPage::~CameraPage() = default;

void CameraPage::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Create splitter for camera list and video display
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter);

    // Left panel: Camera list and controls
    auto* leftWidget = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);

    cameraList_ = new QListWidget(leftWidget);
    leftLayout->addWidget(cameraList_);

    // Camera control buttons
    auto* buttonLayout = new QHBoxLayout;
    refreshButton_ = new QPushButton(tr("Refresh"), leftWidget);
    connectButton_ = new QPushButton(tr("Connect"), leftWidget);
    disconnectButton_ = new QPushButton(tr("Disconnect"), leftWidget);

    // Disable buttons until a camera is selected
    connectButton_->setEnabled(false);
    disconnectButton_->setEnabled(false);

    buttonLayout->addWidget(refreshButton_);
    buttonLayout->addWidget(connectButton_);
    buttonLayout->addWidget(disconnectButton_);
    leftLayout->addLayout(buttonLayout);

    // Camera controls
    cameraControl_ = new CameraControlWidget(leftWidget);
    cameraControl_->setEnabled(false);
    leftLayout->addWidget(cameraControl_);

    // Right panel: Video display
    videoDisplay_ = new VideoDisplayWidget(splitter);

    // Set initial splitter sizes
    splitter->setSizes({200, 600});

    // Update the camera list
    updateCameraList();
}

void CameraPage::createConnections() {
    connect(refreshButton_, &QPushButton::clicked,
            this, &CameraPage::onRefreshCameras);

    connect(connectButton_, &QPushButton::clicked,
            this, &CameraPage::onConnectCamera);

    connect(disconnectButton_, &QPushButton::clicked,
            this, &CameraPage::onDisconnectCamera);

    connect(cameraList_, &QListWidget::currentRowChanged,
            this, &CameraPage::onCameraSelected);

    connect(cameraControl_, &CameraControlWidget::statusChanged,
            this, &CameraPage::onCameraStatusChanged);

    // Connect camera manager signals
    connect(cameraManager_.get(), &core::CameraManager::camerasChanged,
            this, &CameraPage::updateCameraList);
}

void CameraPage::initialize() {
    Page::initialize();
    loadSettings();
}

void CameraPage::cleanup() {
    saveSettings();
    cameraManager_->disconnectAllCameras();
    Page::cleanup();
}

void CameraPage::loadSettings() {
    // In a real implementation, we would load camera settings from storage
}

void CameraPage::saveSettings() {
    // In a real implementation, we would save camera settings to storage
}

void CameraPage::onRefreshCameras() {
    emit statusChanged(tr("Refreshing cameras..."));
    videoDisplay_->clearFrame();
    cameraManager_->scanForCameras();
    emit statusChanged(tr("Cameras refreshed"));
}

void CameraPage::onCameraSelected(int index) {
    // Disconnect from previous camera if needed
    if (selectedCameraIndex_ >= 0) {
        auto mockCamera = cameraManager_->getMockCameraByIndex(selectedCameraIndex_);
        if (mockCamera && mockCamera->isConnected()) {
            mockCamera->disconnectCamera();
        }

        // Disconnect old signal connections
        if (mockCamera) {
            mockCamera->disconnect(this); // This is OK - using QObject::disconnect
        }
    }

    selectedCameraIndex_ = index;

    if (index >= 0) {
        auto camera = cameraManager_->getCameraByIndex(index);
        if (camera) {
            connectButton_->setEnabled(!camera->isConnected());
            disconnectButton_->setEnabled(camera->isConnected());
            cameraControl_->setEnabled(true);
            cameraControl_->setCameraIndex(index);

            // Connect to frame signals from this camera
            auto mockCamera = cameraManager_->getMockCameraByIndex(index);
            if (mockCamera) {
                connect(mockCamera.get(), &core::MockCamera::newFrameAvailable,
                        this, &CameraPage::onNewFrame);

                // If already connected, display frames
                if (mockCamera->isConnected()) {
                    onNewFrame(mockCamera->getFrame());
                } else {
                    videoDisplay_->clearFrame();
                }
            }

            emit statusChanged(tr("Selected camera: %1").arg(QString::fromStdString(camera->getName())));
        }
    } else {
        connectButton_->setEnabled(false);
        disconnectButton_->setEnabled(false);
        cameraControl_->setEnabled(false);
        videoDisplay_->clearFrame();
    }
}

void CameraPage::onConnectCamera() {
    if (selectedCameraIndex_ >= 0) {
        emit statusChanged(tr("Connecting to camera..."));

        if (cameraManager_->connectCamera(selectedCameraIndex_)) {
            connectButton_->setEnabled(false);
            disconnectButton_->setEnabled(true);
            emit statusChanged(tr("Camera connected"));
        } else {
            emit error(tr("Failed to connect to camera"));
        }
    }
}

void CameraPage::onDisconnectCamera() {
    if (selectedCameraIndex_ >= 0) {
        emit statusChanged(tr("Disconnecting from camera..."));

        if (cameraManager_->disconnectCamera(selectedCameraIndex_)) {
            connectButton_->setEnabled(true);
            disconnectButton_->setEnabled(false);
            videoDisplay_->clearFrame();
            emit statusChanged(tr("Camera disconnected"));
        } else {
            emit error(tr("Failed to disconnect from camera"));
        }
    }
}

void CameraPage::updateCameraList() {
    cameraList_->clear();
    auto cameras = cameraManager_->getCameras();

    for (size_t i = 0; i < cameras.size(); ++i) {
        QString itemText = QString("%1: %2")
            .arg(i)
            .arg(QString::fromStdString(cameras[i]->getName()));
        cameraList_->addItem(itemText);
    }

    // Reset selection
    selectedCameraIndex_ = -1;
    cameraControl_->setEnabled(false);
    connectButton_->setEnabled(false);
    disconnectButton_->setEnabled(false);
}

void CameraPage::onCameraStatusChanged(const QString& status) {
    emit statusChanged(status);
}

void CameraPage::onNewFrame(const QImage& frame) {
    videoDisplay_->updateFrame(frame);
}

} // namespace cam_matrix::ui
