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
#include <QMetaObject>

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
    try {
        // Disconnect all signal connections first to prevent deadlocks
        if (selectedCameraIndex_ >= 0) {
            auto saperaCamera = cameraManager_->getSaperaCameraByIndex(selectedCameraIndex_);
            if (saperaCamera) {
                // Disconnect from frame signals to prevent deadlocks during shutdown
                disconnect(saperaCamera, &core::sapera::SaperaCamera::newFrameAvailable,
                          this, &CameraPage::onNewFrame);
                disconnect(saperaCamera, &core::sapera::SaperaCamera::photoCaptured,
                          this, &CameraPage::onPhotoCaptured);
            }
        }
        
        // Now it's safe to disconnect all cameras
        saveSettings();
        cameraManager_->disconnectAllCameras();
        
        videoDisplay_->clearFrame();
        Page::cleanup();
    } catch (const std::exception& e) {
        qWarning() << "Exception in CameraPage::cleanup:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception in CameraPage::cleanup";
    }
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
    try {
        // Disconnect from previous camera
        if (selectedCameraIndex_ >= 0) {
            auto camera = cameraManager_->getCameraByIndex(selectedCameraIndex_);
            if (camera && camera->isConnected()) {
                camera->disconnectCamera();
            }

            // Disconnect signal connections for the old camera
            auto saperaCamera = cameraManager_->getSaperaCameraByIndex(selectedCameraIndex_);
            if (saperaCamera) {
                // Use disconnect with specific signals to avoid disconnecting all signals
                disconnect(saperaCamera, &core::sapera::SaperaCamera::newFrameAvailable,
                          this, &CameraPage::onNewFrame);
                disconnect(saperaCamera, &core::sapera::SaperaCamera::photoCaptured,
                          this, &CameraPage::onPhotoCaptured);
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
                    connect(saperaCamera, &core::sapera::SaperaCamera::newFrameAvailable,
                            this, &CameraPage::onNewFrame, Qt::QueuedConnection);

                    // If already connected, display frames
                    if (saperaCamera->isConnected()) {
                        qDebug() << "Camera is already connected, getting current frame";
                        // Get the initial frame in a thread-safe way
                        QImage initialFrame = saperaCamera->getFrame();
                        if (!initialFrame.isNull()) {
                            qDebug() << "Got valid frame with size:" << initialFrame.width() << "x" << initialFrame.height();
                            
                            // Use the same queued approach as onNewFrame for consistency
                            QMetaObject::invokeMethod(this, [this, initialFrame]() {
                                try {
                                    // Create a deep copy
                                    QImage frameCopy = initialFrame.copy();
                                    videoDisplay_->updateFrame(frameCopy);
                                } catch (const std::exception& e) {
                                    qWarning() << "Exception in initial frame update:" << e.what();
                                } catch (...) {
                                    qWarning() << "Unknown exception in initial frame update";
                                }
                            }, Qt::QueuedConnection);
                        } else {
                            qDebug() << "Camera returned null frame";
                            // Use queued connection for clearing frame
                            QMetaObject::invokeMethod(videoDisplay_, "clearFrame", Qt::QueuedConnection);
                        }
                    } else {
                        qDebug() << "Camera not connected yet";
                        // Use queued connection for clearing frame
                        QMetaObject::invokeMethod(videoDisplay_, "clearFrame", Qt::QueuedConnection);
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
    } catch (const std::exception& e) {
        qWarning() << "Exception in onCameraSelected:" << e.what();
        emit error(tr("Error selecting camera: %1").arg(e.what()));
    } catch (...) {
        qWarning() << "Unknown exception in onCameraSelected";
        emit error(tr("Unknown error selecting camera"));
    }
}

void CameraPage::onConnectCamera() {
    try {
        if (selectedCameraIndex_ >= 0) {
            emit statusChanged(tr("Connecting to camera..."));
            
            qDebug() << "Attempting to connect to camera index:" << selectedCameraIndex_;

            if (cameraManager_->connectCamera(selectedCameraIndex_)) {
                qDebug() << "Camera connected successfully";
                connectButton_->setEnabled(false);
                disconnectButton_->setEnabled(true);
                
                // The camera is now connected and will emit newFrameAvailable signals
                // which are already connected to onNewFrame via the onCameraSelected method
                // No need to manually get a frame here, which could cause deadlocks
                
                emit statusChanged(tr("Camera connected"));
            } else {
                qDebug() << "Camera connection failed";
                emit error(tr("Failed to connect to camera"));
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in onConnectCamera:" << e.what();
        emit error(tr("Error connecting to camera: %1").arg(e.what()));
    } catch (...) {
        qWarning() << "Unknown exception in onConnectCamera";
        emit error(tr("Unknown error connecting to camera"));
    }
}

void CameraPage::onDisconnectCamera() {
    try {
        if (selectedCameraIndex_ >= 0) {
            emit statusChanged(tr("Disconnecting from camera..."));

            // First disconnect from signals to prevent any deadlocks
            auto saperaCamera = cameraManager_->getSaperaCameraByIndex(selectedCameraIndex_);
            if (saperaCamera) {
                disconnect(saperaCamera, &core::sapera::SaperaCamera::newFrameAvailable,
                          this, &CameraPage::onNewFrame);
                disconnect(saperaCamera, &core::sapera::SaperaCamera::photoCaptured,
                          this, &CameraPage::onPhotoCaptured);
            }

            // Then disconnect the camera
            if (cameraManager_->disconnectCamera(selectedCameraIndex_)) {
                connectButton_->setEnabled(true);
                disconnectButton_->setEnabled(false);
                videoDisplay_->clearFrame();
                emit statusChanged(tr("Camera disconnected"));
            } else {
                emit error(tr("Failed to disconnect from camera"));
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in onDisconnectCamera:" << e.what();
        emit error(tr("Error disconnecting camera: %1").arg(e.what()));
        
        // Try to reset UI state
        connectButton_->setEnabled(true);
        disconnectButton_->setEnabled(false);
        videoDisplay_->clearFrame();
    } catch (...) {
        qWarning() << "Unknown exception in onDisconnectCamera";
        emit error(tr("Unknown error disconnecting camera"));
        
        // Try to reset UI state
        connectButton_->setEnabled(true);
        disconnectButton_->setEnabled(false);
        videoDisplay_->clearFrame();
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
    try {
        qDebug() << "New frame received, size:" << frame.width() << "x" << frame.height();
        
        if (!frame.isNull()) {
            // Create a deep copy of the frame to ensure thread safety
            QImage frameCopy = frame.copy();
            
            // Use QMetaObject::invokeMethod to update the UI with a queued connection
            // This prevents deadlocks by not blocking the calling thread
            QMetaObject::invokeMethod(this, [this, frameCopy]() {
                try {
                    videoDisplay_->updateFrame(frameCopy);
                } catch (const std::exception& e) {
                    qWarning() << "Exception in frame update:" << e.what();
                } catch (...) {
                    qWarning() << "Unknown exception in frame update";
                }
            }, Qt::QueuedConnection);
        } else {
            qDebug() << "Received null frame";
            
            // Also use queued connection for clearing frame
            QMetaObject::invokeMethod(videoDisplay_, "clearFrame", Qt::QueuedConnection);
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in onNewFrame:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception in onNewFrame";
    }
}

void CameraPage::onCapturePhotoRequested(int cameraIndex) {
    try {
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
        if (!connect(saperaCamera, &core::sapera::SaperaCamera::photoCaptured,
                    this, &CameraPage::onPhotoCaptured, Qt::UniqueConnection)) {
            qDebug() << "Connected to photoCaptured signal";
        }
        
        // Trigger the photo capture
        if (saperaCamera->capturePhoto(fileName.toStdString())) {
            emit statusChanged(tr("Capturing photo from camera %1...").arg(cameraIndex));
        } else {
            emit statusChanged(tr("Failed to capture photo from camera %1").arg(cameraIndex));
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in onCapturePhotoRequested:" << e.what();
        emit error(tr("Error capturing photo: %1").arg(e.what()));
    } catch (...) {
        qWarning() << "Unknown exception in onCapturePhotoRequested";
        emit error(tr("Unknown error capturing photo"));
    }
}

void CameraPage::onPhotoCaptured(const QImage& image, const std::string& path) {
    try {
        qDebug() << "Photo captured and saved to:" << QString::fromStdString(path);
        emit statusChanged(tr("Photo captured and saved to: %1").arg(QString::fromStdString(path)));
    } catch (const std::exception& e) {
        qWarning() << "Exception in onPhotoCaptured:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception in onPhotoCaptured";
    }
}

} // namespace cam_matrix::ui
