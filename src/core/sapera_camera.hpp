#pragma once
#include "core/camera.hpp"
#include "core/sapera_defs.hpp"  // Use our conditional include

#include <string>
#include <memory>
#include <QImage>
#include <QObject>
#include <mutex>
#include <QPainter>
#include <QThread>
#include <atomic>
#include <chrono>
#include <functional>

namespace cam_matrix::core {

// Define a worker class for frame generation
class FrameGeneratorWorker : public QObject {
    Q_OBJECT
public:
    explicit FrameGeneratorWorker(QObject* parent = nullptr);
    ~FrameGeneratorWorker();
    
    void setCamera(const std::string& name, double* exposureTime);
    void stop();

public slots:
    void generateFrames();

signals:
    void frameGenerated(const QImage& frame);
    void finished();

private:
    std::string cameraName_;
    double* exposureTimePtr_{nullptr};
    std::atomic<bool> running_{false};
    QImage generatePattern(int patternId, qint64 timestamp);
};

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
    void statusChanged(const std::string& message) const;
    void error(const std::string& message) const;

private slots:
    void handleNewFrame(const QImage& frame);

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

    // Frame generator thread
    QThread frameGeneratorThread_;
    FrameGeneratorWorker* frameGenerator_{nullptr};

    // Helper functions
    bool createSaperaObjects();
    void destroySaperaObjects();
    void updateFrameFromBuffer();
    void startFrameThread();
    void stopFrameThread();

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
    void statusChanged(const std::string& message) const;
    void error(const std::string& message) const;

private slots:
    void handleNewFrame(const QImage& frame);

private:
    std::string name_;
    bool isConnected_;
    double exposureTime_ = 10000.0; // Default exposure time

    // Current frame
    mutable std::mutex frameMutex_;
    QImage currentFrame_;

    // Frame generator thread
    QThread frameGeneratorThread_;
    FrameGeneratorWorker* frameGenerator_{nullptr};
    
    // Thread management
    void startFrameThread();
    void stopFrameThread();
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
    void statusChanged(const std::string& message) const;
    void error(const std::string& message) const;

private slots:
    void handleNewFrame(const QImage& frame);

private:
    std::string name_;
    bool isConnected_;
    double exposureTime_;

    // Current frame
    mutable std::mutex frameMutex_;
    QImage currentFrame_;

    // Frame generator thread
    QThread frameGeneratorThread_;
    FrameGeneratorWorker* frameGenerator_{nullptr};
    
    // Thread management
    void startFrameThread();
    void stopFrameThread();
};
#endif

} // namespace cam_matrix::core
