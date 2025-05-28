#include "core/camera_manager.hpp"
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <thread>
#include <future>
#include <chrono>
#include <QCoreApplication>
#include <QThread>
#include <QRandomGenerator>

namespace cam_matrix::core
{

    CameraManager::CameraManager(QObject *parent)
        : QObject(parent)
    {
        // Print diagnostic information about working directory and config file
        QDir currentDir = QDir::current();
        qDebug() << "Current working directory:" << currentDir.absolutePath();
        qDebug() << "Application directory:" << QCoreApplication::applicationDirPath();

        // Check if config file exists
        QString configFile = QCoreApplication::applicationDirPath() + "/NanoC4020.ccf";
        QFileInfo checkFile(configFile);
        if (checkFile.exists())
        {
            qDebug() << "Found camera config file at:" << configFile;
        }
        else
        {
            qWarning() << "Could not find camera config file at:" << configFile;
            qWarning() << "This file is required for camera operation.";
        }

        // Initialize Sapera if possible
        try
        {
            // Disable error dialogs
            SapManager::SetDisplayStatusMode(FALSE);

            qDebug() << "Sapera initialized with display dialogs disabled";
        }
        catch (const std::exception &e)
        {
            qWarning() << "Exception during Sapera initialization:" << e.what();
        }
        catch (...)
        {
            qWarning() << "Unknown exception during Sapera initialization";
        }

        QMetaObject::invokeMethod(this, "scanForCameras", Qt::QueuedConnection);
    }

    CameraManager::~CameraManager() = default;

    void CameraManager::scanForCameras()
    {
        cameras_.clear();
        emit managerStatusChanged("Scanning for cameras...");

        // Use simple enumeration that matches the sample code naming pattern
        QString ccfPath = QCoreApplication::applicationDirPath() + "/NanoC4020.ccf";
        qDebug() << "Using camera config file at:" << ccfPath;

        QStringList filteredCameras;

        // Use the SaperaCameraFactory to enumerate cameras
        auto cameraFullNames = SaperaCameraFactory::enumerateCameras();
        qDebug() << "Found" << cameraFullNames.size() << "cameras through Sapera enumeration";

        // Add cameras with exact names from sample code, excluding "System"
        for (const auto &name : cameraFullNames)
        {
            // Skip if the camera name contains "System"
            if (!name.contains("System", Qt::CaseInsensitive))
            {
                filteredCameras.append(name);
                qDebug() << "Added camera:" << name;
            }
            else
            {
                qDebug() << "Skipped system camera:" << name;
            }
        }

        // Create camera objects - default to SingleCamera mode initially
        for (int i = 0; i < filteredCameras.size(); ++i)
        {
            cameras_.push_back(SaperaCameraFactory::createCamera(CameraMode::SingleCamera, i, filteredCameras[i], ccfPath));
        }

        emit statusChanged(QString("Found %1 cameras").arg(cameras_.size()));
        emit cameraStatusChanged("Camera list updated");
    }

    std::vector<SaperaCameraBase *> CameraManager::getCameras() const
    {
        std::vector<SaperaCameraBase *> result;
        result.reserve(cameras_.size());
        for (const auto &camera : cameras_)
        {
            result.push_back(camera.get());
        }
        return result;
    }

    SaperaCameraBase *CameraManager::getCameraByIndex(size_t index) const
    {
        if (index < cameras_.size())
        {
            return cameras_[index].get();
        }
        return nullptr;
    }

    bool CameraManager::connectCamera(size_t index)
    {
        if (index < cameras_.size())
        {
            auto *camera = cameras_[index].get();
            if (camera->connectCamera())
            {
                connect(camera, &SaperaCameraBase::frameReady, this, &CameraManager::newFrameAvailable, Qt::QueuedConnection);
                emit cameraConnected(index);
                emit cameraStatusChanged(QString("Camera %1 connected").arg(index));
                return true;
            }
            else
            {
                emit cameraStatusChanged(QString("Failed to connect camera %1").arg(index));
            }
        }
        return false;
    }

    bool CameraManager::disconnectCamera(size_t index)
    {
        if (index < cameras_.size())
        {
            auto *camera = cameras_[index].get();

            // Use try-catch to handle any exceptions during disconnect
            try
            {
                // First emit a signal that we're about to disconnect
                emit cameraStatusChanged(QString("Disconnecting camera %1...").arg(index));

                // Safely disconnect signals first, using correct invokeMethod syntax for Qt6.9
                disconnect(camera, &SaperaCameraBase::frameReady, this, &CameraManager::newFrameAvailable);

                // Use a direct disconnect instead of invokeMethod to avoid compatibility issues
                // This is safe because we're already in a try-catch block

                // Give a small delay to ensure signal disconnections propagate
                QThread::msleep(50);

                // Now disconnect the camera
                camera->disconnectCamera();

                // Emit signals after the camera is disconnected
                emit cameraDisconnected(index);
                emit cameraStatusChanged(QString("Camera %1 disconnected").arg(index));

                return true;
            }
            catch (const std::exception &e)
            {
                qWarning() << "Exception during camera disconnect:" << e.what();
                emit error(QString("Error disconnecting camera: %1").arg(e.what()));
                return false;
            }
            catch (...)
            {
                qWarning() << "Unknown exception during camera disconnect";
                emit error("Unknown error disconnecting camera");
                return false;
            }
        }
        return false;
    }

    bool CameraManager::disconnectAllCameras()
    {
        bool allDisconnected = true;
        for (size_t i = 0; i < cameras_.size(); ++i)
        {
            if (!disconnectCamera(i))
            {
                allDisconnected = false;
            }
        }
        return allDisconnected;
    }

    void CameraManager::selectCameraForSync(size_t index, bool selected)
    {
        if (index >= cameras_.size())
            return;
        {
            std::lock_guard<std::mutex> lock(selectionMutex_);
            if (selected)
                selectedCameras_.insert(index);
            else
                selectedCameras_.erase(index);
        }
        emit statusChanged(QString("Camera %1 %2 for sync (%3 cameras selected)")
                               .arg(index)
                               .arg(selected ? "selected" : "deselected")
                               .arg(selectedCameras_.size()));
    }

    void CameraManager::clearCameraSelection()
    {
        {
            std::lock_guard<std::mutex> lock(selectionMutex_);
            selectedCameras_.clear();
        }
        emit statusChanged("Camera selection cleared");
    }

    bool CameraManager::connectSelectedCameras()
    {
        bool allConnected = true;
        std::vector<size_t> indexesToConnect;
        {
            std::lock_guard<std::mutex> lock(selectionMutex_);
            indexesToConnect.assign(selectedCameras_.begin(), selectedCameras_.end());
        }

        if (indexesToConnect.empty())
        {
            emit error("No cameras selected for synchronized connection");
            return false;
        }

        emit statusChanged(QString("Connecting %1 cameras...").arg(indexesToConnect.size()));
        int connectedCount = 0;

        // Add delay between each camera connection to avoid resource contention
        for (size_t index : indexesToConnect)
        {
            if (index < cameras_.size())
            {
                // Add delay before connecting each camera
                QThread::msleep(500); // 500ms delay between connections

                if (connectCamera(index))
                {
                    connectedCount++;
                    // Add delay after successful connection
                    QThread::msleep(200); // 200ms delay after successful connection
                }
                else
                {
                    allConnected = false;
                    emit error(QString("Failed to connect camera %1").arg(index));
                }
            }
        }

        emit statusChanged(QString("Connected %1 of %2 selected cameras")
                               .arg(connectedCount)
                               .arg(indexesToConnect.size()));
        return allConnected;
    }

    bool CameraManager::disconnectSelectedCameras()
    {
        bool allDisconnected = true;
        std::vector<size_t> indexesToDisconnect;
        {
            std::lock_guard<std::mutex> lock(selectionMutex_);
            indexesToDisconnect.assign(selectedCameras_.begin(), selectedCameras_.end());
        }

        if (indexesToDisconnect.empty())
        {
            emit error("No cameras selected for disconnection");
            return false;
        }

        emit statusChanged(QString("Disconnecting %1 cameras...").arg(indexesToDisconnect.size()));
        int disconnectedCount = 0;

        for (size_t index : indexesToDisconnect)
        {
            if (index < cameras_.size())
            {
                if (disconnectCamera(index))
                {
                    disconnectedCount++;
                    // Add delay after successful disconnection
                    QThread::msleep(200); // 200ms delay after successful disconnection
                }
                else
                {
                    allDisconnected = false;
                    emit error(QString("Failed to disconnect camera %1").arg(index));
                }
            }
        }

        emit statusChanged(QString("Disconnected %1 of %2 selected cameras")
                               .arg(disconnectedCount)
                               .arg(indexesToDisconnect.size()));
        return allDisconnected;
    }

    std::set<size_t> CameraManager::getSelectedCameras() const
    {
        std::lock_guard<std::mutex> lock(selectionMutex_);
        return selectedCameras_;
    }

    bool CameraManager::isCameraSelected(size_t index) const
    {
        std::lock_guard<std::mutex> lock(selectionMutex_);
        return selectedCameras_.find(index) != selectedCameras_.end();
    }

    QString CameraManager::generateCaptureFolder() const
    {
        // Create a timestamped folder for this capture session
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        QString folderName = QDir::homePath() + "/Pictures/CamMatrixCapture_" + timestamp;

        // Create the directory
        QDir dir(folderName);
        if (!dir.exists())
        {
            dir.mkpath(".");
        }

        return folderName;
    }

    bool CameraManager::capturePhotosSync(const QString &basePath)
    {
        QString captureFolder = basePath;
        if (captureFolder.isEmpty())
        {
            captureFolder = generateCaptureFolder();
        }

        // Get selected camera indices
        std::vector<size_t> selectedIndices;
        {
            std::lock_guard<std::mutex> lock(selectionMutex_);
            selectedIndices.assign(selectedCameras_.begin(), selectedCameras_.end());
        }

        if (selectedIndices.empty())
        {
            emit error("No cameras selected for synchronized capture");
            return false;
        }

        int totalCameras = selectedIndices.size();
        emit syncCaptureStarted(totalCameras);
        emit statusChanged(QString("Starting synchronized capture with %1 cameras...").arg(totalCameras));

        // Vector to store async results
        std::vector<std::future<bool>> results;
        std::vector<QString> filenames(totalCameras);

        // Launch capture operations for all selected cameras
        for (int i = 0; i < selectedIndices.size(); i++)
        {
            size_t cameraIndex = selectedIndices[i];

            if (cameraIndex >= cameras_.size() || !cameras_[cameraIndex] || !cameras_[cameraIndex]->isConnected())
            {
                emit syncCaptureProgress(i, totalCameras);
                continue;
            }

            // Generate a unique filename for this camera
            QString cameraName = cameras_[cameraIndex]->getServerName().replace(":", "_").replace(" ", "_");
            QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
            QString filename = QString("%1/%2_%3_%4.png")
                                   .arg(captureFolder)
                                   .arg(cameraName)
                                   .arg(timestamp)
                                   .arg(i);
            filenames[i] = filename;

            // Check if the implementation supports async capture
            auto *multiCamera = dynamic_cast<SaperaMultiCamera *>(cameras_[cameraIndex].get());

            if (multiCamera)
            {
                // Use the async capture API for multi-camera
                multiCamera->capturePhotoAsync(filename, [this, cameraIndex, filename](bool success, const QString &path)
                                               {
                    if (success) {
                        emit photoCaptured(cameraIndex, filename);
                    } });
            }
            else
            {
                // For regular cameras, capture synchronously
                results.push_back(std::async(std::launch::async, [this, cameraIndex, filename]()
                                             {
                    bool success = this->capturePhoto(cameraIndex, filename);
                    return success; }));
            }

            emit syncCaptureProgress(i + 1, totalCameras);

            // Add a small delay between capture starts to distribute load
            QThread::msleep(50);
        }

        // Wait for all operations to complete and count successes
        int successCount = 0;
        for (auto &result : results)
        {
            if (result.get())
            {
                successCount++;
            }
        }

        emit syncCaptureComplete(successCount, totalCameras);
        emit statusChanged(QString("Synchronized capture completed: %1/%2 successful")
                               .arg(successCount)
                               .arg(totalCameras));

        return (successCount == totalCameras);
    }

    bool CameraManager::captureHighQualityPhoto(size_t index, const QString &path, const QString &format)
    {
        if (index < cameras_.size() && cameras_[index] && cameras_[index]->isConnected())
        {
            bool success = cameras_[index]->captureHighQualityPhoto(path, format);
            if (success)
            {
                emit photoCaptured(index, path);
            }
            return success;
        }
        return false;
    }

    bool CameraManager::captureHighQualityPhotosSync(const QString &basePath, const QString &format)
    {
        QString captureFolder = basePath;
        if (captureFolder.isEmpty())
        {
            captureFolder = generateCaptureFolder();
        }

        // Get selected camera indices
        std::vector<size_t> selectedIndices;
        {
            std::lock_guard<std::mutex> lock(selectionMutex_);
            selectedIndices.assign(selectedCameras_.begin(), selectedCameras_.end());
        }

        if (selectedIndices.empty())
        {
            emit error("No cameras selected for synchronized high-quality capture");
            return false;
        }

        int totalCameras = selectedIndices.size();
        emit syncCaptureStarted(totalCameras);
        emit statusChanged(QString("Starting synchronized high-quality capture with %1 cameras...").arg(totalCameras));

        // Vector to store async results
        std::vector<std::future<bool>> results;
        std::vector<QString> filenames(totalCameras);

        // Launch capture operations for all selected cameras - with sequential delay to prevent resource conflicts
        for (int i = 0; i < selectedIndices.size(); i++)
        {
            size_t cameraIndex = selectedIndices[i];

            if (cameraIndex >= cameras_.size() || !cameras_[cameraIndex] || !cameras_[cameraIndex]->isConnected())
            {
                emit syncCaptureProgress(i, totalCameras);
                continue;
            }

            // Generate a unique filename for this camera
            QString cameraName = cameras_[cameraIndex]->getServerName().replace(":", "_").replace(" ", "_");
            QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
            QString filename = QString("%1/%2_HQ_%3_%4.%5")
                                   .arg(captureFolder)
                                   .arg(cameraName)
                                   .arg(timestamp)
                                   .arg(i)
                                   .arg(format.toLower());
            filenames[i] = filename;

            // Add longer delay between capture starts (500ms instead of 100ms)
            // This helps prevent resource conflicts in the Sapera library
            QThread::msleep(500);

            qDebug() << "Starting high-quality capture for camera" << i << "of" << totalCameras;

            // For high-quality capture, we'll use the new captureHighQualityPhoto method
            // We'll run these captures in separate threads with staggered starts
            results.push_back(std::async(std::launch::async, [this, cameraIndex, filename, format]()
                                         {
                // Add random delay inside each thread to further stagger capture operations
                QThread::msleep(QRandomGenerator::global()->bounded(50, 200));
                
                bool success = this->captureHighQualityPhoto(cameraIndex, filename, format);
                return success; }));

            emit syncCaptureProgress(i + 1, totalCameras);
        }

        // Wait for all operations to complete and count successes
        int successCount = 0;
        for (auto &result : results)
        {
            if (result.get())
            {
                successCount++;
            }
        }

        emit syncCaptureComplete(successCount, totalCameras);
        emit statusChanged(QString("Synchronized high-quality capture completed: %1/%2 successful")
                               .arg(successCount)
                               .arg(totalCameras));

        return (successCount == totalCameras);
    }

    std::vector<std::string> CameraManager::getAvailableCameras() const
    {
        std::vector<std::string> names;
        for (const auto &camera : cameras_)
        {
            names.push_back(camera->getServerName().toStdString());
        }
        return names;
    }

    bool CameraManager::isCameraConnected(size_t index) const
    {
        if (index < cameras_.size() && cameras_[index])
        {
            return cameras_[index]->isConnected();
        }
        return false;
    }

    bool CameraManager::setExposureTime(size_t index, double value)
    {
        if (index < cameras_.size() && cameras_[index])
        {
            // Not implemented in the base class yet, can be added later
            return false;
        }
        return false;
    }

    bool CameraManager::setGain(size_t index, double value)
    {
        if (index < cameras_.size() && cameras_[index])
        {
            // Not implemented in the base class yet, can be added later
            return false;
        }
        return false;
    }

    double CameraManager::getExposureTime(size_t index) const
    {
        if (index < cameras_.size() && cameras_[index])
        {
            // Not implemented in the base class yet, can be added later
            return 0.0;
        }
        return 0.0;
    }

    double CameraManager::getGain(size_t index) const
    {
        if (index < cameras_.size() && cameras_[index])
        {
            // Not implemented in the base class yet, can be added later
            return 0.0;
        }
        return 0.0;
    }

    bool CameraManager::capturePhoto(size_t index, const QString &path)
    {
        if (index < cameras_.size() && cameras_[index] && cameras_[index]->isConnected())
        {
            bool success = cameras_[index]->capturePhoto(path);
            if (success)
            {
                emit photoCaptured(index, path);
            }
            return success;
        }
        return false;
    }

    void CameraManager::updateCamerasFromDirectAccess(const std::vector<std::string> &cameraNames)
    {
        Q_UNUSED(cameraNames)
        // No-op: direct access not used in real Sapera integration
    }

    // Implement the new mode setting methods
    void CameraManager::setAllCamerasToSingleMode()
    {
        for (size_t i = 0; i < cameras_.size(); ++i)
        {
            setCameraMode(i, CameraMode::SingleCamera);
        }
        emit statusChanged("All cameras set to single camera mode");
    }

    void CameraManager::setAllCamerasToMultiMode()
    {
        for (size_t i = 0; i < cameras_.size(); ++i)
        {
            setCameraMode(i, CameraMode::MultiCamera);
        }
        emit statusChanged("All cameras set to multi-camera mode");
    }

    void CameraManager::setCameraMode(size_t index, CameraMode mode)
    {
        if (index >= cameras_.size())
            return;

        // First disconnect the camera if it's connected
        bool wasConnected = isCameraConnected(index);
        if (wasConnected)
        {
            disconnectCamera(index);
        }

        // Replace the camera instance with one of the desired mode
        QString serverName = cameras_[index]->getServerName();
        int serverIndex = index;
        QString configName = QCoreApplication::applicationDirPath() + "/NanoC4020.ccf";

        // Create new camera of the desired type
        cameras_[index] = SaperaCameraFactory::createCamera(mode, serverIndex, serverName, configName);

        // Reconnect if it was connected
        if (wasConnected)
        {
            connectCamera(index);
        }

        emit statusChanged(QString("Camera %1 set to %2 mode")
                               .arg(index)
                               .arg(mode == CameraMode::SingleCamera ? "single camera" : "multi-camera"));
    }

} // namespace cam_matrix::core
