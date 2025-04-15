#pragma once
#include "core/camera.hpp"
#include "core/sapera_defs.hpp"  // Use our conditional include

#include <string>
#include <memory>
#include <QImage>
#include <QTimer>
#include <QObject>
#include <mutex>

namespace cam_matrix::core {

#if HAS_SAPERA
// Only compile the real implementation if Sapera is available
class SaperaCamera : public Camera {
    Q_OBJECT

public:
    explicit SaperaCamera(const std::string& name);
    ~SaperaCamera() override;

    // Camera interface implementation
    bool connectCamera() override;
    bool disconnectCamera() override;
    bool isConnected() const override;
    std::string getName() const override;
    bool setExposureTime(double microseconds);

    // Camera properties
    double getExposureTime() const;

    // Get the current frame
    QImage getFrame() const;

    // Camera configuration
    bool configureCamera();
    void printCameraInfo() const;

signals:
    void newFrameAvailable(const QImage& frame);
    void statusChanged(const std::string& message);
    void error(const std::string& message);

private:
    std::string name_;
    bool isConnected_;

    // Sapera objects
    std::unique_ptr<SapAcqDevice> device_;
    std::unique_ptr<SapBufferWithTrash> buffer_;
    std::unique_ptr<SapAcqDeviceToBuf> transfer_;
    std::unique_ptr<SapView> view_;

    // Current frame
    mutable std::mutex frameMutex_;
    QImage currentFrame_;

    // Helper functions
    bool createSaperaObjects();
    void destroySaperaObjects();
    void updateFrameFromBuffer();

    // Static callback functions for the Sapera SDK
    static void XferCallback(SapXferCallbackInfo *pInfo);

    // Feature helper functions
    void printFeatureValue(const char* featureName) const;
    bool isFeatureAvailable(const char* featureName) const;
};
#else
// Provide a stub implementation when Sapera is not available
class SaperaCamera : public Camera {
    Q_OBJECT

public:
    SaperaCamera(const std::string& serverName) : Camera(), serverName_(serverName), isConnected_(false) {}
    ~SaperaCamera() override = default;

    std::string getName() const override { return serverName_ + " (Sapera SDK not available)"; }
    bool isConnected() const override { return false; }
    bool connectCamera() override { return false; }
    bool disconnectCamera() override { return false; }
    bool setExposureTime(double microseconds) { 
        emit statusChanged("Exposure time set to " + std::to_string(microseconds) + " microseconds (stub)");
        return true; 
    }

    QImage getFrame() const { return QImage(); }

signals:
    void newFrameAvailable(QImage frame);
    void statusChanged(const std::string& message);
    void error(const std::string& errorMessage);

private:
    std::string serverName_;
    bool isConnected_;
};
#endif

} // namespace cam_matrix::core
