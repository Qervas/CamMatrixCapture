#pragma once

#if defined(SAPERA_FOUND)
#include <SapClassBasic.h>
#include <QString>
#include <QImage>
#include <QObject>
#include <functional>
#include <string>
#include <vector>

namespace cam_matrix::core {

// Callback function for the transfer - forward declaration
class SaperaDirectAccess;
void XferCallback(SapXferCallbackInfo* pInfo);

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
    
    // Friend function for callback
    friend void XferCallback(SapXferCallbackInfo* pInfo);
    
    // Process a new frame
    void processNewFrame(void* pBuffer);

    // Sapera objects
    SapAcqDevice* acqDevice_;
    SapBuffer* buffer_;
    SapTransfer* transfer_;
    
    // Current frame
    QImage currentFrame_;
    
    // Camera parameters
    std::string serverName_;
    double exposureTime_;
    bool isAcquiring_;
    
    // Last error message
    QString lastError_;
};

} // namespace cam_matrix::core

#endif 