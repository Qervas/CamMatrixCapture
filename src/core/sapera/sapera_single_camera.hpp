#pragma once
#include "sapera_camera_base.hpp"
#include <mutex>
#include <future>

namespace cam_matrix::core::sapera
{

    // SingleCamera class - optimized for live view with continuous frame acquisition
    class SaperaSingleCamera : public SaperaCameraBase
    {
        Q_OBJECT
    public:
        SaperaSingleCamera(int serverIndex, const QString &serverName, const QString &configName, QObject *parent = nullptr);
        ~SaperaSingleCamera() override;

        // Override base class methods
        bool connectCamera() override;
        bool capturePhoto(const QString &filePath) override;

        // Async version of photo capture - better for multi-camera scenarios
        void capturePhotoAsync(const QString &filePath, const std::function<void(bool, const QString &)> &callback = nullptr);

        // High-quality photo capture with dedicated resources
        bool captureHighQualityPhoto(const QString &filePath, const QString &format = "tiff");

        // Additional methods specific to live view
        bool startAcquisition();
        void stopAcquisition();
        bool isAcquiring() const { return m_isAcquiring; }

    private:
        static void XferCallback(SapXferCallbackInfo *pInfo);

        bool m_isAcquiring = false;
        std::mutex m_frameMutex;
    };

} // namespace cam_matrix::core::sapera