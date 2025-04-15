#include "core/sapera_camera.hpp"
#include <QDebug>
#include <QColor>
#include <QString>

namespace cam_matrix::core {

#if HAS_SAPERA

SaperaCamera::SaperaCamera(const std::string& name)
    : name_(name)
    , isConnected_(false)
{
    // Create empty QImage for now
    currentFrame_ = QImage(640, 480, QImage::Format_RGB32);
    currentFrame_.fill(Qt::black);
}

SaperaCamera::~SaperaCamera() {
    if (isConnected_) {
        disconnectCamera();
    }
}

bool SaperaCamera::connectCamera() {
    if (isConnected_) {
        return true;
    }

    // TODO: Implement actual Sapera camera connection
    // This is a placeholder implementation
    isConnected_ = true;
    emit statusChanged("Camera connected: " + name_);
    return true;
}

bool SaperaCamera::disconnectCamera() {
    if (!isConnected_) {
        return true;
    }

    // TODO: Implement actual Sapera camera disconnection
    // This is a placeholder implementation
    isConnected_ = false;
    emit statusChanged("Camera disconnected: " + name_);
    return true;
}

bool SaperaCamera::isConnected() const {
    return isConnected_;
}

std::string SaperaCamera::getName() const {
    return name_;
}

bool SaperaCamera::setExposureTime(double microseconds) {
    if (!isConnected_) {
        emit error("Cannot set exposure time: camera not connected");
        return false;
    }

    // TODO: Implement actual Sapera exposure time setting
    // This is a placeholder implementation
    emit statusChanged("Exposure time set to " + std::to_string(microseconds) + " microseconds");
    return true;
}

double SaperaCamera::getExposureTime() const {
    if (!isConnected_ || !device_) {
        return 0.0;
    }

    if (!isFeatureAvailable("ExposureTime")) {
        return 0.0;
    }

    double exposureTime = 0.0;
    if (device_->GetFeatureValue("ExposureTime", &exposureTime)) {
        return exposureTime;
    }

    return 0.0;
}

QImage SaperaCamera::getFrame() const {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return currentFrame_.copy();
}

bool SaperaCamera::configureCamera() {
    if (!device_) {
        return false;
    }

    emit statusChanged("Configuring camera...");

    // Print camera information
    printCameraInfo();

    // Configure acquisition mode
    if (isFeatureAvailable("AcquisitionMode")) {
        device_->SetFeatureValue("AcquisitionMode", "Continuous");
    }

    // Configure exposure mode
    if (isFeatureAvailable("ExposureMode")) {
        device_->SetFeatureValue("ExposureMode", "Timed");
    }

    // Set default exposure time
    if (isFeatureAvailable("ExposureTime")) {
        double exposureTime = 10000.0; // 10ms default
        device_->SetFeatureValue("ExposureTime", exposureTime);
    }

    return true;
}

void SaperaCamera::printCameraInfo() const {
    if (!device_) {
        return;
    }

    std::string infoMessage = "Camera Information:\n";

    // Print some important camera information
    const char* important_features[] = {
        "DeviceModelName",
        "DeviceSerialNumber",
        "DeviceFirmwareVersion",
        "DeviceUserID"
    };

    for (const char* feature : important_features) {
        if (isFeatureAvailable(feature)) {
            char value[256] = {0};
            if (device_->GetFeatureValue(feature, value, sizeof(value))) {
                infoMessage += std::string(feature) + ": " + std::string(value) + "\n";
            }
        }
    }

    const_cast<SaperaCamera*>(this)->statusChanged(infoMessage);
}

bool SaperaCamera::createSaperaObjects() {
    try {
        // Create acquisition device
        device_ = std::make_unique<SapAcqDevice>(name_.c_str());
        if (!device_->Create()) {
            emit error("Failed to create acquisition device");
            return false;
        }

        // Create buffer
        buffer_ = std::make_unique<SapBufferWithTrash>(2, device_.get());
        if (!buffer_->Create()) {
            emit error("Failed to create buffer");
            device_->Destroy();
            return false;
        }

        // Create transfer object with callback
        transfer_ = std::make_unique<SapAcqDeviceToBuf>(device_.get(), buffer_.get());
        transfer_->SetCallbackInfo(XferCallback, this);
        if (!transfer_->Create()) {
            emit error("Failed to create transfer");
            buffer_->Destroy();
            device_->Destroy();
            return false;
        }

        // Create view object
        view_ = std::make_unique<SapView>(buffer_.get());
        if (!view_->Create()) {
            emit error("Failed to create view");
            transfer_->Destroy();
            buffer_->Destroy();
            device_->Destroy();
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        emit error(std::string("Exception creating Sapera objects: ") + e.what());
        return false;
    }
}

void SaperaCamera::destroySaperaObjects() {
    if (view_) {
        view_->Destroy();
        view_.reset();
    }

    if (transfer_) {
        transfer_->Destroy();
        transfer_.reset();
    }

    if (buffer_) {
        buffer_->Destroy();
        buffer_.reset();
    }

    if (device_) {
        device_->Destroy();
        device_.reset();
    }
}

void SaperaCamera::updateFrameFromBuffer() {
    if (!buffer_) {
        return;
    }

    // Get the buffer information
    void* pData = nullptr;

    try {
        // Get the current buffer index
        UINT32 index = buffer_->GetIndex();

        // Get the buffer data
        if (buffer_->GetAddress(&pData)) {
            // Get buffer properties - use the correct method signature
            UINT32 width = buffer_->GetWidth();
            UINT32 height = buffer_->GetHeight();
            UINT32 pitch = buffer_->GetPitch();

            // Lock the frame mutex
            std::lock_guard<std::mutex> lock(frameMutex_);

            // Create QImage from buffer data (assuming 8-bit grayscale for now)
            QImage newFrame(width, height, QImage::Format_RGB32);

            const uint8_t* src = static_cast<const uint8_t*>(pData);
            for (UINT32 y = 0; y < height; y++) {
                for (UINT32 x = 0; x < width; x++) {
                    uint8_t pixel = src[y * pitch + x];
                    newFrame.setPixelColor(x, y, QColor(pixel, pixel, pixel));
                }
            }

            currentFrame_ = newFrame;
            emit newFrameAvailable(currentFrame_);
        }
    } catch (const std::exception& e) {
        qDebug() << "Error updating frame:" << e.what();
    }
}

void SaperaCamera::XferCallback(SapXferCallbackInfo *pInfo) {
    SaperaCamera* camera = static_cast<SaperaCamera*>(pInfo->GetContext());
    if (camera) {
        camera->updateFrameFromBuffer();
    }
}

void SaperaCamera::printFeatureValue(const char* featureName) const {
    if (!device_) {
        return;
    }

    BOOL isAvailable = FALSE;
    if (device_->IsFeatureAvailable(featureName, &isAvailable) && isAvailable) {
        // Get feature info to check access mode
        SapFeature feature(device_->GetLocation());
        if (feature.Create() && device_->GetFeatureInfo(featureName, &feature)) {
            SapFeature::AccessMode accessMode;
            if (feature.GetAccessMode(&accessMode)) {
                if (accessMode == SapFeature::AccessRO || accessMode == SapFeature::AccessRW) {
                    char valueBuffer[256] = {0};
                    if (device_->GetFeatureValue(featureName, valueBuffer, sizeof(valueBuffer))) {
                        // Use const_cast to emit signal from const method
                        const_cast<SaperaCamera*>(this)->statusChanged(std::string(featureName) + ": " + std::string(valueBuffer));
                    }
                }
            }
            feature.Destroy();
        }
    }
}

bool SaperaCamera::isFeatureAvailable(const char* featureName) const {
    if (!device_) {
        return false;
    }

    BOOL isAvailable = FALSE;
    if (device_->IsFeatureAvailable(featureName, &isAvailable)) {
        return isAvailable == TRUE;
    }
    return false;
}

#endif // HAS_SAPERA

} // namespace cam_matrix::core


