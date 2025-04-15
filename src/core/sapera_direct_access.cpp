#include "core/sapera_direct_access.hpp"

#if defined(SAPERA_FOUND) && SAPERA_FOUND
#include <QDebug>
#include <cstring>
#include <QPainter>
#include <QDateTime>
#include <QRandomGenerator>

namespace cam_matrix::core {

// Mock callback function for the transfer
void SaperaXferCallback(SapXferCallbackInfo* pInfo)
{
    SaperaDirectAccess* pSapera = static_cast<SaperaDirectAccess*>(pInfo->GetContext());
    if (pSapera)
    {
        // Process the new frame
        pSapera->processNewFrame(pSapera->buffer_);
    }
}

SaperaDirectAccess::SaperaDirectAccess(QObject* parent)
    : QObject(parent)
{
    // Create empty placeholder image
    currentFrame_ = QImage(640, 480, QImage::Format_RGB32);
    currentFrame_.fill(Qt::black);
    
    QPainter painter(&currentFrame_);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 16));
    painter.drawText(QRect(0, 0, 640, 480), Qt::AlignCenter, "Direct Camera Access\n(Mock Implementation)");
    painter.end();
}

SaperaDirectAccess::~SaperaDirectAccess() {
    // Make sure to stop acquisition before destroying objects
    if (isAcquiring_) {
        stopAcquisition();
    }
    
    destroySaperaObjects();
}

bool SaperaDirectAccess::initialize(const std::string& serverName) {
    // Store server name
    serverName_ = serverName;
    
    // Create Sapera objects
    if (!createSaperaObjects()) {
        emit error(tr("Failed to create Sapera objects"));
        return false;
    }
    
    // Configure camera
    if (!configureCamera()) {
        emit error(tr("Failed to configure camera"));
        return false;
    }
    
    emit statusChanged(tr("Camera initialized: %1").arg(QString::fromStdString(serverName_)));
    return true;
}

bool SaperaDirectAccess::createSaperaObjects() {
    // Create acquisition device
    acqDevice_ = new SapAcqDevice(serverName_.c_str());
    if (!acqDevice_->Create()) {
        lastError_ = tr("Failed to create acquisition device");
        emit error(lastError_);
        return false;
    }
    
    // Create buffer (2 buffers for continuous acquisition)
    buffer_ = new SapBufferWithTrash(2, acqDevice_);
    if (!buffer_->Create()) {
        lastError_ = tr("Failed to create buffer");
        emit error(lastError_);
        acqDevice_->Destroy();
        delete acqDevice_;
        acqDevice_ = nullptr;
        return false;
    }
    
    // Create transfer object with callback
    transfer_ = new SapAcqDeviceToBuf(acqDevice_, buffer_);
    transfer_->SetCallbackInfo(SaperaXferCallback, this);
    
    if (!transfer_->Create()) {
        lastError_ = tr("Failed to create transfer");
        emit error(lastError_);
        buffer_->Destroy();
        delete buffer_;
        buffer_ = nullptr;
        acqDevice_->Destroy();
        delete acqDevice_;
        acqDevice_ = nullptr;
        return false;
    }
    
    // Create view object (optional, for debugging)
    view_ = new SapView(buffer_);
    if (!view_->Create()) {
        // Just log warning, view is not critical
        qDebug() << "Warning: Failed to create Sapera view - continuing without it";
    }
    
    return true;
}

void SaperaDirectAccess::destroySaperaObjects() {
    // Destroy objects in reverse order
    if (view_) {
        view_->Destroy();
        delete view_;
        view_ = nullptr;
    }
    
    if (transfer_) {
        transfer_->Destroy();
        delete transfer_;
        transfer_ = nullptr;
    }
    
    if (buffer_) {
        buffer_->Destroy();
        delete buffer_;
        buffer_ = nullptr;
    }
    
    if (acqDevice_) {
        acqDevice_->Destroy();
        delete acqDevice_;
        acqDevice_ = nullptr;
    }
}

bool SaperaDirectAccess::configureCamera() {
    if (!acqDevice_) return false;
    
    emit statusChanged(tr("Configuring camera..."));
    
    // Print important camera information
    printFeatureValue("DeviceModelName");
    printFeatureValue("DeviceSerialNumber");
    printFeatureValue("DeviceFirmwareVersion");
    printFeatureValue("DeviceUserID");
    
    // Set acquisition mode to continuous
    if (acqDevice_->SetFeatureValue("AcquisitionMode", "Continuous")) {
        emit statusChanged(tr("Set acquisition mode to Continuous"));
    }
    
    // Set exposure mode to Timed
    if (acqDevice_->SetFeatureValue("ExposureMode", "Timed")) {
        emit statusChanged(tr("Set exposure mode to Timed"));
    }
    
    // Set exposure time
    if (acqDevice_->SetFeatureValue("ExposureTime", exposureTime_)) {
        emit statusChanged(tr("Set exposure time to %1 μs").arg(exposureTime_));
    }
    
    return true;
}

// Helper function to safely print feature values
void SaperaDirectAccess::printFeatureValue(const char* featureName) {
    if (!acqDevice_) return;
    
    char value[256] = {0};
    if (acqDevice_->GetFeatureValue(featureName, value, sizeof(value))) {
        QString message = QString("%1: %2").arg(featureName).arg(value);
        qDebug() << message;
        emit statusChanged(message);
    }
}

bool SaperaDirectAccess::startAcquisition() {
    if (!transfer_ || isAcquiring_) return false;
    
    // Start continuous grab
    if (transfer_->Grab()) {
        isAcquiring_ = true;
        emit statusChanged(tr("Started image acquisition"));
        
        // For the mock implementation, let's create a simulated image
        QImage mockImage(640, 480, QImage::Format_RGB32);
        mockImage.fill(Qt::darkGray);
        
        QPainter painter(&mockImage);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 16));
        painter.drawText(10, 30, tr("Live Feed (Mock)"));
        painter.drawText(10, 60, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
        painter.drawText(10, 90, tr("Exposure: %1 μs").arg(exposureTime_));
        
        // Draw some simulated patterns
        painter.setPen(QPen(Qt::red, 2));
        painter.drawEllipse(QPoint(320, 240), 100, 100);
        painter.setPen(QPen(Qt::green, 2));
        painter.drawEllipse(QPoint(320, 240), 150, 150);
        painter.setPen(QPen(Qt::blue, 2));
        painter.drawEllipse(QPoint(320, 240), 200, 200);
        
        // Draw crosshair
        painter.setPen(QPen(Qt::white, 1));
        painter.drawLine(320, 0, 320, 480);
        painter.drawLine(0, 240, 640, 240);
        
        // Add some noise for realism
        for (int i = 0; i < 1000; i++) {
            int x = QRandomGenerator::global()->bounded(640);
            int y = QRandomGenerator::global()->bounded(480);
            painter.setPen(Qt::white);
            painter.drawPoint(x, y);
        }
        
        currentFrame_ = mockImage;
        emit newFrameAvailable(currentFrame_);
        
        return true;
    } else {
        lastError_ = tr("Failed to start acquisition");
        emit error(lastError_);
        return false;
    }
}

bool SaperaDirectAccess::stopAcquisition() {
    if (!transfer_ || !isAcquiring_) return false;
    
    // Stop acquisition
    transfer_->Freeze();
    
    isAcquiring_ = false;
    emit statusChanged(tr("Stopped image acquisition"));
    
    // Update displayed image to show not acquiring
    QImage stoppedImage = currentFrame_.copy();
    
    QPainter painter(&stoppedImage);
    painter.fillRect(0, 0, stoppedImage.width(), 30, QColor(0, 0, 0, 180));
    painter.setPen(Qt::red);
    painter.setFont(QFont("Arial", 16, QFont::Bold));
    painter.drawText(10, 25, tr("STOPPED"));
    
    currentFrame_ = stoppedImage;
    emit newFrameAvailable(currentFrame_);
    
    return true;
}

void SaperaDirectAccess::processNewFrame(SapBuffer* pBuffer) {
    if (!pBuffer) return;
    
    // Get buffer information
    int width = pBuffer->GetWidth();
    int height = pBuffer->GetHeight();
    int format = pBuffer->GetFormat();
    
    // Create QImage from buffer data
    QImage newFrame;
    
    // Get buffer address directly using the GetAddress method with a pointer
    void* pData = nullptr;
    if (!pBuffer->GetAddress(&pData) || !pData) {
        return;
    }
    
    // Define format constants locally for the switch statement
    constexpr int FORMAT_MONO8 = 0;  // SAPBUFFER_FORMAT_MONO8
    constexpr int FORMAT_MONO16 = 1; // SAPBUFFER_FORMAT_MONO16
    constexpr int FORMAT_RGB24 = 2;  // SAPBUFFER_FORMAT_RGB24
    constexpr int FORMAT_RGB32 = 3;  // SAPBUFFER_FORMAT_RGB32
    
    // Create QImage based on format
    // Note: in a real implementation, we would handle different pixel formats
    switch (format) {
        case FORMAT_MONO8:
            newFrame = QImage(static_cast<uchar*>(pData), width, height, width, QImage::Format_Grayscale8);
            break;
        case FORMAT_RGB24:
            newFrame = QImage(static_cast<uchar*>(pData), width, height, width * 3, QImage::Format_RGB888);
            break;
        case FORMAT_RGB32:
            newFrame = QImage(static_cast<uchar*>(pData), width, height, width * 4, QImage::Format_RGB32);
            break;
        default:
            // Create a mock image if format is not supported
            newFrame = QImage(width, height, QImage::Format_RGB32);
            newFrame.fill(Qt::darkGray);
            break;
    }
    
    // Make a deep copy of the image to ensure it remains valid
    currentFrame_ = newFrame.copy();
    
    // Add overlay with camera information
    QPainter painter(&currentFrame_);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12));
    painter.drawText(10, 20, tr("Camera: %1").arg(QString::fromStdString(serverName_)));
    painter.drawText(10, 40, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    painter.drawText(10, 60, tr("Exposure: %1 μs").arg(exposureTime_, 0, 'f', 1));
    
    // Emit signal with the new frame
    emit newFrameAvailable(currentFrame_);
}

bool SaperaDirectAccess::setExposureTime(double microseconds) {
    if (!acqDevice_) return false;
    
    if (microseconds <= 0) {
        lastError_ = tr("Invalid exposure time: %1").arg(microseconds);
        emit error(lastError_);
        return false;
    }
    
    if (acqDevice_->SetFeatureValue("ExposureTime", microseconds)) {
        exposureTime_ = microseconds;
        emit statusChanged(tr("Exposure time set to %1 μs").arg(microseconds));
        return true;
    } else {
        lastError_ = tr("Failed to set exposure time");
        emit error(lastError_);
        return false;
    }
}

double SaperaDirectAccess::getExposureTime() const {
    return exposureTime_;
}

std::string SaperaDirectAccess::getCameraModelName() const {
    if (!acqDevice_) return "Unknown";
    
    char modelName[256] = {0};
    if (acqDevice_->GetFeatureValue("DeviceModelName", modelName, sizeof(modelName))) {
        return modelName;
    }
    return "Unknown Model";
}

std::string SaperaDirectAccess::getCameraSerialNumber() const {
    if (!acqDevice_) return "Unknown";
    
    char serialNumber[256] = {0};
    if (acqDevice_->GetFeatureValue("DeviceSerialNumber", serialNumber, sizeof(serialNumber))) {
        return serialNumber;
    }
    return "Unknown SN";
}

std::string SaperaDirectAccess::getCameraFirmwareVersion() const {
    if (!acqDevice_) return "Unknown";
    
    char firmwareVersion[256] = {0};
    if (acqDevice_->GetFeatureValue("DeviceFirmwareVersion", firmwareVersion, sizeof(firmwareVersion))) {
        return firmwareVersion;
    }
    return "Unknown Version";
}

std::vector<std::string> SaperaDirectAccess::getAvailablePixelFormats() const {
    std::vector<std::string> formats;
    
    if (!acqDevice_) return formats;
    
    // In the real implementation, we would query the camera for supported pixel formats
    // For the mock implementation, return some common formats
    formats.push_back("Mono8");
    formats.push_back("Mono16");
    formats.push_back("RGB24");
    formats.push_back("RGB32");
    
    return formats;
}

bool SaperaDirectAccess::setPixelFormat(const std::string& format) {
    if (!acqDevice_) return false;
    
    if (acqDevice_->SetFeatureValue("PixelFormat", format.c_str())) {
        emit statusChanged(tr("Pixel format set to %1").arg(QString::fromStdString(format)));
        return true;
    } else {
        lastError_ = tr("Failed to set pixel format to %1").arg(QString::fromStdString(format));
        emit error(lastError_);
        return false;
    }
}

QImage SaperaDirectAccess::getCurrentFrame() const {
    return currentFrame_.copy();
}

std::vector<std::string> SaperaDirectAccess::getAvailableCameras() {
    std::vector<std::string> cameras;
    cam_matrix::core::SaperaUtils::getAvailableCameras(cameras);
    return cameras;
}

bool SaperaDirectAccess::isAcquiring() const {
    return isAcquiring_;
}

} // namespace cam_matrix::core

#endif 