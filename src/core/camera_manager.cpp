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

        // First try standard Sapera enumeration
        auto cameraFullNames = SaperaCameraReal::enumerateCameras();
        qDebug() << "Found" << cameraFullNames.size() << "cameras through Sapera enumeration";

        // Add cameras with exact names from sample code
        for (const auto &name : cameraFullNames)
        {
            filteredCameras.append(name);
            qDebug() << "Added camera:" << name;
        }

        // Create camera objects
        for (int i = 0; i < filteredCameras.size(); ++i)
        {
            cameras_.push_back(std::make_unique<SaperaCameraReal>(i, filteredCameras[i], ccfPath));
        }

        emit statusChanged(QString("Found %1 cameras").arg(cameras_.size()));
        emit cameraStatusChanged("Camera list updated");
    }

    std::vector<SaperaCameraReal *> CameraManager::getCameras() const
    {
        std::vector<SaperaCameraReal *> result;
        result.reserve(cameras_.size());
        for (const auto &camera : cameras_)
        {
            result.push_back(camera.get());
        }
        return result;
    }

    SaperaCameraReal *CameraManager::getCameraByIndex(size_t index) const
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
                connect(camera, &SaperaCameraReal::frameReady, this, &CameraManager::newFrameAvailable, Qt::QueuedConnection);
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
                disconnect(camera, &SaperaCameraReal::frameReady, this, &CameraManager::newFrameAvailable);

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
        for (size_t index : indexesToConnect)
        {
            if (index < cameras_.size())
            {
                if (connectCamera(index))
                    connectedCount++;
                else
                    allConnected = false;
            }
        }
        emit statusChanged(QString("Connected %1 of %2 cameras").arg(connectedCount).arg(indexesToConnect.size()));
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
            emit error("No cameras selected for synchronized disconnection");
            return false;
        }
        emit statusChanged(QString("Disconnecting %1 cameras...").arg(indexesToDisconnect.size()));
        int disconnectedCount = 0;
        for (size_t index : indexesToDisconnect)
        {
            if (index < cameras_.size())
            {
                if (disconnectCamera(index))
                    disconnectedCount++;
                else
                    allDisconnected = false;
            }
        }
        emit statusChanged(QString("Disconnected %1 of %2 cameras").arg(disconnectedCount).arg(indexesToDisconnect.size()));
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
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        QString folderName = "captures/nerf_" + timestamp;
        QDir dir(folderName);
        if (!dir.exists())
        {
            if (!dir.mkpath("."))
            {
                qWarning() << "Failed to create directory:" << folderName;
                return "";
            }
        }
        return folderName;
    }

    bool CameraManager::capturePhotosSync(const QString &basePath)
    {
        std::vector<size_t> indexesToCapture;
        {
            std::lock_guard<std::mutex> lock(selectionMutex_);
            indexesToCapture.assign(selectedCameras_.begin(), selectedCameras_.end());
        }
        if (indexesToCapture.empty())
        {
            emit error("No cameras selected for synchronized capture");
            return false;
        }
        for (size_t index : indexesToCapture)
        {
            if (index >= cameras_.size() || !isCameraConnected(index))
            {
                emit error(QString("Camera %1 is not connected. All cameras must be connected for synchronized capture.").arg(index));
                return false;
            }
        }

        QString captureFolder = basePath;
        if (captureFolder.isEmpty())
        {
            captureFolder = generateCaptureFolder();
            if (captureFolder.isEmpty())
            {
                emit error("Failed to create capture folder");
                return false;
            }
        }

        emit syncCaptureStarted(static_cast<int>(indexesToCapture.size()));
        emit statusChanged(QString("Starting synchronized capture with %1 cameras...").arg(indexesToCapture.size()));

        // Use a small delay to ensure all cameras have settled
        std::this_thread::sleep_for(std::chrono::milliseconds(800)); // Increased for better stability

        // Store results for reporting
        std::vector<QString> capturePaths;
        capturePaths.reserve(indexesToCapture.size());

        // Generate timestamps with a consistent base timestamp
        QDateTime baseTime = QDateTime::currentDateTime();
        QString timeStamp = baseTime.toString("yyyy-MM-dd_HH-mm-ss");

        // Format pattern for filenames - use PNG instead of TIFF to reduce memory usage
        for (size_t i = 0; i < indexesToCapture.size(); ++i)
        {
            size_t cameraIndex = indexesToCapture[i];
            QString filename = captureFolder + QString("/camera_%1_%2_%3.png").arg(cameraIndex).arg(i).arg(timeStamp);
            capturePaths.push_back(filename);
        }

        // Process ONE camera at a time to avoid buffer overflow
        int successCount = 0;
        int totalCameras = static_cast<int>(indexesToCapture.size());

        for (size_t i = 0; i < indexesToCapture.size(); ++i)
        {
            size_t cameraIndex = indexesToCapture[i];
            QString filename = capturePaths[i];

            emit statusChanged(QString("Capturing camera %1 of %2...").arg(i + 1).arg(totalCameras));

            // Capture one photo synchronously
            auto *camera = getCameraByIndex(cameraIndex);
            bool success = false;

            if (camera)
            {
                try
                {
                    // Add small delay before each camera capture
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    success = camera->capturePhoto(filename);
                    // Add delay after capture to let the system recover
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                catch (const std::exception &e)
                {
                    qCritical() << "Exception during sync capture:" << e.what();
                }
                catch (...)
                {
                    qCritical() << "Unknown exception during sync capture";
                }
            }

            if (success)
            {
                successCount++;
                emit photoCaptured(cameraIndex, filename);
            }
            else
            {
                emit error(QString("Failed to capture photo from camera %1").arg(cameraIndex));
            }

            emit syncCaptureProgress(static_cast<int>(i + 1), totalCameras);
        }

        emit syncCaptureComplete(successCount, totalCameras);
        emit statusChanged(QString("Synchronized capture complete: %1 of %2 successful").arg(successCount).arg(totalCameras));
        return successCount == totalCameras;
    }

    std::vector<std::string> CameraManager::getAvailableCameras() const
    {
        std::vector<std::string> cameraNames;
        cameraNames.reserve(cameras_.size());
        for (const auto &camera : cameras_)
        {
            if (camera)
            {
                cameraNames.push_back(camera->getServerName().toStdString());
            }
        }
        return cameraNames;
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
        // Not implemented in SaperaCameraReal, add if needed
        Q_UNUSED(index)
        Q_UNUSED(value)
        return false;
    }

    bool CameraManager::setGain(size_t index, double value)
    {
        // Not implemented in SaperaCameraReal, add if needed
        Q_UNUSED(index)
        Q_UNUSED(value)
        return false;
    }

    double CameraManager::getExposureTime(size_t index) const
    {
        // Not implemented in SaperaCameraReal, add if needed
        Q_UNUSED(index)
        return 0.0;
    }

    double CameraManager::getGain(size_t index) const
    {
        // Not implemented in SaperaCameraReal, add if needed
        Q_UNUSED(index)
        return 0.0;
    }

    bool CameraManager::capturePhoto(size_t index, const QString &path)
    {
        if (index < cameras_.size())
        {
            auto *camera = cameras_[index].get();

            // Check if camera is valid and connected before attempting capture
            if (!camera)
            {
                emit error(QString("Camera %1 not initialized").arg(index));
                return false;
            }

            if (!camera->isConnected())
            {
                emit error(QString("Camera %1 not connected").arg(index));
                return false;
            }

            emit statusChanged(QString("Capturing photo with camera %1...").arg(index));

            // Attempt to capture the photo
            bool success = camera->capturePhoto(path);

            if (success)
            {
                emit statusChanged(QString("Photo capture successful for camera %1").arg(index));
            }
            else
            {
                emit error(QString("Failed to capture photo with camera %1").arg(index));
            }

            return success;
        }

        emit error(QString("Invalid camera index: %1").arg(index));
        return false;
    }

    void CameraManager::updateCamerasFromDirectAccess(const std::vector<std::string> &cameraNames)
    {
        Q_UNUSED(cameraNames)
        // No-op: direct access not used in real Sapera integration
    }

} // namespace cam_matrix::core
