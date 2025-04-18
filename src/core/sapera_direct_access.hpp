#pragma once

#if defined(SAPERA_FOUND) && SAPERA_FOUND
// We'll use our stubs from sapera_defs.hpp instead of including SapClassBasic.h
#include "core/sapera_defs.hpp"
#include <QString>
#include <QImage>
#include <QObject>
#include <functional>
#include <string>
#include <vector>

namespace cam_matrix::core {

// Forward declarations
class SaperaDirectAccess;

// Callback function for the transfer - proper function declaration
void SaperaXferCallback(SapXferCallbackInfo* pInfo);

class SaperaDirectAccess : public QObject {
    Q_OBJECT

public:
    explicit SaperaDirectAccess(QObject* parent = nullptr);
    ~SaperaDirectAccess();

    // Initialize and connect to the camera
    bool initialize(const std::string& serverName);
    
    // Start/stop acquisition
    bool startAcquisition();
    bool stopAcquisition();
    
    // Check if acquisition is running
    bool isAcquiring() const;
    
    // Camera settings
    bool setExposureTime(double microseconds);
    double getExposureTime() const;
    
    // Get camera information
    std::string getCameraModelName() const;
    std::string getCameraSerialNumber() const;
    std::string getCameraFirmwareVersion() const;
    
    // Get available pixel formats
    std::vector<std::string> getAvailablePixelFormats() const;
    bool setPixelFormat(const std::string& format);
    
    // Get the current frame as QImage
    QImage getCurrentFrame() const;
    
    // Static function to get available cameras
    static std::vector<std::string> getAvailableCameras();

signals:
    void newFrameAvailable(const QImage& frame);
    void statusChanged(const QString& status);
    void error(const QString& error);

private:
    // Configure camera parameters
    bool configureCamera();
    
    // Create Sapera objects
    bool createSaperaObjects();
    
    // Destroy Sapera objects
    void destroySaperaObjects();
    
    // Print feature value
    void printFeatureValue(const char* featureName);
    
    // Make the callback function a friend so it can access our private members
    friend void SaperaXferCallback(SapXferCallbackInfo* pInfo);
    
    // Process a new frame
    void processNewFrame(SapBuffer* pBuffer);

    // Sapera objects
    SapAcqDevice* acqDevice_{nullptr};
    SapBuffer* buffer_{nullptr};
    SapTransfer* transfer_{nullptr};
    SapView* view_{nullptr};
    
    // Current frame
    QImage currentFrame_;
    
    // Camera parameters
    std::string serverName_;
    double exposureTime_{10000.0};
    bool isAcquiring_{false};
    
    // Last error message
    QString lastError_;
};

} // namespace cam_matrix::core

#endif 