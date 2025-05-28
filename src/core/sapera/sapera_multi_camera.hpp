#pragma once
#include "sapera_camera_base.hpp"
#include <mutex>
#include <future>

namespace cam_matrix::core::sapera
{

    // MultiCamera class - optimized for synchronized capture without continuous streaming
    class SaperaMultiCamera : public SaperaCameraBase
    {
        Q_OBJECT
    public:
        SaperaMultiCamera(int serverIndex, const QString &serverName, const QString &configName, QObject *parent = nullptr);
        ~SaperaMultiCamera() override;

        // Override base class methods
        bool connectCamera() override;
        bool capturePhoto(const QString &filePath) override;

        // Async version for multi-camera synchronized capture
        void capturePhotoAsync(const QString &filePath, const std::function<void(bool, const QString &)> &callback = nullptr);

        // High-quality photo capture with dedicated resources
        bool captureHighQualityPhoto(const QString &filePath, const QString &format = "tiff");

    private:
        static void XferCallback(SapXferCallbackInfo *pInfo);

        std::mutex m_frameMutex;
    };

} // namespace cam_matrix::core::sapera