#include "multi_camera_controller.hpp"
#include "core/settings.hpp"
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QPainter>

namespace cam_matrix::ui
{

    MultiCameraController::MultiCameraController(QObject *parent)
        : QObject(parent)
    {
    }

    MultiCameraController::~MultiCameraController() = default;

    void MultiCameraController::initialize(
        std::shared_ptr<core::CameraManager> cameraManager,
        VideoDisplayWidget *videoDisplay,
        QListWidget *cameraList,
        QPushButton *toggleSelectButton,
        QPushButton *clearSelectionButton,
        QPushButton *connectSelectedButton,
        QPushButton *disconnectSelectedButton,
        QPushButton *captureSyncButton,
        QProgressBar *syncProgressBar,
        QLabel *syncStatusLabel)
    {
        cameraManager_ = cameraManager;
        videoDisplay_ = videoDisplay;
        cameraList_ = cameraList;
        toggleSelectButton_ = toggleSelectButton;
        clearSelectionButton_ = clearSelectionButton;
        connectSelectedButton_ = connectSelectedButton;
        disconnectSelectedButton_ = disconnectSelectedButton;
        captureSyncButton_ = captureSyncButton;
        syncProgressBar_ = syncProgressBar;
        syncStatusLabel_ = syncStatusLabel;

        // Connect camera manager signals
        connect(cameraManager_.get(), &core::CameraManager::syncCaptureProgress,
                this, &MultiCameraController::onSyncCaptureProgress);
        connect(cameraManager_.get(), &core::CameraManager::syncCaptureComplete,
                this, &MultiCameraController::onSyncCaptureComplete);

        // Initially disable buttons
        connectSelectedButton_->setEnabled(false);
        disconnectSelectedButton_->setEnabled(false);
        captureSyncButton_->setEnabled(false);
    }

    void MultiCameraController::setVisible(bool visible)
    {
        if (!toggleSelectButton_ || !clearSelectionButton_ || !connectSelectedButton_ ||
            !disconnectSelectedButton_ || !captureSyncButton_ || !syncProgressBar_ || !syncStatusLabel_)
        {
            return;
        }

        toggleSelectButton_->setVisible(visible);
        clearSelectionButton_->setVisible(visible);
        connectSelectedButton_->setVisible(visible);
        disconnectSelectedButton_->setVisible(visible);
        captureSyncButton_->setVisible(visible);
        syncProgressBar_->setVisible(visible);
        syncStatusLabel_->setVisible(visible);

        if (visible && cameraManager_)
        {
            // Set all cameras to multi-camera mode when entering multi-camera view
            cameraManager_->setAllCamerasToMultiMode();
        }

        if (videoDisplay_ && !visible)
        {
            // When hiding multi-camera mode, don't do anything to video display
            // That will be handled by the single camera controller
        }
        else if (videoDisplay_ && visible)
        {
            // When showing multi-camera mode, disable video display and show placeholder
            videoDisplay_->setEnabled(false);
            QImage placeholderImage(640, 480, QImage::Format_RGB32);
            placeholderImage.fill(Qt::black);
            QPainter painter(&placeholderImage);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 14));
            painter.drawText(placeholderImage.rect(), Qt::AlignCenter,
                             "Video preview disabled in multi-camera mode.\nUse single camera mode for live preview.");
            videoDisplay_->updateFrame(placeholderImage);
        }

        updateUI();
    }

    void MultiCameraController::refreshCameraList()
    {
        updateUI();
    }

    void MultiCameraController::updateUI()
    {
        if (!cameraList_ || !connectSelectedButton_ || !disconnectSelectedButton_ || !captureSyncButton_ || !toggleSelectButton_)
        {
            return;
        }

        int selectedCount = 0;
        int connectedSelectedCount = 0;

        for (int i = 0; i < cameraList_->count(); ++i)
        {
            if (cameraList_->item(i)->checkState() == Qt::Checked)
            {
                selectedCount++;
                if (cameraManager_->isCameraConnected(i))
                {
                    connectedSelectedCount++;
                }
            }
        }

        connectSelectedButton_->setEnabled(selectedCount > 0);
        disconnectSelectedButton_->setEnabled(connectedSelectedCount > 0);
        captureSyncButton_->setEnabled(connectedSelectedCount > 1);

        toggleSelectButton_->setText(areAllCamerasSelected() ? tr("Deselect All") : tr("Select All"));
    }

    bool MultiCameraController::areAllCamerasSelected() const
    {
        if (!cameraList_)
        {
            return false;
        }

        for (int i = 0; i < cameraList_->count(); ++i)
        {
            if (cameraList_->item(i)->checkState() != Qt::Checked)
            {
                return false;
            }
        }

        return cameraList_->count() > 0;
    }

    void MultiCameraController::onToggleSelectAll()
    {
        bool allSelected = areAllCamerasSelected();

        // First clear all selections in the manager if we're toggling all to selected
        if (!allSelected)
        {
            cameraManager_->clearCameraSelection();
        }

        for (int i = 0; i < cameraList_->count(); ++i)
        {
            bool newState = !allSelected;
            cameraList_->item(i)->setCheckState(newState ? Qt::Checked : Qt::Unchecked);
            // Update selection in camera manager
            cameraManager_->selectCameraForSync(i, newState);
        }

        updateUI();
        emit logMessage(allSelected ? "Deselected all cameras" : "Selected all cameras");
    }

    void MultiCameraController::onClearSelection()
    {
        for (int i = 0; i < cameraList_->count(); ++i)
        {
            cameraList_->item(i)->setCheckState(Qt::Unchecked);
            // Make sure to deselect in the camera manager as well
            cameraManager_->selectCameraForSync(i, false);
        }

        // Clear all selections in the manager
        cameraManager_->clearCameraSelection();

        updateUI();
        emit logMessage("Cleared camera selection");
    }

    void MultiCameraController::onConnectSelectedCameras()
    {
        std::vector<int> selectedIndices;
        for (int i = 0; i < cameraList_->count(); ++i)
        {
            if (cameraList_->item(i)->checkState() == Qt::Checked)
            {
                selectedIndices.push_back(i);
                // Ensure the camera is selected in the manager
                cameraManager_->selectCameraForSync(i, true);
            }
        }

        if (!selectedIndices.empty())
        {
            emit logMessage(QString("Connecting %1 selected cameras...").arg(selectedIndices.size()));
            // Use the camera manager's functionality to connect selected cameras
            if (cameraManager_->connectSelectedCameras())
            {
                emit logMessage("All selected cameras connected successfully", "SUCCESS");
            }
            else
            {
                emit logMessage("Some cameras failed to connect", "WARNING");
            }
            updateUI();
        }
    }

    void MultiCameraController::onDisconnectSelectedCameras()
    {
        std::vector<int> selectedIndices;
        for (int i = 0; i < cameraList_->count(); ++i)
        {
            if (cameraList_->item(i)->checkState() == Qt::Checked)
            {
                selectedIndices.push_back(i);
                // Ensure the camera is selected in the manager
                cameraManager_->selectCameraForSync(i, true);
            }
        }

        if (!selectedIndices.empty())
        {
            emit logMessage(QString("Disconnecting %1 selected cameras...").arg(selectedIndices.size()));

            // Use the camera manager's functionality to disconnect selected cameras
            bool success = cameraManager_->disconnectSelectedCameras();

            if (success)
            {
                emit logMessage("All selected cameras disconnected successfully", "SUCCESS");
            }
            else
            {
                emit logMessage("Some cameras failed to disconnect", "WARNING");
            }

            updateUI();
        }
    }

    void MultiCameraController::onCaptureSync()
    {
        QString timeStamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        // Use app's home directory for simplicity
        QString basePath = QDir::homePath() + "/Pictures/CamMatrixCaptures";
        QString dirPath = basePath + "/sync_" + timeStamp;

        QDir dir;
        if (!dir.exists(dirPath))
        {
            dir.mkpath(dirPath);
        }

        emit logMessage(QString("Starting synchronized capture to %1").arg(dirPath), "ACTION");

        if (syncProgressBar_)
        {
            syncProgressBar_->setValue(0);
            syncProgressBar_->show();
        }

        // Call the capturePhotosSync method
        bool success = cameraManager_->capturePhotosSync(dirPath);

        if (!success)
        {
            emit logMessage("Failed to start synchronized capture", "ERROR");
            if (syncProgressBar_)
            {
                syncProgressBar_->hide();
            }
        }
    }

    void MultiCameraController::onCameraSelectionChanged(QListWidgetItem *item)
    {
        // Find the index of the changed item
        int itemIndex = cameraList_->row(item);
        if (itemIndex >= 0)
        {
            bool selected = item->checkState() == Qt::Checked;
            // Update the camera manager's selection state
            cameraManager_->selectCameraForSync(itemIndex, selected);
            emit logMessage(QString("Camera %1 %2 for synchronized capture")
                                .arg(itemIndex)
                                .arg(selected ? "selected" : "deselected"));
        }

        updateUI();
    }

    void MultiCameraController::onSyncCaptureProgress(int current, int total)
    {
        if (syncProgressBar_)
        {
            syncProgressBar_->setValue(current);
        }

        if (syncStatusLabel_)
        {
            syncStatusLabel_->setText(tr("Capturing %1 of %2...").arg(current).arg(total));
        }

        emit logMessage(QString("Sync capture progress: %1/%2").arg(current).arg(total));
    }

    void MultiCameraController::onSyncCaptureComplete(int successCount, int total)
    {
        if (syncProgressBar_)
        {
            syncProgressBar_->setValue(total);
        }

        if (syncStatusLabel_)
        {
            syncStatusLabel_->setText(tr("Captured %1 of %2 successfully").arg(successCount).arg(total));
        }

        if (successCount == total)
        {
            emit logMessage(QString("Synchronized capture completed successfully: %1 images").arg(total), "SUCCESS");
        }
        else
        {
            emit logMessage(QString("Synchronized capture completed with issues: %1/%2 successful")
                                .arg(successCount)
                                .arg(total),
                            "WARNING");
        }
    }

} // namespace cam_matrix::ui