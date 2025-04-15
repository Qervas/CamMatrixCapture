#include "core/sapera_direct_access.hpp"

#if defined(SAPERA_FOUND)
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
    , acqDevice_(nullptr)
    , buffer_(nullptr)
    , transfer_(nullptr)
    , view_(nullptr)
    , exposureTime_(10000.0) // 10ms default
    , isAcquiring_(false)
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
    acqDevice_->SetFeatureValue("AcquisitionMode", "Continuous");
    emit statusChanged(tr("Set acquisition mode to Continuous"));
    
    // Set exposure mode to Timed
    acqDevice_->SetFeatureValue("ExposureMode", "Timed");
    emit statusChanged(tr("Set exposure mode to Timed"));
    
    // Set exposure time
    acqDevice_->SetFeatureValue("ExposureTime", exposureTime_);
    emit statusChanged(tr("Set exposure time to %1 μs").arg(exposureTime_));
    
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

bool SaperaDirectAccess::isAcquiring() const {
    return isAcquiring_;
}

void SaperaDirectAccess::processNewFrame(SapBuffer* pBuffer) {
    // In the mock implementation, we'll generate a new frame periodically
    static int frameCount = 0;
    frameCount++;
    
    if (frameCount % 30 == 0) { // Update frame about once per second
        QImage newFrame(640, 480, QImage::Format_RGB32);
        newFrame.fill(Qt::darkGray);
        
        QPainter painter(&newFrame);
        
        // Draw some dynamic elements
        qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
        double phase = timestamp * 0.001;
        
        // Draw moving object
        int centerX = 320 + int(150 * cos(phase));
        int centerY = 240 + int(100 * sin(phase * 0.7));
        
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(QColor(255, 128, 0));
        painter.drawEllipse(QPoint(centerX, centerY), 40, 40);
        
        // Draw crosshair
        painter.setPen(QPen(Qt::green, 1));
        painter.drawLine(centerX - 50, centerY, centerX + 50, centerY);
        painter.drawLine(centerX, centerY - 50, centerX, centerY + 50);
        
        // Draw some info text
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 12));
        painter.drawText(10, 20, tr("Mock Camera: %1").arg(QString::fromStdString(serverName_)));
        painter.drawText(10, 40, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
        painter.drawText(10, 60, tr("Frame: %1").arg(frameCount));
        painter.drawText(10, 80, tr("Exposure: %1 μs").arg(exposureTime_));
        
        // Add some noise for realism
        for (int i = 0; i < 500; i++) {
            int x = QRandomGenerator::global()->bounded(640);
            int y = QRandomGenerator::global()->bounded(480);
            int intensity = QRandomGenerator::global()->bounded(128) + 128;
            painter.setPen(QColor(intensity, intensity, intensity));
            painter.drawPoint(x, y);
        }
        
        currentFrame_ = newFrame;
        emit newFrameAvailable(currentFrame_);
    }
}

bool SaperaDirectAccess::setExposureTime(double microseconds) {
    if (!acqDevice_) return false;
    
    if (acqDevice_->SetFeatureValue("ExposureTime", microseconds)) {
        exposureTime_ = microseconds;
        emit statusChanged(tr("Set exposure time to %1 μs").arg(exposureTime_));
        return true;
    }
    
    return false;
}

double SaperaDirectAccess::getExposureTime() const {
    return exposureTime_;
}

std::string SaperaDirectAccess::getCameraModelName() const {
    if (!acqDevice_) return "Unknown Model";
    
    char value[256] = {0};
    if (acqDevice_->GetFeatureValue("DeviceModelName", value, sizeof(value))) {
        return value;
    }
    
    return "Nano-C4020 (Mock)";
}

std::string SaperaDirectAccess::getCameraSerialNumber() const {
    if (!acqDevice_) return "Unknown SN";
    
    char value[256] = {0};
    if (acqDevice_->GetFeatureValue("DeviceSerialNumber", value, sizeof(value))) {
        return value;
    }
    
    return "SN12345678";
}

std::string SaperaDirectAccess::getCameraFirmwareVersion() const {
    if (!acqDevice_) return "Unknown Version";
    
    char value[256] = {0};
    if (acqDevice_->GetFeatureValue("DeviceFirmwareVersion", value, sizeof(value))) {
        return value;
    }
    
    return "1.0.0";
}

std::vector<std::string> SaperaDirectAccess::getAvailablePixelFormats() const {
    std::vector<std::string> formats = {
        "Mono8",
        "Mono16",
        "RGB8",
        "RGB16",
        "YUV422_8"
    };
    
    return formats;
}

bool SaperaDirectAccess::setPixelFormat(const std::string& format) {
    if (!acqDevice_) return false;
    
    if (acqDevice_->SetFeatureValue("PixelFormat", format.c_str())) {
        emit statusChanged(tr("Set pixel format to %1").arg(QString::fromStdString(format)));
        return true;
    }
    
    return false;
}

QImage SaperaDirectAccess::getCurrentFrame() const {
    return currentFrame_;
}

std::vector<std::string> SaperaDirectAccess::getAvailableCameras() {
    std::vector<std::string> cameras;
    
    // Add some mock cameras
    cameras.push_back("Nano-C4020 (Mock Camera 1)");
    cameras.push_back("Nano-C4020 (Mock Camera 2)");
    cameras.push_back("Linea-CLHS (Mock Camera 3)");
    
    return cameras;
}

} // namespace cam_matrix::core

#endif 