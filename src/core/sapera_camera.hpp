#pragma once
#include "core/camera.hpp"
#include "core/sapera_defs.hpp"  // Use our conditional include

#include <string>
#include <memory>
#include <QImage>
#include <QTimer>
#include <QObject>
#include <mutex>
#include <QPainter>

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

private slots:
    void generateMockFrame();

private:
    std::string name_;
    bool isConnected_;
    double exposureTime_ = 10000.0; // Default to 10ms

    // Sapera objects
    std::unique_ptr<SapAcqDevice> device_;
    std::unique_ptr<SapBufferWithTrash> buffer_;
    std::unique_ptr<SapAcqDeviceToBuf> transfer_;
    std::unique_ptr<SapView> view_;

    // Current frame
    mutable std::mutex frameMutex_;
    QImage currentFrame_;

    // For simulation mode
    std::unique_ptr<QTimer> frameTimer_;

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

#elif HAS_GIGE_VISION
// GigE Vision Interface implementation
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

signals:
    void newFrameAvailable(const QImage& frame);
    void statusChanged(const std::string& message);
    void error(const std::string& message);

private slots:
    void generateDummyFrame();

private:
    std::string name_;
    bool isConnected_;
    double exposureTime_ = 10000.0; // Default exposure time

    // Current frame
    mutable std::mutex frameMutex_;
    QImage currentFrame_;

    // For simulation mode
    std::unique_ptr<QTimer> frameTimer_;
};

#else
// Provide a stub implementation when no camera SDK is available
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

signals:
    void newFrameAvailable(const QImage& frame);
    void statusChanged(const std::string& message);
    void error(const std::string& message);

private slots:
    void generateDummyFrame();

private:
    std::string name_;
    bool isConnected_;
    double exposureTime_;

    // Current frame
    mutable std::mutex frameMutex_;
    QImage currentFrame_;

    // For simulation mode
    std::unique_ptr<QTimer> frameTimer_;
};
#endif

} // namespace cam_matrix::core
