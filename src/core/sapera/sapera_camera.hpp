#pragma once
#include "core/camera.hpp"
#include "core/sapera_defs.hpp"  // Use our conditional include
#include "camera_thread.hpp"

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

namespace cam_matrix::core::sapera {

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
    void frameReady(const QImage& frame);
    void finished();

private:
    std::string cameraName_;
    double* exposureTimePtr_{nullptr};
    std::atomic<bool> running_{false};
    QImage generatePattern(int patternId, qint64 timestamp);
};

// New Sapera Camera implementation using thread safety
class SaperaCamera : public core::Camera {
    Q_OBJECT

public:
    explicit SaperaCamera(const std::string& name);
    ~SaperaCamera() override;

    // Camera interface implementation
    bool connectCamera() override;
    bool disconnectCamera() override;
    bool isConnected() const override;
    std::string getName() const override;
    bool setExposureTime(double microseconds) override;
    
    // Photo capture implementation
    bool capturePhoto(const std::string& savePath = "") override;

    // Async operations with callbacks
    void connectCameraAsync(const std::function<void(bool)>& callback = nullptr);
    void disconnectCameraAsync(const std::function<void(bool)>& callback = nullptr);
    void capturePhotoAsync(const std::string& savePath, const std::function<void(bool)>& callback = nullptr);
    void getFrameAsync(const std::function<void(QImage)>& callback);

    // Camera properties
    double getExposureTime() const;
    double getGain() const;
    std::string getPixelFormat() const;
    
    // Camera settings
    bool setGain(double gain);
    bool setPixelFormat(const std::string& format);

    // Get the current frame - non-blocking, may return empty frame
    QImage getFrame() const;

    // Camera configuration
    bool configureCamera();
    
    // Show if we are real Sapera or simulation
    bool isRealSapera() const { 
    #ifdef HAS_SAPERA
        return true; 
    #else
        return false;
    #endif
    }

signals:
    void newFrameAvailable(const QImage& frame);
    void statusChanged(const std::string& message) const;
    void error(const std::string& message) const;
    void photoCaptured(const QImage& image, const std::string& path);
    void operationCompleted(const std::string& operation, bool success);

private slots:
    void handleNewFrame(const QImage& frame);
    void handleThreadError(const QString& errorMessage);

private:
    // Common properties
    std::string name_;
    std::atomic<bool> isConnected_{false};
    std::atomic<double> exposureTime_{10000.0};

    // Thread safety
    mutable std::mutex frameMutex_;
    QImage currentFrame_;
    
    // Operation thread
    std::unique_ptr<CameraThread> cameraThread_;

    // Helper methods
    bool saveImageToFile(const QImage& image, const std::string& filePath);
    void updateFrameFromBuffer();
    void createCameraThread();
    
    // Frame generator thread and worker - available in both compilation modes
    QThread frameGeneratorThread_;
    FrameGeneratorWorker* frameGenerator_{nullptr};
    
    // Thread management methods - available in both compilation modes
    void startFrameThread();
    void stopFrameThread();
    
#ifdef HAS_SAPERA
    // Sapera-specific implementation
    bool createSaperaObjects();
    void destroySaperaObjects();
    void startFrameAcquisition();
    void stopFrameAcquisition();
    void printCameraInfo() const;
    void printFeatureValue(const char* featureName) const;
    bool isFeatureAvailable(const char* featureName) const;
    static void XferCallback(SapXferCallbackInfo* pInfo);

    // Sapera objects
    std::unique_ptr<SapAcqDevice> device_;
    std::unique_ptr<SapBufferWithTrash> buffer_;
    std::unique_ptr<SapAcqDeviceToBuf> transfer_;
    std::unique_ptr<SapView> view_;
#endif
};

} // namespace cam_matrix::core::sapera 