#include "sapera_camera.hpp"
#include <QDebug>
#include <QColor>
#include <QString>
#include <QRandomGenerator>
#include <QDateTime>
#include <QPainter>
#include <QFont>
#include <QMetaObject>
#include <QImage>
#include <QThread>
#include <QFileInfo>
#include <QDir>
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
    
    // Draw a colorful border to make it obvious the frame is being updated
    painter.setPen(QPen(QColor(255, 0, 0), 4));  // Red border
    painter.drawRect(2, 2, newFrame.width() - 4, newFrame.height() - 4);
    
    // Draw a moving indicator to show that frames are changing
    int indicatorX = 20 + (timestamp / 100 % (newFrame.width() - 40));
    painter.setBrush(QColor(0, 255, 0));  // Green indicator
    painter.drawEllipse(QPoint(indicatorX, 20), 10, 10);
    
    // Add text overlays
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 16, QFont::Bold));
    painter.drawText(10, 50, QString::fromStdString("Camera: " + cameraName_));
    
    painter.setFont(QFont("Arial", 14));
    painter.drawText(10, 80, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    painter.drawText(10, 110, QString("Exposure: %1 μs").arg(exposureValue));
    
    // Add frame counter
    static int frameCounter = 0;
    frameCounter++;
    painter.drawText(10, 140, QString("Frame: %1").arg(frameCounter));
    
    qDebug() << "Generated frame" << frameCounter << "for camera" << QString::fromStdString(cameraName_);
    
    return newFrame;
}

#if HAS_SAPERA

SaperaCamera::SaperaCamera(const std::string& name)
    : name_(name)
    , isConnected_(false)
{
    qDebug() << "Creating SaperaCamera instance for" << QString::fromStdString(name);
    // Frame generator worker is created for both real and mock implementations
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
    
    qDebug() << "SaperaCamera instance created successfully, thread started";
}

SaperaCamera::~SaperaCamera()
{
    qDebug() << "Destroying SaperaCamera instance for" << QString::fromStdString(name_);
    disconnectCamera();
    
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
    
    qDebug() << "SaperaCamera instance destroyed";
}

bool SaperaCamera::connectCamera()
{
    qDebug() << "Attempting to connect to camera:" << QString::fromStdString(name_);
    
    if (isConnected_)
    {
        qDebug() << "Camera already connected";
        return true;
    }
    
    std::lock_guard<std::mutex> lock(frameMutex_);
    
    qDebug() << "Using real Sapera SDK implementation";
    if (!createSaperaObjects())
    {
        emit error("Failed to create Sapera objects");
        return false;
    }

    isConnected_ = true;
    emit statusChanged("Connected to camera: " + name_);
    
    // Start generating frames
    qDebug() << "Starting frame generation";
    startFrameThread();
    
    return true;
}

bool SaperaCamera::disconnectCamera()
{
    qDebug() << "Attempting to disconnect camera:" << QString::fromStdString(name_);
    if (!isConnected_)
    {
        qDebug() << "Camera not connected";
        return true;
    }
    
    std::lock_guard<std::mutex> lock(frameMutex_);
    
    // Stop frame generation
    qDebug() << "Stopping frame generation";
    stopFrameThread();
    
    destroySaperaObjects();
    
    isConnected_ = false;
    emit statusChanged("Disconnected from camera: " + name_);
    
    return true;
}

void SaperaCamera::handleNewFrame(const QImage& frame)
{
    qDebug() << "Frame ready from camera" << QString::fromStdString(name_) << "- size:" << frame.size();
    
    // Create a copy of the frame
    QImage frameCopy = frame.copy();
    
    // Update the current frame with thread safety
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_ = frameCopy;
    }
    
    // Use a queued connection to emit the signal to prevent deadlocks
    QMetaObject::invokeMethod(this, [this, frameCopy]() {
        emit newFrameAvailable(frameCopy);
    }, Qt::QueuedConnection);
}

bool SaperaCamera::createSaperaObjects()
{
    qDebug() << "Creating Sapera objects for camera:" << QString::fromStdString(name_);
    
    try {
        // Create a Sapera acquisition device
        device_ = std::make_unique<SapAcqDevice>(name_.c_str());
        if (!device_->Create()) {
            qDebug() << "Failed to create SapAcqDevice";
            destroySaperaObjects();
            return false;
        }
        
        // Create a buffer with trash for image storage - using dummy values for now
        const int bufferCount = 2; // Number of buffers to use
        buffer_ = std::make_unique<SapBufferWithTrash>(bufferCount, device_.get());
        if (!buffer_->Create()) {
            qDebug() << "Failed to create SapBufferWithTrash";
            destroySaperaObjects();
            return false;
        }
        
        // Create a transfer object from device to buffer
        transfer_ = std::make_unique<SapAcqDeviceToBuf>(device_.get(), buffer_.get());
        if (!transfer_->Create()) {
            qDebug() << "Failed to create SapAcqDeviceToBuf";
            destroySaperaObjects();
            return false;
        }
        
        // Set the transfer callback
        transfer_->SetCallbackInfo(XferCallback, this);
        
        // Create a view for displaying the image - null handle means no windows will be created
        view_ = std::make_unique<SapView>(buffer_.get(), nullptr);
        if (!view_->Create()) {
            qDebug() << "Failed to create SapView";
            destroySaperaObjects();
            return false;
        }
        
        qDebug() << "Sapera objects created successfully";
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "Exception creating Sapera objects:" << e.what();
        destroySaperaObjects();
        return false;
    }
}

void SaperaCamera::destroySaperaObjects()
{
    qDebug() << "Destroying Sapera objects";
    
    // Destroy objects in reverse order of creation
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
    
    qDebug() << "Sapera objects destroyed";
}

void SaperaCamera::startFrameThread()
{
    if (!frameGeneratorThread_.isRunning()) {
        frameGeneratorThread_.start();
    }
}

void SaperaCamera::stopFrameThread()
{
    if (frameGenerator_ && frameGeneratorThread_.isRunning()) {
        frameGenerator_->stop();
        frameGeneratorThread_.quit();
        frameGeneratorThread_.wait(500);  // Wait up to 500ms
    }
}

void SaperaCamera::XferCallback(SapXferCallbackInfo* pInfo)
{
    // Get the camera object from the context
    SaperaCamera* camera = static_cast<SaperaCamera*>(pInfo->GetContext());
    if (camera) {
        camera->updateFrameFromBuffer();
    }
}

void SaperaCamera::updateFrameFromBuffer()
{
    if (!buffer_) {
        return;
    }
    
    try {
        // Get the image data from the buffer
        // This is just a placeholder - in a real implementation, you would 
        // convert the Sapera buffer to a QImage
        QImage frame(640, 480, QImage::Format_RGB32);
        frame.fill(Qt::blue);  // Fill with blue as a placeholder
        
        // Add a timestamp
        QPainter painter(&frame);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 14));
        painter.drawText(10, 30, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
        painter.drawText(10, 50, QString::fromStdString("Camera: " + name_));
        painter.end();
        
        // Update the current frame
        {
            std::lock_guard<std::mutex> lock(frameMutex_);
            currentFrame_ = frame.copy();
        }
        
        // Emit the frame available signal
        emit newFrameAvailable(frame);
    }
    catch (const std::exception& e) {
        qDebug() << "Exception in updateFrameFromBuffer:" << e.what();
    }
}

bool SaperaCamera::configureCamera()
{
    if (!device_ || !isConnected_) {
        return false;
    }
    
    try {
        // Configure camera settings
        // This is just a placeholder - in a real implementation, you would
        // set various camera parameters
        
        // Print some camera info
        printCameraInfo();
        
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "Exception configuring camera:" << e.what();
        return false;
    }
}

void SaperaCamera::printCameraInfo() const
{
    if (!device_ || !isConnected_) {
        return;
    }
    
    qDebug() << "Camera Info for" << QString::fromStdString(name_);
    qDebug() << "----------------";
    
    // Print various camera features
    printFeatureValue("DeviceVendorName");
    printFeatureValue("DeviceModelName");
    printFeatureValue("DeviceVersion");
    printFeatureValue("DeviceID");
    
    // Print exposure time
    qDebug() << "Exposure Time:" << getExposureTime() << "microseconds";
    
    qDebug() << "----------------";
}

void SaperaCamera::printFeatureValue(const char* featureName) const
{
    if (!device_ || !isFeatureAvailable(featureName)) {
        return;
    }
    
    try {
        // This is a placeholder - in a real implementation, you would
        // get the actual feature value from the camera
        qDebug() << featureName << ": [Feature value would be shown here]";
    }
    catch (const std::exception& e) {
        qDebug() << "Error getting feature" << featureName << ":" << e.what();
    }
}

bool SaperaCamera::isFeatureAvailable(const char* featureName) const
{
    if (!device_) {
        return false;
    }
    
    // This is a placeholder - in a real implementation, you would
    // check if the feature exists on the camera
    return true;
}

bool SaperaCamera::capturePhoto(const std::string& savePath)
{
    if (!isConnected_) {
        emit error("Cannot capture photo: Camera not connected");
        return false;
    }

    // Get the current frame
    QImage capturedFrame;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        capturedFrame = currentFrame_.copy(); // Make a copy to avoid threading issues
    }

    if (capturedFrame.isNull()) {
        emit error("Failed to capture photo: No valid frame available");
        return false;
    }

    // Generate a filename if not provided
    std::string finalPath = savePath;
    if (finalPath.empty()) {
        // Create a timestamp-based filename in the current directory
        QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
        finalPath = name_ + "_" + timeStamp.toStdString() + ".png";
    }

    // Save the image to file
    bool saved = saveImageToFile(capturedFrame, finalPath);
    if (saved) {
        emit statusChanged("Photo captured and saved to: " + finalPath);
        emit photoCaptured(capturedFrame, finalPath);
    } else {
        emit error("Failed to save photo to: " + finalPath);
    }

    return saved;
}

bool SaperaCamera::saveImageToFile(const QImage& image, const std::string& filePath)
{
    if (image.isNull()) {
        return false;
    }

    // Ensure the directory exists
    QFileInfo fileInfo(QString::fromStdString(filePath));
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create directory for saving image:" << dir.path();
            return false;
        }
    }

    // Save the image to file
    bool success = image.save(QString::fromStdString(filePath));
    qDebug() << "Saved image to" << QString::fromStdString(filePath) << ":" << (success ? "Success" : "Failed");
    return success;
}

std::string SaperaCamera::getName() const
{
    return name_;
}

bool SaperaCamera::isConnected() const
{
    return isConnected_;
}

bool SaperaCamera::setExposureTime(double microseconds)
{
    if (!isConnected_ || !device_) {
        return false;
    }
    
    try {
        // Set the exposure time
        // This is a placeholder - in a real implementation, you would
        // set the actual exposure time on the camera
        exposureTime_ = microseconds;
        emit statusChanged("Exposure time set to " + std::to_string(microseconds) + " microseconds");
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "Error setting exposure time:" << e.what();
        return false;
    }
}

double SaperaCamera::getExposureTime() const
{
    return exposureTime_;
}

QImage SaperaCamera::getFrame() const
{
    std::lock_guard<std::mutex> lock(frameMutex_);
    return currentFrame_.copy();
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

bool SaperaCamera::capturePhoto(const std::string& savePath)
{
    if (!isConnected_) {
        emit error("Cannot capture photo: Camera not connected");
        return false;
    }

    // Get the current frame
    QImage capturedFrame;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        capturedFrame = currentFrame_.copy(); // Make a copy to avoid threading issues
    }

    if (capturedFrame.isNull()) {
        emit error("Failed to capture photo: No valid frame available");
        return false;
    }

    // Generate a filename if not provided
    std::string finalPath = savePath;
    if (finalPath.empty()) {
        // Create a timestamp-based filename in the current directory
        QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
        finalPath = name_ + "_" + timeStamp.toStdString() + ".png";
    }

    // Save the image to file
    bool saved = saveImageToFile(capturedFrame, finalPath);
    if (saved) {
        emit statusChanged("Photo captured and saved to: " + finalPath);
        emit photoCaptured(capturedFrame, finalPath);
    } else {
        emit error("Failed to save photo to: " + finalPath);
    }

    return saved;
}

bool SaperaCamera::saveImageToFile(const QImage& image, const std::string& filePath)
{
    if (image.isNull()) {
        return false;
    }

    // Ensure the directory exists
    QFileInfo fileInfo(QString::fromStdString(filePath));
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create directory for saving image:" << dir.path();
            return false;
        }
    }

    // Save the image to file
    bool success = image.save(QString::fromStdString(filePath));
    qDebug() << "Saved image to" << QString::fromStdString(filePath) << ":" << (success ? "Success" : "Failed");
    return success;
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

bool SaperaCamera::capturePhoto(const std::string& savePath)
{
    if (!isConnected_) {
        emit error("Cannot capture photo: Camera not connected");
        return false;
    }

    // Get the current frame
    QImage capturedFrame;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        capturedFrame = currentFrame_.copy(); // Make a copy to avoid threading issues
    }

    if (capturedFrame.isNull()) {
        emit error("Failed to capture photo: No valid frame available");
        return false;
    }

    // Generate a filename if not provided
    std::string finalPath = savePath;
    if (finalPath.empty()) {
        // Create a timestamp-based filename in the current directory
        QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
        finalPath = name_ + "_" + timeStamp.toStdString() + ".png";
    }

    // Save the image to file
    bool saved = saveImageToFile(capturedFrame, finalPath);
    if (saved) {
        emit statusChanged("Photo captured and saved to: " + finalPath);
        emit photoCaptured(capturedFrame, finalPath);
    } else {
        emit error("Failed to save photo to: " + finalPath);
    }

    return saved;
}

bool SaperaCamera::saveImageToFile(const QImage& image, const std::string& filePath)
{
    if (image.isNull()) {
        return false;
    }

    // Ensure the directory exists
    QFileInfo fileInfo(QString::fromStdString(filePath));
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Failed to create directory for saving image:" << dir.path();
            return false;
        }
    }

    // Save the image to file
    bool success = image.save(QString::fromStdString(filePath));
    qDebug() << "Saved image to" << QString::fromStdString(filePath) << ":" << (success ? "Success" : "Failed");
    return success;
}

#endif

} // namespace cam_matrix::core


