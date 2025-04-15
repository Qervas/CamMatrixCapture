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
#include <QFileDialog>
#include <QProgressBar>

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
    , syncGroup_(nullptr)
    , clearSelectionButton_(nullptr)
    , toggleSelectButton_(nullptr)
    , connectSelectedButton_(nullptr)
    , disconnectSelectedButton_(nullptr)
    , captureSyncButton_(nullptr)
    , syncProgressBar_(nullptr)
    , syncStatusLabel_(nullptr)
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
    // Enable multiple selection with checkboxes
    cameraList_->setSelectionMode(QAbstractItemView::SingleSelection);
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

    // Multi-camera synchronization group
    syncGroup_ = new QGroupBox(tr("Multi-Camera Synchronization"), leftWidget);
    auto* syncLayout = new QVBoxLayout(syncGroup_);
    
    // Sync camera selection buttons
    auto* syncSelectionLayout = new QHBoxLayout;
    toggleSelectButton_ = new QPushButton(tr("Select All"), syncGroup_);
    clearSelectionButton_ = new QPushButton(tr("Clear Selection"), syncGroup_);
    syncSelectionLayout->addWidget(toggleSelectButton_);
    syncSelectionLayout->addWidget(clearSelectionButton_);
    syncLayout->addLayout(syncSelectionLayout);
    
    // Sync camera connection buttons
    auto* syncConnectionLayout = new QHBoxLayout;
    connectSelectedButton_ = new QPushButton(tr("Connect Selected"), syncGroup_);
    disconnectSelectedButton_ = new QPushButton(tr("Disconnect Selected"), syncGroup_);
    syncConnectionLayout->addWidget(connectSelectedButton_);
    syncConnectionLayout->addWidget(disconnectSelectedButton_);
    syncLayout->addLayout(syncConnectionLayout);
    
    // Sync camera capture button
    captureSyncButton_ = new QPushButton(tr("Capture Photos Sync"), syncGroup_);
    syncLayout->addWidget(captureSyncButton_);
    
    // Sync progress bar and status label
    syncProgressBar_ = new QProgressBar(syncGroup_);
    syncProgressBar_->setMinimum(0);
    syncProgressBar_->setMaximum(100);
    syncProgressBar_->setValue(0);
    syncLayout->addWidget(syncProgressBar_);
    
    syncStatusLabel_ = new QLabel(tr("Ready for synchronized capture"), syncGroup_);
    syncLayout->addWidget(syncStatusLabel_);
    
    leftLayout->addWidget(syncGroup_);

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
    
    // Initial update of sync UI elements
    updateSyncUI();
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
            
    // New connections for multi-camera synchronization
    connect(cameraList_, &QListWidget::itemChanged,
            this, &CameraPage::onCameraSelectionChanged);
            
    connect(toggleSelectButton_, &QPushButton::clicked,
            this, &CameraPage::onToggleSelectAll);
            
    connect(clearSelectionButton_, &QPushButton::clicked,
            this, &CameraPage::onClearSelection);
            
    connect(connectSelectedButton_, &QPushButton::clicked,
            this, &CameraPage::onConnectSelectedCameras);
            
    connect(disconnectSelectedButton_, &QPushButton::clicked,
            this, &CameraPage::onDisconnectSelectedCameras);
            
    connect(captureSyncButton_, &QPushButton::clicked,
            this, &CameraPage::onCaptureSync);
            
    // Connect to camera manager signals for synchronized capture
    connect(cameraManager_.get(), &core::CameraManager::syncCaptureProgress,
            this, &CameraPage::onSyncCaptureProgress, Qt::QueuedConnection);
            
    connect(cameraManager_.get(), &core::CameraManager::syncCaptureComplete,
            this, &CameraPage::onSyncCaptureComplete, Qt::QueuedConnection);
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
    // Disconnect item change signals temporarily to prevent triggering during update
    disconnect(cameraList_, &QListWidget::itemChanged,
              this, &CameraPage::onCameraSelectionChanged);
    
    cameraList_->clear();
    auto cameras = cameraManager_->getCameras();

    for (size_t i = 0; i < cameras.size(); ++i) {
        QString itemText = QString("%1: %2")
            .arg(i)
            .arg(QString::fromStdString(cameras[i]->getName()));
        
        auto item = new QListWidgetItem(itemText, cameraList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, static_cast<int>(i)); // Store camera index
    }

    // Reset selection
    selectedCameraIndex_ = -1;
    cameraControl_->setEnabled(false);
    connectButton_->setEnabled(false);
    disconnectButton_->setEnabled(false);
    
    // Reconnect item change signals
    connect(cameraList_, &QListWidget::itemChanged,
           this, &CameraPage::onCameraSelectionChanged);
    
    // Update multi-camera sync UI
    updateSyncUI();
}

void CameraPage::updateSyncUI() {
    auto selectedCameras = cameraManager_->getSelectedCameras();
    bool hasSelection = !selectedCameras.empty();
    bool allSelected = hasSelection && selectedCameras.size() == static_cast<size_t>(cameraList_->count());
    
    // Enable/disable buttons based on selection
    toggleSelectButton_->setEnabled(cameraList_->count() > 0);
    clearSelectionButton_->setEnabled(hasSelection);
    connectSelectedButton_->setEnabled(hasSelection);
    disconnectSelectedButton_->setEnabled(hasSelection);
    captureSyncButton_->setEnabled(hasSelection);
    
    // Update toggle button text based on selection state
    toggleSelectButton_->setText(allSelected ? tr("Deselect All") : tr("Select All"));
    
    // Update status label
    if (hasSelection) {
        syncStatusLabel_->setText(tr("%1 cameras selected for sync").arg(selectedCameras.size()));
    } else {
        syncStatusLabel_->setText(tr("No cameras selected for sync"));
    }
    
    // Reset progress bar
    syncProgressBar_->setValue(0);
}

// New multi-camera synchronization slot implementations
void CameraPage::onCameraSelectionChanged(QListWidgetItem* item) {
    if (!item) return;
    
    // Get camera index from item data
    int cameraIndex = item->data(Qt::UserRole).toInt();
    bool selected = item->checkState() == Qt::Checked;
    
    // Update camera selection in manager
    cameraManager_->selectCameraForSync(cameraIndex, selected);
    
    // Update UI
    updateSyncUI();
}

void CameraPage::onToggleSelectAll() {
    // Check if all cameras are currently selected
    bool allSelected = areAllCamerasSelected();
    
    // Disconnect item change signals temporarily to prevent multiple triggers
    disconnect(cameraList_, &QListWidget::itemChanged,
               this, &CameraPage::onCameraSelectionChanged);
    
    if (allSelected) {
        // If all are selected, deselect all
        for (int i = 0; i < cameraList_->count(); ++i) {
            cameraList_->item(i)->setCheckState(Qt::Unchecked);
        }
        cameraManager_->clearCameraSelection();
        emit statusChanged(tr("All cameras deselected"));
    } else {
        // If not all are selected, select all
        for (int i = 0; i < cameraList_->count(); ++i) {
            QListWidgetItem* item = cameraList_->item(i);
            item->setCheckState(Qt::Checked);
            
            int cameraIndex = item->data(Qt::UserRole).toInt();
            cameraManager_->selectCameraForSync(cameraIndex, true);
        }
        emit statusChanged(tr("All cameras selected for synchronization"));
    }
    
    // Reconnect item change signals
    connect(cameraList_, &QListWidget::itemChanged,
            this, &CameraPage::onCameraSelectionChanged);
    
    // Update UI
    updateSyncUI();
}

void CameraPage::onClearSelection() {
    // Disconnect item change signals temporarily to prevent multiple triggers
    disconnect(cameraList_, &QListWidget::itemChanged,
              this, &CameraPage::onCameraSelectionChanged);
    
    // Uncheck all cameras in the list
    for (int i = 0; i < cameraList_->count(); ++i) {
        cameraList_->item(i)->setCheckState(Qt::Unchecked);
    }
    
    // Clear selection in manager
    cameraManager_->clearCameraSelection();
    
    // Reconnect item change signals
    connect(cameraList_, &QListWidget::itemChanged,
           this, &CameraPage::onCameraSelectionChanged);
    
    // Update UI
    updateSyncUI();
    
    emit statusChanged(tr("Camera selection cleared"));
}

void CameraPage::onConnectSelectedCameras() {
    try {
        emit statusChanged(tr("Connecting selected cameras..."));
        
        // Connect selected cameras
        bool success = cameraManager_->connectSelectedCameras();
        
        if (!success) {
            emit error(tr("Failed to connect one or more selected cameras"));
        }
        
        // Update UI
        updateSyncUI();
    } catch (const std::exception& e) {
        qWarning() << "Exception in onConnectSelectedCameras:" << e.what();
        emit error(tr("Error connecting selected cameras: %1").arg(e.what()));
    } catch (...) {
        qWarning() << "Unknown exception in onConnectSelectedCameras";
        emit error(tr("Unknown error connecting selected cameras"));
    }
}

void CameraPage::onDisconnectSelectedCameras() {
    try {
        emit statusChanged(tr("Disconnecting selected cameras..."));
        
        // Disconnect from signals to prevent deadlocks
        auto selectedCameras = cameraManager_->getSelectedCameras();
        for (size_t index : selectedCameras) {
            auto saperaCamera = cameraManager_->getSaperaCameraByIndex(index);
            if (saperaCamera) {
                disconnect(saperaCamera, &core::sapera::SaperaCamera::newFrameAvailable,
                          this, &CameraPage::onNewFrame);
                disconnect(saperaCamera, &core::sapera::SaperaCamera::photoCaptured,
                          this, &CameraPage::onPhotoCaptured);
            }
        }
        
        // Disconnect selected cameras
        bool success = cameraManager_->disconnectSelectedCameras();
        
        if (!success) {
            emit error(tr("Failed to disconnect one or more selected cameras"));
        }
        
        // Update UI
        updateSyncUI();
    } catch (const std::exception& e) {
        qWarning() << "Exception in onDisconnectSelectedCameras:" << e.what();
        emit error(tr("Error disconnecting selected cameras: %1").arg(e.what()));
    } catch (...) {
        qWarning() << "Unknown exception in onDisconnectSelectedCameras";
        emit error(tr("Unknown error disconnecting selected cameras"));
    }
}

void CameraPage::onCaptureSync() {
    try {
        // Check if there are selected cameras
        auto selectedCameras = cameraManager_->getSelectedCameras();
        if (selectedCameras.empty()) {
            emit statusChanged(tr("No cameras selected for synchronized capture"));
            return;
        }
        
        // Ask for save directory
        QString dirPath = QFileDialog::getExistingDirectory(this, 
                                                           tr("Select Directory for Synchronized Captures"),
                                                           "captures",
                                                           QFileDialog::ShowDirsOnly);
        
        if (dirPath.isEmpty()) {
            // User canceled the dialog
            emit statusChanged(tr("Synchronized capture canceled"));
            return;
        }
        
        // Generate a timestamp for this capture session
        QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss-zzz");
        
        // Create a timestamped folder for this sync capture session
        QString captureSessionPath = dirPath + "/sync_" + timeStamp;
        QDir captureDir(captureSessionPath);
        if (!captureDir.exists() && !captureDir.mkpath(".")) {
            emit statusChanged(tr("Failed to create directory for synchronized captures"));
            return;
        }
        
        // Reset progress bar and update status
        syncProgressBar_->setValue(0);
        syncStatusLabel_->setText(tr("Starting synchronized capture..."));
        emit statusChanged(tr("Starting synchronized capture with %1 cameras...").arg(selectedCameras.size()));
        
        // Connect to any camera photo signals if not already connected
        for (size_t index : selectedCameras) {
            auto saperaCamera = cameraManager_->getSaperaCameraByIndex(index);
            if (saperaCamera) {
                connect(saperaCamera, &core::sapera::SaperaCamera::photoCaptured,
                        this, &CameraPage::onPhotoCaptured, Qt::UniqueConnection);
            }
        }
        
        // Start the synchronized capture with the timestamped folder path
        bool success = cameraManager_->capturePhotosSync(captureSessionPath.toStdString());
        
        // Update UI based on the initial result
        if (!success) {
            syncStatusLabel_->setText(tr("Failed to start synchronized capture"));
            emit error(tr("Failed to start synchronized capture"));
        } else {
            emit statusChanged(tr("Synchronized capture started in folder: %1").arg(captureSessionPath));
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in onCaptureSync:" << e.what();
        emit error(tr("Error during synchronized capture: %1").arg(e.what()));
        syncStatusLabel_->setText(tr("Error during synchronized capture"));
        syncProgressBar_->setValue(0);
    } catch (...) {
        qWarning() << "Unknown exception in onCaptureSync";
        emit error(tr("Unknown error during synchronized capture"));
        syncStatusLabel_->setText(tr("Unknown error during synchronized capture"));
        syncProgressBar_->setValue(0);
    }
}

void CameraPage::onSyncCaptureProgress(int current, int total) {
    try {
        // Update progress bar
        syncProgressBar_->setMaximum(total);
        syncProgressBar_->setValue(current);
        
        // Update status label
        syncStatusLabel_->setText(tr("Capturing photos: %1 of %2").arg(current).arg(total));
        
        // Update status bar
        emit statusChanged(tr("Synchronized capture in progress: %1 of %2").arg(current).arg(total));
    } catch (const std::exception& e) {
        qWarning() << "Exception in onSyncCaptureProgress:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception in onSyncCaptureProgress";
    }
}

void CameraPage::onSyncCaptureComplete(int successCount, int total) {
    try {
        // Update progress bar to show completion
        syncProgressBar_->setMaximum(total);
        syncProgressBar_->setValue(total);
        
        // Update status label
        syncStatusLabel_->setText(tr("Synchronized capture complete: %1 of %2 successful").arg(successCount).arg(total));
        
        // Update status bar
        emit statusChanged(tr("Synchronized capture complete: %1 of %2 cameras successful").arg(successCount).arg(total));
        
        // Show a message if some captures failed
        if (successCount < total) {
            emit error(tr("Some synchronized captures failed. Check camera connections."));
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in onSyncCaptureComplete:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception in onSyncCaptureComplete";
    }
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

bool CameraPage::areAllCamerasSelected() const {
    auto selectedCameras = cameraManager_->getSelectedCameras();
    return !selectedCameras.empty() && selectedCameras.size() == static_cast<size_t>(cameraList_->count());
}

} // namespace cam_matrix::ui
