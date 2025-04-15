#include "core/sapera_direct_access.hpp"

#if defined(SAPERA_FOUND)
#include <QDebug>
#include <cstring>

namespace cam_matrix::core {

// Callback function for the transfer
void XferCallback(SapXferCallbackInfo* pInfo) {
    SaperaDirectAccess* pSapera = static_cast<SaperaDirectAccess*>(pInfo->GetContext());
    if (pSapera && pInfo->IsNewBuffer()) {
        void* pBuffer = pInfo->GetBuffer()->GetVirtualAddress();
        pSapera->processNewFrame(pBuffer);
    }
}

SaperaDirectAccess::SaperaDirectAccess(QObject* parent)
    : QObject(parent)
    , acqDevice_(nullptr)
    , buffer_(nullptr)
    , transfer_(nullptr)
    , exposureTime_(10000.0) // 10ms default
    , isAcquiring_(false)
{
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
        return false;
    }
    
    // Create transfer object with callback
    transfer_ = new SapAcqDeviceToBuf(acqDevice_, buffer_);
    transfer_->SetCallbackInfo(XferCallback, this);
    
    if (!transfer_->Create()) {
        lastError_ = tr("Failed to create transfer");
        emit error(lastError_);
        buffer_->Destroy();
        acqDevice_->Destroy();
        return false;
    }
    
    return true;
}

void SaperaDirectAccess::destroySaperaObjects() {
    // Destroy objects in reverse order
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
    
    // Set acquisition mode to continuous
    BOOL isAvailable = FALSE;
    if (acqDevice_->IsFeatureAvailable("AcquisitionMode", &isAvailable) && isAvailable) {
        acqDevice_->SetFeatureValue("AcquisitionMode", "Continuous");
        emit statusChanged(tr("Set acquisition mode to Continuous"));
    }
    
    // Set exposure mode to Timed
    if (acqDevice_->IsFeatureAvailable("ExposureMode", &isAvailable) && isAvailable) {
        acqDevice_->SetFeatureValue("ExposureMode", "Timed");
        emit statusChanged(tr("Set exposure mode to Timed"));
    }
    
    // Set exposure time
    if (acqDevice_->IsFeatureAvailable("ExposureTime", &isAvailable) && isAvailable) {
        acqDevice_->SetFeatureValue("ExposureTime", exposureTime_);
        emit statusChanged(tr("Set exposure time to %1 μs").arg(exposureTime_));
    }
    
    // Get camera information
    emit statusChanged(tr("Camera Model: %1").arg(QString::fromStdString(getCameraModelName())));
    emit statusChanged(tr("Serial Number: %1").arg(QString::fromStdString(getCameraSerialNumber())));
    emit statusChanged(tr("Firmware Version: %1").arg(QString::fromStdString(getCameraFirmwareVersion())));
    
    return true;
}

void SaperaDirectAccess::printFeatureValue(const char* featureName) {
    if (!acqDevice_) return;
    
    BOOL isAvailable = FALSE;
    if (acqDevice_->IsFeatureAvailable(featureName, &isAvailable) && isAvailable) {
        // Get feature info to check access mode
        SapFeature feature(acqDevice_->GetLocation());
        if (feature.Create() && acqDevice_->GetFeatureInfo(featureName, &feature)) {
            SapFeature::AccessMode accessMode;
            if (feature.GetAccessMode(&accessMode)) {
                if (accessMode == SapFeature::AccessRO || accessMode == SapFeature::AccessRW) {
                    char value[256] = {0};
                    if (acqDevice_->GetFeatureValue(featureName, value, sizeof(value))) {
                        qDebug() << featureName << ":" << value;
                    }
                }
            }
            feature.Destroy();
        }
    }
}

bool SaperaDirectAccess::startAcquisition() {
    if (!transfer_ || isAcquiring_) return false;
    
    // Start continuous grab
    if (transfer_->Grab()) {
        isAcquiring_ = true;
        emit statusChanged(tr("Started image acquisition"));
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
    if (!transfer_->Wait(5000)) {
        lastError_ = tr("Warning: Grab could not stop properly");
        emit error(lastError_);
    }
    
    isAcquiring_ = false;
    emit statusChanged(tr("Stopped image acquisition"));
    return true;
}

bool SaperaDirectAccess::isAcquiring() const {
    return isAcquiring_;
}

void SaperaDirectAccess::processNewFrame(void* pBuffer) {
    if (!buffer_ || !pBuffer) return;
    
    // Get buffer format
    SapFormat format = buffer_->GetFormat();
    int width = buffer_->GetWidth();
    int height = buffer_->GetHeight();
    
    // Convert to QImage
    QImage::Format qFormat;
    
    switch (format) {
        case SapFormat::Mono8:
            qFormat = QImage::Format_Grayscale8;
            break;
        case SapFormat::RGB888:
            qFormat = QImage::Format_RGB888;
            break;
        case SapFormat::RGB8888:
            qFormat = QImage::Format_RGBA8888;
            break;
        default:
            // Unsupported format, try to treat as grayscale
            qFormat = QImage::Format_Grayscale8;
            break;
    }
    
    // Create QImage from buffer
    currentFrame_ = QImage(static_cast<uchar*>(pBuffer), width, height, qFormat).copy();
    
    // Emit signal
    emit newFrameAvailable(currentFrame_);
}

bool SaperaDirectAccess::setExposureTime(double microseconds) {
    if (!acqDevice_) return false;
    
    BOOL isAvailable = FALSE;
    if (acqDevice_->IsFeatureAvailable("ExposureTime", &isAvailable) && isAvailable) {
        if (acqDevice_->SetFeatureValue("ExposureTime", microseconds)) {
            exposureTime_ = microseconds;
            emit statusChanged(tr("Set exposure time to %1 μs").arg(exposureTime_));
            return true;
        }
    }
    
    return false;
}

double SaperaDirectAccess::getExposureTime() const {
    return exposureTime_;
}

std::string SaperaDirectAccess::getCameraModelName() const {
    if (!acqDevice_) return "";
    
    char value[256] = {0};
    BOOL isAvailable = FALSE;
    if (acqDevice_->IsFeatureAvailable("DeviceModelName", &isAvailable) && isAvailable) {
        if (acqDevice_->GetFeatureValue("DeviceModelName", value, sizeof(value))) {
            return value;
        }
    }
    
    return "";
}

std::string SaperaDirectAccess::getCameraSerialNumber() const {
    if (!acqDevice_) return "";
    
    char value[256] = {0};
    BOOL isAvailable = FALSE;
    if (acqDevice_->IsFeatureAvailable("DeviceSerialNumber", &isAvailable) && isAvailable) {
        if (acqDevice_->GetFeatureValue("DeviceSerialNumber", value, sizeof(value))) {
            return value;
        }
    }
    
    return "";
}

std::string SaperaDirectAccess::getCameraFirmwareVersion() const {
    if (!acqDevice_) return "";
    
    char value[256] = {0};
    BOOL isAvailable = FALSE;
    if (acqDevice_->IsFeatureAvailable("DeviceFirmwareVersion", &isAvailable) && isAvailable) {
        if (acqDevice_->GetFeatureValue("DeviceFirmwareVersion", value, sizeof(value))) {
            return value;
        }
    }
    
    return "";
}

std::vector<std::string> SaperaDirectAccess::getAvailablePixelFormats() const {
    std::vector<std::string> formats;
    
    if (!acqDevice_) return formats;
    
    BOOL isAvailable = FALSE;
    if (acqDevice_->IsFeatureAvailable("PixelFormat", &isAvailable) && isAvailable) {
        SapFeature feature(acqDevice_->GetLocation());
        if (feature.Create() && acqDevice_->GetFeatureInfo("PixelFormat", &feature)) {
            int enumCount = 0;
            if (feature.GetEnumCount(&enumCount)) {
                for (int i = 0; i < enumCount; i++) {
                    char enumString[256] = {0};
                    if (feature.GetEnumString(i, enumString, sizeof(enumString))) {
                        formats.push_back(enumString);
                    }
                }
            }
            feature.Destroy();
        }
    }
    
    return formats;
}

bool SaperaDirectAccess::setPixelFormat(const std::string& format) {
    if (!acqDevice_) return false;
    
    BOOL isAvailable = FALSE;
    if (acqDevice_->IsFeatureAvailable("PixelFormat", &isAvailable) && isAvailable) {
        if (acqDevice_->SetFeatureValue("PixelFormat", format.c_str())) {
            emit statusChanged(tr("Set pixel format to %1").arg(QString::fromStdString(format)));
            return true;
        }
    }
    
    return false;
}

QImage SaperaDirectAccess::getCurrentFrame() const {
    return currentFrame_;
}

std::vector<std::string> SaperaDirectAccess::getAvailableCameras() {
    std::vector<std::string> cameras;
    
    // Get server count
    int serverCount = SapManager::GetServerCount();
    
    // Iterate through servers
    for (int i = 0; i < serverCount; i++) {
        char serverName[CORSERVER_MAX_STRLEN];
        if (SapManager::GetServerName(i, serverName, sizeof(serverName))) {
            cameras.push_back(serverName);
        }
    }
    
    return cameras;
}

} // namespace cam_matrix::core

#endif 