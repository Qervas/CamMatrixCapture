#include "core/sapera_camera.hpp"
#include <QDebug>
#include <QColor>
#include <QString>
#include <QRandomGenerator>
#include <QDateTime>
#include <QPainter>
#include <QMetaObject>
#include <QFont>
#include <cmath>

namespace cam_matrix::core {

// FrameGeneratorWorker implementation
FrameGeneratorWorker::FrameGeneratorWorker(QObject* parent)
    : QObject(parent)
{
}

FrameGeneratorWorker::~FrameGeneratorWorker() 
{
    stop();
}

void FrameGeneratorWorker::setCamera(const std::string& name, double* exposureTime) 
{
    cameraName_ = name;
    exposureTimePtr_ = exposureTime;
}

void FrameGeneratorWorker::stop() 
{
    running_ = false;
}

void FrameGeneratorWorker::generateFrames() 
{
    running_ = true;
    
    try {
        while (running_) {
            if (!running_) break;  // Check again in case it changed during processing
            
            // Get current timestamp for animation
            qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
            int pattern = (timestamp / 1000) % 4; // Change pattern every second
            
            // Generate the frame based on the pattern and make a copy to avoid any threading issues
            QImage frame = generatePattern(pattern, timestamp).copy();
            
            // Use queued connection to safely emit the signal across thread boundaries
            QMetaObject::invokeMethod(this, [this, frame]() {
                emit frameGenerated(frame);
            }, Qt::QueuedConnection);
            
            // Sleep for about 33ms (30fps)
            QThread::msleep(33);
        }
    }
    catch (const std::exception& e) {
        qDebug() << "Exception in frame generator:" << e.what();
    }
    catch (...) {
        qDebug() << "Unknown exception in frame generator";
    }
    
    // Always emit finished even if we exit due to an exception
    // Make sure this is queued too
    QMetaObject::invokeMethod(this, [this]() {
        emit finished();
    }, Qt::QueuedConnection);
}

QImage FrameGeneratorWorker::generatePattern(int pattern, qint64 timestamp) 
{
    QImage newFrame(640, 480, QImage::Format_RGB32);
    double exposureValue = (exposureTimePtr_) ? *exposureTimePtr_ : 10000.0;
    
    switch (pattern) {
        case 0: // Gradient
            for (int y = 0; y < newFrame.height(); y++) {
                for (int x = 0; x < newFrame.width(); x++) {
                    int r = (x * 255) / newFrame.width();
                    int g = (y * 255) / newFrame.height();
                    int b = 128 + (sin(timestamp * 0.001) * 127);
                    newFrame.setPixelColor(x, y, QColor(r, g, b));
                }
            }
            break;
            
        case 1: // Moving lines
            newFrame.fill(Qt::black);
            for (int y = 0; y < newFrame.height(); y++) {
                int offset = (timestamp / 20) % newFrame.width();
                for (int x = 0; x < newFrame.width(); x += 20) {
                    int lineX = (x + offset) % newFrame.width();
                    for (int i = 0; i < 10 && lineX + i < newFrame.width(); i++) {
                        newFrame.setPixelColor(lineX + i, y, QColor(200, 200, 200));
                    }
                }
            }
            break;
            
        case 2: // Checkerboard
            {
                int squareSize = 40;
                int offset = (timestamp / 100) % squareSize;
                for (int y = 0; y < newFrame.height(); y++) {
                    for (int x = 0; x < newFrame.width(); x++) {
                        int adjustedX = (x + offset) / squareSize;
                        int adjustedY = y / squareSize;
                        bool isWhite = (adjustedX + adjustedY) % 2 == 0;
                        newFrame.setPixelColor(x, y, isWhite ? QColor(230, 230, 230) : QColor(30, 30, 30));
                    }
                }
            }
            break;
            
        case 3: // Random noise
            for (int y = 0; y < newFrame.height(); y++) {
                for (int x = 0; x < newFrame.width(); x++) {
                    int noise = QRandomGenerator::global()->bounded(256);
                    newFrame.setPixelColor(x, y, QColor(noise, noise, noise));
                }
            }
            break;
    }
    
    // Add camera name and timestamp overlay
    QPainter painter(&newFrame);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 16));
    painter.drawText(10, 30, QString::fromStdString(cameraName_));
    painter.drawText(10, 60, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    painter.drawText(10, 90, QString("Exposure: %1 μs").arg(exposureValue));
    
    return newFrame;
}

#if HAS_SAPERA

SaperaCamera::SaperaCamera(const std::string& name)
    : name_(name)
    , isConnected_(false)
    , exposureTime_(10000.0) // Default to 10ms
{
    // Create empty QImage for now
    currentFrame_ = QImage(640, 480, QImage::Format_RGB32);
    currentFrame_.fill(Qt::black);
    
    // Draw camera name on start image
    QPainter painter(&currentFrame_);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(currentFrame_.rect(), Qt::AlignCenter, 
                     QString::fromStdString(name_) + "\nNot Connected");
    painter.end();
    
    // Create the frame generator but don't start it yet
    frameGenerator_ = new FrameGeneratorWorker();
    frameGenerator_->setCamera(name_, &exposureTime_);
    frameGenerator_->moveToThread(&frameGeneratorThread_);
    
    // Connect signals and slots with proper connection types
    connect(&frameGeneratorThread_, &QThread::started, 
            frameGenerator_, &FrameGeneratorWorker::generateFrames);
    connect(frameGenerator_, &FrameGeneratorWorker::finished, 
            &frameGeneratorThread_, &QThread::quit, Qt::QueuedConnection);
    connect(&frameGeneratorThread_, &QThread::finished, 
            frameGenerator_, &FrameGeneratorWorker::deleteLater, Qt::QueuedConnection);
    // Use direct connection from worker to camera to avoid race conditions
    connect(frameGenerator_, &FrameGeneratorWorker::frameGenerated,
            this, &SaperaCamera::handleNewFrame, Qt::DirectConnection);
}

SaperaCamera::~SaperaCamera() {
    // Make sure we're disconnected from camera
    if (isConnected_) {
        disconnectCamera();
    }
    
    // Safely clean up thread resources
    stopFrameThread();
    
    // If thread is still running, wait for it to finish
    if (frameGeneratorThread_.isRunning()) {
        frameGeneratorThread_.quit();
        if (!frameGeneratorThread_.wait(1000)) {
            qDebug() << "Warning: Frame generator thread taking too long to quit, terminating...";
            frameGeneratorThread_.terminate();
        }
    }
    
    // Delete the frame generator if it wasn't automatically deleted
    if (frameGenerator_) {
        disconnect(frameGenerator_, nullptr, this, nullptr);
        if (frameGenerator_->parent() == nullptr) {
            delete frameGenerator_;
            frameGenerator_ = nullptr;
        }
    }
}

bool SaperaCamera::connectCamera() {
    if (isConnected_) {
        return true;
    }

    // Create Sapera objects
    try {
        qDebug() << "Connecting to camera:" << QString::fromStdString(name_);
        
        // Create acquisition device
        qDebug() << "Creating SapAcqDevice for" << QString::fromStdString(name_);
        device_ = std::make_unique<SapAcqDevice>(name_.c_str());
        if (!device_->Create()) {
            qDebug() << "Failed to create acquisition device";
            emit error("Failed to create acquisition device");
            return false;
        }
        qDebug() << "SapAcqDevice created successfully";
        
        // Create buffer
        qDebug() << "Creating SapBufferWithTrash";
        buffer_ = std::make_unique<SapBufferWithTrash>(2, device_.get());
        if (!buffer_->Create()) {
            qDebug() << "Failed to create buffer";
            emit error("Failed to create buffer");
            destroySaperaObjects();
            return false;
        }
        qDebug() << "SapBufferWithTrash created successfully";
        
        // Create view object
        qDebug() << "Creating SapView";
        view_ = std::make_unique<SapView>(buffer_.get());
        if (!view_->Create()) {
            qDebug() << "Failed to create view";
            emit error("Failed to create view");
            destroySaperaObjects();
            return false;
        }
        qDebug() << "SapView created successfully";
        
        // Get initial camera properties
        if (isFeatureAvailable("ExposureTime")) {
            double exposure = 0.0;
            if (device_->GetFeatureValue("ExposureTime", &exposure)) {
                exposureTime_ = exposure;
            }
        }
        
        // Start the frame generation thread
        startFrameThread();
        
        // Connection successful
        isConnected_ = true;
        qDebug() << "Successfully connected to camera:" << QString::fromStdString(name_);
        emit statusChanged("Connected to camera: " + name_);
        
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "Exception connecting to camera:" << e.what();
        emit error(std::string("Exception connecting to camera: ") + e.what());
        destroySaperaObjects();
        return false;
    }
    catch (...) {
        qDebug() << "Unknown exception connecting to camera";
        emit error("Unknown exception connecting to camera");
        destroySaperaObjects();
        return false;
    }
}

bool SaperaCamera::disconnectCamera() {
    if (!isConnected_) {
        return true;
    }
    
    // Stop the frame generation thread
    stopFrameThread();

    // Destroy Sapera objects
    destroySaperaObjects();

    isConnected_ = false;
    emit statusChanged("Disconnected from camera: " + name_);
    
    // Update display with disconnected message
    QImage disconnectedImage(640, 480, QImage::Format_RGB32);
    disconnectedImage.fill(Qt::black);
    
    QPainter painter(&disconnectedImage);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(disconnectedImage.rect(), Qt::AlignCenter, 
                     QString::fromStdString(name_) + "\nDisconnected");
    painter.end();
    
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_ = disconnectedImage;
    }
    
    emit newFrameAvailable(disconnectedImage);
    
    return true;
}

void SaperaCamera::startFrameThread() {
    if (!frameGeneratorThread_.isRunning()) {
        frameGeneratorThread_.start();
    }
}

void SaperaCamera::stopFrameThread() {
    if (frameGenerator_ && frameGeneratorThread_.isRunning()) {
        frameGenerator_->stop();
        frameGeneratorThread_.quit();
        frameGeneratorThread_.wait(1000);  // Wait up to 1 second
        
        if (frameGeneratorThread_.isRunning()) {
            // Force termination if not stopped normally
            frameGeneratorThread_.terminate();
            qDebug() << "Warning: Frame generator thread had to be terminated";
        }
    }
}

void SaperaCamera::handleNewFrame(const QImage& frame) {
    // Create a copy of the frame
    QImage frameCopy = frame.copy();
    
    // Update the current frame with thread safety
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_ = frameCopy;
    }
    
    // Use a queued connection to emit the signal to prevent deadlocks
    // in the event loop when connecting to slots in the UI thread
    QMetaObject::invokeMethod(this, [this, frameCopy]() {
        emit newFrameAvailable(frameCopy);
    }, Qt::QueuedConnection);
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

    if (device_) {
        // Set the exposure time in the device
        if (device_->SetFeatureValue("ExposureTime", microseconds)) {
            exposureTime_ = microseconds;
            emit statusChanged("Exposure time set to " + std::to_string(microseconds) + " microseconds");
            return true;
        }
        else {
            emit error("Failed to set exposure time");
            return false;
        }
    }
    
    // If no device (simulation mode), just store the value
    exposureTime_ = microseconds;
    emit statusChanged("Exposure time set to " + std::to_string(microseconds) + " microseconds");
    return true;
}

double SaperaCamera::getExposureTime() const {
    if (!isConnected_) {
        return exposureTime_; // Return cached value when not connected
    }

    if (device_) {
        double value = 0.0;
        if (device_->GetFeatureValue("ExposureTime", &value)) {
            return value;
        }
    }
    
    return exposureTime_;
}

QImage SaperaCamera::getFrame() const {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return currentFrame_.copy();
}

bool SaperaCamera::configureCamera() {
    if (!device_ || !isConnected_) {
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
        device_->SetFeatureValue("ExposureTime", exposureTime_);
    }

    return true;
}

void SaperaCamera::printCameraInfo() const {
    if (!device_ || !isConnected_) {
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

    // Use const_cast to safely emit the signal from a const method
    const_cast<SaperaCamera*>(this)->emit statusChanged(infoMessage);
}

bool SaperaCamera::createSaperaObjects() {
    // Objects are created in connectCamera
    return true;
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
    if (!buffer_ || !isConnected_) {
        return;
    }

    // Get the buffer information
    void* pData = nullptr;

    try {
        // Get the buffer data
        if (buffer_->GetAddress(&pData)) {
            // Get buffer properties
            UINT32 width = buffer_->GetWidth();
            UINT32 height = buffer_->GetHeight();
            UINT32 pitch = buffer_->GetPitch();
            UINT32 format = buffer_->GetFormat();

            // Create QImage from buffer data
            QImage newFrame(width, height, QImage::Format_RGB32);

            // Convert buffer data to QImage based on format
            const uint8_t* src = static_cast<const uint8_t*>(pData);
            
            // For simplicity, we'll treat everything as grayscale
            for (UINT32 y = 0; y < height; y++) {
                for (UINT32 x = 0; x < width; x++) {
                    uint8_t pixel = src[y * pitch + x];
                    newFrame.setPixelColor(x, y, QColor(pixel, pixel, pixel));
                }
            }

            // Use a queued connection to pass the new frame to the main thread safely
            QMetaObject::invokeMethod(this, [this, newFrame]() {
                // Call handleNewFrame with a deep copy to ensure thread safety
                handleNewFrame(newFrame.copy());
            }, Qt::QueuedConnection);
        }
    }
    catch (const std::exception& e) {
        qDebug() << "Error updating frame:" << e.what();
    }
    catch (...) {
        qDebug() << "Unknown exception in updateFrameFromBuffer";
    }
}

void SaperaCamera::XferCallback(SapXferCallbackInfo *pInfo) {
    SaperaCamera* camera = static_cast<SaperaCamera*>(pInfo->GetContext());
    if (camera) {
        camera->updateFrameFromBuffer();
    }
}

void SaperaCamera::printFeatureValue(const char* featureName) const {
    if (!device_ || !isConnected_) {
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
                        // Use const_cast to safely emit the signal from a const method
                        const_cast<SaperaCamera*>(this)->emit statusChanged(std::string(featureName) + ": " + std::string(valueBuffer));
                    }
                }
            }
            feature.Destroy();
        }
    }
}

bool SaperaCamera::isFeatureAvailable(const char* featureName) const {
    if (!device_ || !isConnected_) {
        return false;
    }

    BOOL isAvailable = FALSE;
    if (device_->IsFeatureAvailable(featureName, &isAvailable)) {
        return isAvailable == TRUE;
    }
    return false;
}

#elif HAS_GIGE_VISION
// GigE Vision Interface implementation

SaperaCamera::SaperaCamera(const std::string& name)
    : name_(name)
    , isConnected_(false)
    , exposureTime_(10000.0)
{
    // Create empty QImage for now
    currentFrame_ = QImage(640, 480, QImage::Format_RGB32);
    currentFrame_.fill(Qt::black);
    
    // Draw camera name on start image
    QPainter painter(&currentFrame_);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(currentFrame_.rect(), Qt::AlignCenter, 
                     QString::fromStdString(name_) + "\nNot Connected");
    painter.end();
    
    // Create the frame generator but don't start it yet
    frameGenerator_ = new FrameGeneratorWorker();
    frameGenerator_->setCamera(name_, &exposureTime_);
    frameGenerator_->moveToThread(&frameGeneratorThread_);
    
    // Connect signals and slots with proper connection types
    connect(&frameGeneratorThread_, &QThread::started, 
            frameGenerator_, &FrameGeneratorWorker::generateFrames);
    connect(frameGenerator_, &FrameGeneratorWorker::finished, 
            &frameGeneratorThread_, &QThread::quit, Qt::QueuedConnection);
    connect(&frameGeneratorThread_, &QThread::finished, 
            frameGenerator_, &FrameGeneratorWorker::deleteLater, Qt::QueuedConnection);
    // Use direct connection from worker to camera to avoid race conditions
    connect(frameGenerator_, &FrameGeneratorWorker::frameGenerated,
            this, &SaperaCamera::handleNewFrame, Qt::DirectConnection);
}

SaperaCamera::~SaperaCamera() {
    if (isConnected_) {
        disconnectCamera();
    }
    
    // Safely clean up thread resources
    stopFrameThread();
    
    // If thread is still running, wait for it to finish
    if (frameGeneratorThread_.isRunning()) {
        frameGeneratorThread_.quit();
        if (!frameGeneratorThread_.wait(1000)) {
            qDebug() << "Warning: Frame generator thread taking too long to quit, terminating...";
            frameGeneratorThread_.terminate();
        }
    }
    
    // Delete the frame generator if it wasn't automatically deleted
    if (frameGenerator_) {
        disconnect(frameGenerator_, nullptr, this, nullptr);
        if (frameGenerator_->parent() == nullptr) {
            delete frameGenerator_;
            frameGenerator_ = nullptr;
        }
    }
}

bool SaperaCamera::connectCamera() {
    if (isConnected_) {
        return true;
    }

    #if HAS_GIGE_VISION_HEADERS
    // Attempt to connect using GigE Vision Interface
    try {
        // TODO: Implement GigE Vision camera connection
        // This is a temporary implementation that simulates connection
        
        // Start the frame generation thread
        startFrameThread();
        
        isConnected_ = true;
        emit statusChanged("Camera connected via GigE Vision: " + name_);
        
        return true;
    } catch (const std::exception& e) {
        emit error(std::string("Failed to connect to camera: ") + e.what());
        return false;
    }
    #else
    // No GigE Vision headers available, simulate connection
    
    // Start the frame generation thread
    startFrameThread();
    
    isConnected_ = true;
    emit statusChanged("Camera connected (simulation): " + name_);
    
    return true;
    #endif
}

bool SaperaCamera::disconnectCamera() {
    if (!isConnected_) {
        return true;
    }

    // Stop the frame thread
    stopFrameThread();

    #if HAS_GIGE_VISION_HEADERS
    // TODO: Implement actual GigE Vision camera disconnection
    #endif

    isConnected_ = false;
    emit statusChanged("Camera disconnected: " + name_);
    
    // Update display with disconnected message
    QImage disconnectedImage(640, 480, QImage::Format_RGB32);
    disconnectedImage.fill(Qt::black);
    
    QPainter painter(&disconnectedImage);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(disconnectedImage.rect(), Qt::AlignCenter, 
                    QString::fromStdString(name_) + "\nDisconnected");
    painter.end();
    
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_ = disconnectedImage;
    }
    
    emit newFrameAvailable(disconnectedImage);
    
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

    #if HAS_GIGE_VISION_HEADERS
    // TODO: Implement actual GigE Vision exposure time setting
    #endif

    exposureTime_ = microseconds;
    emit statusChanged("Exposure time set to " + std::to_string(microseconds) + " microseconds");
    return true;
}

double SaperaCamera::getExposureTime() const {
    return exposureTime_;
}

QImage SaperaCamera::getFrame() const {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return currentFrame_.copy();
}

void SaperaCamera::startFrameThread() {
    if (!frameGeneratorThread_.isRunning()) {
        frameGeneratorThread_.start();
    }
}

void SaperaCamera::stopFrameThread() {
    if (frameGenerator_ && frameGeneratorThread_.isRunning()) {
        frameGenerator_->stop();
        frameGeneratorThread_.quit();
        frameGeneratorThread_.wait(1000);  // Wait up to 1 second
        
        if (frameGeneratorThread_.isRunning()) {
            // Force termination if not stopped normally
            frameGeneratorThread_.terminate();
            qDebug() << "Warning: Frame generator thread had to be terminated";
        }
    }
}

void SaperaCamera::handleNewFrame(const QImage& frame) {
    // Create a copy of the frame
    QImage frameCopy = frame.copy();
    
    // Update the current frame with thread safety
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_ = frameCopy;
    }
    
    // Use a queued connection to emit the signal to prevent deadlocks
    // in the event loop when connecting to slots in the UI thread
    QMetaObject::invokeMethod(this, [this, frameCopy]() {
        emit newFrameAvailable(frameCopy);
    }, Qt::QueuedConnection);
}

#else
// Stub implementation when no camera SDK is available

SaperaCamera::SaperaCamera(const std::string& name)
    : name_(name)
    , isConnected_(false)
    , exposureTime_(10000.0) // 10ms default
{
    // Create empty QImage for now
    currentFrame_ = QImage(640, 480, QImage::Format_RGB32);
    currentFrame_.fill(Qt::black);
    
    // Draw text indicating no SDK
    QPainter painter(&currentFrame_);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 20));
    painter.drawText(currentFrame_.rect(), Qt::AlignCenter, "No Camera SDK Available");
    painter.end();
    
    // Create the frame generator but don't start it yet
    frameGenerator_ = new FrameGeneratorWorker();
    frameGenerator_->setCamera(name_, &exposureTime_);
    frameGenerator_->moveToThread(&frameGeneratorThread_);
    
    // Connect signals and slots with proper connection types
    connect(&frameGeneratorThread_, &QThread::started, 
            frameGenerator_, &FrameGeneratorWorker::generateFrames);
    connect(frameGenerator_, &FrameGeneratorWorker::finished, 
            &frameGeneratorThread_, &QThread::quit, Qt::QueuedConnection);
    connect(&frameGeneratorThread_, &QThread::finished, 
            frameGenerator_, &FrameGeneratorWorker::deleteLater, Qt::QueuedConnection);
    // Use direct connection from worker to camera to avoid race conditions
    connect(frameGenerator_, &FrameGeneratorWorker::frameGenerated,
            this, &SaperaCamera::handleNewFrame, Qt::DirectConnection);
}

SaperaCamera::~SaperaCamera() {
    if (isConnected_) {
        disconnectCamera();
    }
    
    // Safely clean up thread resources
    stopFrameThread();
    
    // If thread is still running, wait for it to finish
    if (frameGeneratorThread_.isRunning()) {
        frameGeneratorThread_.quit();
        if (!frameGeneratorThread_.wait(1000)) {
            qDebug() << "Warning: Frame generator thread taking too long to quit, terminating...";
            frameGeneratorThread_.terminate();
        }
    }
    
    // Delete the frame generator if it wasn't automatically deleted
    if (frameGenerator_) {
        disconnect(frameGenerator_, nullptr, this, nullptr);
        if (frameGenerator_->parent() == nullptr) {
            delete frameGenerator_;
            frameGenerator_ = nullptr;
        }
    }
}

bool SaperaCamera::connectCamera() {
    if (isConnected_) {
        return true;
    }

    // Start the frame generation
    startFrameThread();
    
    // Simulate connection
    isConnected_ = true;
    emit statusChanged("Camera connected (simulation): " + name_);
    
    return true;
}

bool SaperaCamera::disconnectCamera() {
    if (!isConnected_) {
        return true;
    }

    // Stop the frame thread
    stopFrameThread();

    isConnected_ = false;
    emit statusChanged("Camera disconnected: " + name_);
    
    // Update display with disconnected message
    QImage disconnectedImage(640, 480, QImage::Format_RGB32);
    disconnectedImage.fill(Qt::black);
    
    QPainter painter(&disconnectedImage);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 20, QFont::Bold));
    painter.drawText(disconnectedImage.rect(), Qt::AlignCenter, 
                    QString::fromStdString(name_) + "\nDisconnected");
    painter.end();
    
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_ = disconnectedImage;
    }
    
    emit newFrameAvailable(disconnectedImage);
    
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

    exposureTime_ = microseconds;
    emit statusChanged("Exposure time set to " + std::to_string(microseconds) + " microseconds");
    return true;
}

double SaperaCamera::getExposureTime() const {
    return exposureTime_;
}

QImage SaperaCamera::getFrame() const {
    std::lock_guard<std::mutex> lock(frameMutex_);
    return currentFrame_.copy();
}

void SaperaCamera::startFrameThread() {
    if (!frameGeneratorThread_.isRunning()) {
        frameGeneratorThread_.start();
    }
}

void SaperaCamera::stopFrameThread() {
    if (frameGenerator_ && frameGeneratorThread_.isRunning()) {
        frameGenerator_->stop();
        frameGeneratorThread_.quit();
        frameGeneratorThread_.wait(1000);  // Wait up to 1 second
        
        if (frameGeneratorThread_.isRunning()) {
            // Force termination if not stopped normally
            frameGeneratorThread_.terminate();
            qDebug() << "Warning: Frame generator thread had to be terminated";
        }
    }
}

void SaperaCamera::handleNewFrame(const QImage& frame) {
    // Create a copy of the frame
    QImage frameCopy = frame.copy();
    
    // Update the current frame with thread safety
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_ = frameCopy;
    }
    
    // Use a queued connection to emit the signal to prevent deadlocks
    // in the event loop when connecting to slots in the UI thread
    QMetaObject::invokeMethod(this, [this, frameCopy]() {
        emit newFrameAvailable(frameCopy);
    }, Qt::QueuedConnection);
}

#endif

} // namespace cam_matrix::core


