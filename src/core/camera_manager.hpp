#pragma once

#include <QObject>
#include <vector>
#include <set>
#include <memory>
#include <QImage>
#include <mutex>
#include "core/sapera/sapera_camera_factory.hpp"

namespace cam_matrix::core
{
    using namespace cam_matrix::core::sapera;

    class CameraManager : public QObject
    {
        Q_OBJECT

    public:
        explicit CameraManager(QObject *parent = nullptr);
        ~CameraManager();

        // Update cameras from direct access detection
        void updateCamerasFromDirectAccess(const std::vector<std::string> &cameraNames);

        // Get list of available cameras
        std::vector<SaperaCameraBase *> getCameras() const;

        // Get list of available camera names
        std::vector<std::string> getAvailableCameras() const;

        // Camera access
        SaperaCameraBase *getCameraByIndex(size_t index) const;

        // Connection management
        bool connectCamera(size_t index);
        bool disconnectCamera(size_t index);
        bool disconnectAllCameras();
        bool isCameraConnected(size_t index) const;

        // Mode settings for all cameras
        void setAllCamerasToSingleMode();
        void setAllCamerasToMultiMode();
        void setCameraMode(size_t index, CameraMode mode);

        // Camera settings
        bool setExposureTime(size_t index, double value);
        bool setGain(size_t index, double value);
        double getExposureTime(size_t index) const;
        double getGain(size_t index) const;

        // Photo capture
        bool capturePhoto(size_t index, const QString &path);

        // High-quality photo capture
        bool captureHighQualityPhoto(size_t index, const QString &path, const QString &format = "tiff");

        // Multi-camera management
        void selectCameraForSync(size_t index, bool selected);
        void clearCameraSelection();
        bool connectSelectedCameras();
        bool disconnectSelectedCameras();
        std::set<size_t> getSelectedCameras() const;

        // Synchronized capture
        bool capturePhotosSync(const QString &basePath = "");

        // Synchronized high-quality capture
        bool captureHighQualityPhotosSync(const QString &basePath = "", const QString &format = "tiff");

        // Check if camera is selected for sync
        bool isCameraSelected(size_t index) const;

    public slots:
        // Scan for available cameras - moved to slots section for Qt meta-object system
        void scanForCameras();

    signals:
        void statusChanged(const QString &status);
        void error(const QString &error);
        void cameraConnected(size_t index);
        void cameraDisconnected(size_t index);
        void syncCaptureStarted(int count);
        void syncCaptureComplete(int successCount, int totalCount);
        void syncCaptureProgress(int current, int total);
        void photoCaptured(size_t cameraIndex, const QString &path);
        void newFrameAvailable(const QImage &frame);
        void cameraStatusChanged(const QString &status);
        void managerStatusChanged(const QString &status);
        void photoCaptured(const QImage &image, const QString &path);

    private:
        std::vector<std::shared_ptr<SaperaCameraBase>> cameras_;
        std::set<size_t> selectedCameras_;
        mutable std::mutex selectionMutex_;

        // Helper method for sync capture
        QString generateCaptureFolder() const;
    };

} // namespace cam_matrix::core
