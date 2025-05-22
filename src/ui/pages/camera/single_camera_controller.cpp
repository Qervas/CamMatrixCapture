#include "single_camera_controller.hpp"
#include "core/settings.hpp"
#include "core/sapera/sapera_camera_base.hpp"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QPainter>

namespace cam_matrix::ui
{

    SingleCameraController::SingleCameraController(QObject *parent)
        : QObject(parent)
    {
    }

    SingleCameraController::~SingleCameraController() = default;

    void SingleCameraController::initialize(
        std::shared_ptr<core::CameraManager> cameraManager,
        VideoDisplayWidget *videoDisplay,
        CameraControlWidget *cameraControl,
        QListWidget *cameraList,
        QPushButton *connectButton,
        QPushButton *disconnectButton)
    {
        cameraManager_ = cameraManager;
        videoDisplay_ = videoDisplay;
        cameraControl_ = cameraControl;
        cameraList_ = cameraList;
        connectButton_ = connectButton;
        disconnectButton_ = disconnectButton;

        // Connect signals
        connect(cameraControl_, &CameraControlWidget::photoCaptureRequested,
                this, &SingleCameraController::onPhotoCaptureRequested);
    }

    void SingleCameraController::setVisible(bool visible)
    {
        if (cameraControl_)
        {
            cameraControl_->setVisible(visible);
        }

        if (connectButton_)
        {
            connectButton_->setVisible(visible);
        }

        if (disconnectButton_)
        {
            disconnectButton_->setVisible(visible);
        }

        if (videoDisplay_)
        {
            videoDisplay_->setEnabled(visible);

            if (visible)
            {
                // Set all cameras to single camera mode when entering single camera view
                if (cameraManager_)
                {
                    cameraManager_->setAllCamerasToSingleMode();
                }

                // Update display if we have a valid camera selected
                if (selectedCameraIndex_ >= 0 && cameraManager_->isCameraConnected(selectedCameraIndex_))
                {
                    cam_matrix::core::sapera::SaperaCameraBase *camera = cameraManager_->getCameraByIndex(selectedCameraIndex_);
                    if (camera)
                    {
                        QImage currentFrame = camera->getLatestFrame();
                        if (!currentFrame.isNull())
                        {
                            videoDisplay_->updateFrame(currentFrame);
                        }
                    }
                }
            }
            else
            {
                // Display a message indicating that video preview is disabled in multi-camera mode
                QImage placeholderImage(640, 480, QImage::Format_RGB32);
                placeholderImage.fill(Qt::black);
                QPainter painter(&placeholderImage);
                painter.setPen(Qt::white);
                painter.setFont(QFont("Arial", 14));
                painter.drawText(placeholderImage.rect(), Qt::AlignCenter,
                                 "Video preview disabled in multi-camera mode.\nUse single camera mode for live preview.");
                videoDisplay_->updateFrame(placeholderImage);
            }
        }
    }

    void SingleCameraController::onCameraSelected(int index)
    {
        selectedCameraIndex_ = index;
        bool cameraSelected = (index >= 0);

        if (connectButton_)
        {
            connectButton_->setEnabled(cameraSelected && !cameraManager_->isCameraConnected(index));
        }

        if (disconnectButton_)
        {
            disconnectButton_->setEnabled(cameraSelected && cameraManager_->isCameraConnected(index));
        }

        if (cameraControl_)
        {
            cameraControl_->setEnabled(cameraSelected && cameraManager_->isCameraConnected(index));
            cameraControl_->setCameraIndex(cameraSelected && cameraManager_->isCameraConnected(index) ? index : -1);
        }

        // Only update video display if enabled (in single camera mode)
        if (cameraSelected && videoDisplay_ && videoDisplay_->isEnabled())
        {
            emit logMessage(QString("Camera selected: %1").arg(cameraList_->item(index)->text()));

            if (cameraManager_->isCameraConnected(index))
            {
                auto *camera = cameraManager_->getCameraByIndex(index);
                if (camera)
                {
                    QImage currentFrame = camera->getLatestFrame();
                    if (!currentFrame.isNull())
                    {
                        videoDisplay_->updateFrame(currentFrame);
                        emit logMessage("Updated video display with current frame on selection");
                    }
                }
            }
            else
            {
                videoDisplay_->clear(); // Clear display if selected camera is not connected
                emit logMessage("Selected camera not connected - controls updated, display cleared");
            }
        }
        else if (videoDisplay_ && !videoDisplay_->isEnabled())
        {
            // We're in multi-camera mode, don't update video display
            emit logMessage("Camera selected in multi-camera mode, not updating video display");
        }
        else if (videoDisplay_)
        {
            videoDisplay_->clear(); // Clear display if no camera is selected
        }

        emit updateUI();
    }

    void SingleCameraController::refreshCameraList()
    {
        emit updateUI();
    }

    void SingleCameraController::onConnectCamera()
    {
        int index = cameraList_->currentRow();
        if (index >= 0)
        {
            emit logMessage(QString("Connecting to camera at index %1...").arg(index));

            // Get the name of the camera for better logging
            QString cameraName = cameraList_->item(index)->text();
            emit logMessage(QString("Connecting to camera: %1").arg(cameraName));

            // Connect the camera
            bool success = cameraManager_->connectCamera(index);

            if (success)
            {
                emit logMessage(QString("Successfully connected to camera: %1").arg(cameraName), "SUCCESS");

                // Update UI state immediately
                if (disconnectButton_)
                {
                    disconnectButton_->setEnabled(true);
                }

                if (cameraControl_)
                {
                    cameraControl_->setEnabled(true);
                }

                // Automatically select this camera for sync operations
                QListWidgetItem *item = cameraList_->item(index);
                if (item)
                {
                    item->setCheckState(Qt::Checked);
                    cameraManager_->selectCameraForSync(index, true);
                }

                // Get the camera object directly and request an initial frame - ONLY if in single camera mode
                if (videoDisplay_ && videoDisplay_->isEnabled())
                {
                    try
                    {
                        cam_matrix::core::sapera::SaperaCameraBase *camera = cameraManager_->getCameraByIndex(index);
                        if (camera && camera->isConnected())
                        {
                            // Give a small delay to allow the camera to start streaming
                            QApplication::processEvents();

                            // Try to get a current frame
                            try
                            {
                                QImage currentFrame = camera->getLatestFrame();
                                if (!currentFrame.isNull())
                                {
                                    videoDisplay_->updateFrame(currentFrame);
                                    emit logMessage("Updated video display with initial frame");
                                }
                            }
                            catch (const std::exception &e)
                            {
                                emit logMessage(QString("Exception when getting initial frame: %1").arg(e.what()), "WARNING");
                            }
                            catch (...)
                            {
                                emit logMessage("Unknown exception when getting initial frame", "WARNING");
                            }
                        }
                    }
                    catch (...)
                    {
                        emit logMessage("Exception when accessing camera after connecting", "WARNING");
                    }
                }

                // Make sure to update UI after all changes
                QApplication::processEvents();
            }
            else
            {
                emit logMessage(QString("Failed to connect to camera: %1").arg(cameraName), "ERROR");
            }

            emit updateUI();
        }
    }

    void SingleCameraController::onDisconnectCamera()
    {
        int index = cameraList_->currentRow();
        if (index >= 0)
        {
            emit logMessage(QString("Disconnecting camera at index %1...").arg(index));

            // Update UI state immediately to prevent double-clicks or further interactions
            if (disconnectButton_)
            {
                disconnectButton_->setEnabled(false);
            }

            if (connectButton_)
            {
                connectButton_->setEnabled(false); // Will be enabled once disconnection is fully complete
            }

            if (cameraControl_)
            {
                cameraControl_->setEnabled(false);
            }

            // Clear selected camera index to prevent frame timer from accessing it during disconnect
            int previousSelectedIndex = selectedCameraIndex_;
            selectedCameraIndex_ = -1;

            // Clear display before disconnect to avoid accessing a disconnecting camera
            if (videoDisplay_)
            {
                videoDisplay_->clear();
            }

            // Process events to ensure UI updates before disconnecting
            QApplication::processEvents();

            // Disconnect the camera
            bool success = cameraManager_->disconnectCamera(index);

            // Update UI after disconnection
            QListWidgetItem *item = cameraList_->item(index);
            if (item)
            {
                item->setCheckState(Qt::Unchecked);
            }

            // Re-enable the connect button
            if (connectButton_)
            {
                connectButton_->setEnabled(true);
            }

            // Log result
            if (success)
            {
                emit logMessage(QString("Camera %1 successfully disconnected").arg(index), "SUCCESS");
            }
            else
            {
                emit logMessage(QString("Camera %1 disconnect may have had issues").arg(index), "WARNING");
            }

            emit updateUI();
        }
    }

    void SingleCameraController::onPhotoCaptureRequested()
    {
        if (selectedCameraIndex_ >= 0 && cameraManager_->isCameraConnected(selectedCameraIndex_))
        {
            // Auto-generate filepath
            QString timeStamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
            // Use app's home directory for simplicity
            QString basePath = QDir::homePath() + "/Pictures/CamMatrixCaptures";
            QString fileName = QString("%1/capture_%2.png").arg(basePath).arg(timeStamp);

            // Make sure the directory exists
            QDir dir(basePath);
            if (!dir.exists())
            {
                dir.mkpath(".");
                emit logMessage(QString("Created directory %1").arg(basePath), "INFO");
            }

            // Send empty path to trigger the auto-save in SaperaCameraBase::capturePhoto
            emit logMessage(QString("Capturing photo from camera %1 (auto-save)").arg(selectedCameraIndex_), "ACTION");
            bool success = cameraManager_->capturePhoto(selectedCameraIndex_, "");

            if (success)
            {
                emit logMessage(QString("Photo capture initiated successfully"), "SUCCESS");
            }
            else
            {
                emit logMessage(QString("Photo capture failed"), "ERROR");
            }
        }
        else
        {
            emit logMessage(QString("Cannot capture - camera %1 not connected").arg(selectedCameraIndex_), "ERROR");
        }
    }

} // namespace cam_matrix::ui