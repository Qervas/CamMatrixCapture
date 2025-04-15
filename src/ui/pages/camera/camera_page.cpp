#include "camera_page.hpp"
#include "ui/widgets/camera_control_widget.hpp"
#include "ui/widgets/video_display_widget.hpp"
#include "ui/widgets/sapera_status_widget.hpp"
#include "ui/dialogs/camera_test_dialog.hpp"
#include "ui/dialogs/direct_camera_dialog.hpp"
#include "core/sapera_defs.hpp"
#include "core/camera_manager.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QGroupBox>
#include <QDebug>
#include <QTimer>
#include <QDir>
#include <QDateTime>

namespace cam_matrix::ui {

CameraPage::CameraPage(QWidget* parent)
    : Page(parent)
    , cameraList_(nullptr)
    , cameraControl_(nullptr)
    , refreshButton_(nullptr)
    , connectButton_(nullptr)
    , disconnectButton_(nullptr)
    , testSaperaButton_(nullptr)
    , directCameraButton_(nullptr)
    , videoDisplay_(nullptr)
    , saperaStatus_(nullptr)
    , selectedCameraIndex_(-1)
{
    // Create the camera manager
    cameraManager_ = std::make_unique<core::CameraManager>(this);
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

    // Sapera Status Widget
    saperaStatus_ = new SaperaStatusWidget(leftWidget);
    leftLayout->addWidget(saperaStatus_);
    
    // Camera list in a group box
    auto* cameraListGroup = new QGroupBox(tr("Available Cameras"), leftWidget);
    auto* cameraListLayout = new QVBoxLayout(cameraListGroup);
    
    cameraList_ = new QListWidget(cameraListGroup);
    cameraListLayout->addWidget(cameraList_);

    // Camera control buttons
    auto* buttonLayout = new QHBoxLayout;
    refreshButton_ = new QPushButton(tr("Refresh"), cameraListGroup);
    connectButton_ = new QPushButton(tr("Connect"), cameraListGroup);
    disconnectButton_ = new QPushButton(tr("Disconnect"), cameraListGroup);

    // Disable buttons until a camera is selected
    connectButton_->setEnabled(false);
    disconnectButton_->setEnabled(false);

    buttonLayout->addWidget(refreshButton_);
    buttonLayout->addWidget(connectButton_);
    buttonLayout->addWidget(disconnectButton_);
    cameraListLayout->addLayout(buttonLayout);
    
    leftLayout->addWidget(cameraListGroup);

    // Camera controls
    cameraControl_ = new CameraControlWidget(leftWidget);
    cameraControl_->setEnabled(false);
    leftLayout->addWidget(cameraControl_);

    // Buttons for advanced features
    auto* advancedButtonLayout = new QHBoxLayout;
    testSaperaButton_ = new QPushButton(tr("Test Sapera Camera"), leftWidget);
    directCameraButton_ = new QPushButton(tr("Direct Camera Access"), leftWidget);
    advancedButtonLayout->addWidget(testSaperaButton_);
    advancedButtonLayout->addWidget(directCameraButton_);
    leftLayout->addLayout(advancedButtonLayout);

    // Right panel: Video display
    videoDisplay_ = new VideoDisplayWidget(splitter);

    // Set initial splitter sizes
    splitter->setSizes({300, 700});

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
            
    connect(cameraControl_, &CameraControlWidget::capturePhotoRequested,
            this, &CameraPage::onCapturePhotoRequested);

    connect(testSaperaButton_, &QPushButton::clicked,
            this, &CameraPage::onTestSaperaCamera);
            
    connect(directCameraButton_, &QPushButton::clicked,
            this, &CameraPage::onDirectCameraAccess);

    connect(cameraManager_.get(), &core::CameraManager::statusChanged,
            this, &CameraPage::onManagerStatusChanged);
            
    connect(saperaStatus_, &SaperaStatusWidget::statusChanged,
            this, &CameraPage::onCameraStatusChanged);
}

void CameraPage::initialize() {
    Page::initialize();
    loadSettings();
    
    // Refresh the Sapera status to show current status at startup
    saperaStatus_->refresh();
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
    updateCameraList();
    emit statusChanged(tr("Cameras refreshed"));
}

void CameraPage::onCameraSelected(int index) {
    // Disconnect from previous camera
    if (selectedCameraIndex_ >= 0) {
        auto camera = cameraManager_->getCameraByIndex(selectedCameraIndex_);
        if (camera && camera->isConnected()) {
            camera->disconnectCamera();
        }

        // Disconnect signal connections for the old camera
        auto saperaCamera = cameraManager_->getSaperaCameraByIndex(selectedCameraIndex_);
        if (saperaCamera) {
            saperaCamera->disconnect(this);
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
            auto saperaCamera = cameraManager_->getSaperaCameraByIndex(index);
            if (saperaCamera) {
                qDebug() << "Connecting to new frame signals from camera" << QString::fromStdString(saperaCamera->getName());
                
                // For thread safety, use a queued connection to receive frame signals
                connect(saperaCamera, &core::SaperaCamera::newFrameAvailable,
                        this, &CameraPage::onNewFrame, Qt::QueuedConnection);

                // If already connected, display frames
                if (saperaCamera->isConnected()) {
                    qDebug() << "Camera is already connected, getting current frame";
                    QImage initialFrame = saperaCamera->getFrame();
                    if (!initialFrame.isNull()) {
                        qDebug() << "Got valid frame with size:" << initialFrame.width() << "x" << initialFrame.height();
                        onNewFrame(initialFrame);
                    } else {
                        qDebug() << "Camera returned null frame";
                        videoDisplay_->clearFrame();
                    }
                } else {
                    qDebug() << "Camera not connected yet";
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
        
        qDebug() << "Attempting to connect to camera index:" << selectedCameraIndex_;

        if (cameraManager_->connectCamera(selectedCameraIndex_)) {
            qDebug() << "Camera connected successfully";
            connectButton_->setEnabled(false);
            disconnectButton_->setEnabled(true);
            
            // Try to get an initial frame after connection
            auto saperaCamera = cameraManager_->getSaperaCameraByIndex(selectedCameraIndex_);
            if (saperaCamera) {
                // Wait a brief moment for the camera to initialize
                QTimer::singleShot(500, this, [this, saperaCamera]() {
                    qDebug() << "Getting initial frame after connection";
                    QImage initialFrame = saperaCamera->getFrame();
                    if (!initialFrame.isNull()) {
                        qDebug() << "Initial frame received, size:" << initialFrame.width() << "x" << initialFrame.height();
                        videoDisplay_->updateFrame(initialFrame);
                    } else {
                        qDebug() << "Initial frame was null";
                    }
                });
            }
            
            emit statusChanged(tr("Camera connected"));
        } else {
            qDebug() << "Camera connection failed";
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

void CameraPage::onTestSaperaCamera() {
    CameraTestDialog dialog(this);
    dialog.exec();
}

void CameraPage::onDirectCameraAccess() {
    DirectCameraDialog dialog(this);
    dialog.exec();
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

void CameraPage::onManagerStatusChanged(const std::string& status) {
    emit statusChanged(QString::fromStdString(status));
}

void CameraPage::onNewFrame(const QImage& frame) {
    qDebug() << "New frame received, size:" << frame.width() << "x" << frame.height();
    if (!frame.isNull()) {
        videoDisplay_->updateFrame(frame);
    } else {
        qDebug() << "Received null frame";
    }
}

void CameraPage::onCapturePhotoRequested(int cameraIndex) {
    qDebug() << "Capture photo requested for camera index:" << cameraIndex;
    
    if (cameraIndex < 0) {
        emit statusChanged(tr("No camera selected for photo capture"));
        return;
    }
    
    auto saperaCamera = cameraManager_->getSaperaCameraByIndex(cameraIndex);
    if (!saperaCamera) {
        emit statusChanged(tr("Failed to get camera for photo capture"));
        return;
    }
    
    if (!saperaCamera->isConnected()) {
        emit statusChanged(tr("Camera not connected. Connect the camera before capturing photos."));
        return;
    }
    
    // Create a folder for captured photos if it doesn't exist
    QDir captureDir("captures");
    if (!captureDir.exists()) {
        if (!captureDir.mkpath(".")) {
            emit statusChanged(tr("Failed to create directory for photo captures"));
            return;
        }
    }
    
    // Generate a filename with timestamp
    QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
    QString fileName = QString("captures/%1_%2.png")
                            .arg(QString::fromStdString(saperaCamera->getName()))
                            .arg(timeStamp);
    
    // Connect to photoCaptured signal if not already connected
    if (!connect(saperaCamera, &core::SaperaCamera::photoCaptured,
                this, &CameraPage::onPhotoCaptured, Qt::UniqueConnection)) {
        qDebug() << "Connected to photoCaptured signal";
    }
    
    // Trigger the photo capture
    if (saperaCamera->capturePhoto(fileName.toStdString())) {
        emit statusChanged(tr("Capturing photo from camera %1...").arg(cameraIndex));
    } else {
        emit statusChanged(tr("Failed to capture photo from camera %1").arg(cameraIndex));
    }
}

void CameraPage::onPhotoCaptured(const QImage& image, const std::string& path) {
    qDebug() << "Photo captured and saved to:" << QString::fromStdString(path);
    emit statusChanged(tr("Photo captured and saved to: %1").arg(QString::fromStdString(path)));
}

} // namespace cam_matrix::ui
