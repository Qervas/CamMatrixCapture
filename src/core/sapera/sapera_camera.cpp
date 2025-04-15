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
#include <QTimer>

namespace cam_matrix::core::sapera {

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

// SaperaCamera implementation
SaperaCamera::SaperaCamera(const std::string& name)
    : name_(name)
{
    qDebug() << "Creating SaperaCamera instance for" << QString::fromStdString(name);
    
    // Create an empty initial frame
    currentFrame_ = QImage(640, 480, QImage::Format_RGB32);
    currentFrame_.fill(Qt::black);
    
    // Draw initialization text on the frame
    QPainter painter(&currentFrame_);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 16, QFont::Bold));
    painter.drawText(currentFrame_.rect(), Qt::AlignCenter, 
                   QString::fromStdString(name) + "\nNot Connected");
    painter.end();
    
    // Create the camera thread
    createCameraThread();
    
    qDebug() << "SaperaCamera instance created successfully";
}

SaperaCamera::~SaperaCamera()
{
    qDebug() << "Destroying SaperaCamera instance for" << QString::fromStdString(name_);
    
    // Disconnect if still connected
    if (isConnected()) {
        disconnectCamera();
    }
    
    // Clean up the camera thread
    if (cameraThread_) {
        cameraThread_->stop();
        cameraThread_.reset();
    }
    
    qDebug() << "SaperaCamera instance destroyed";
}

void SaperaCamera::createCameraThread()
{
    // Create the camera thread
    cameraThread_ = std::make_unique<CameraThread>();
    
    // Connect signals from the thread
    connect(cameraThread_.get(), &CameraThread::frameReady,
            this, &SaperaCamera::handleNewFrame, Qt::QueuedConnection);
    
    connect(cameraThread_.get(), &CameraThread::errorOccurred,
            this, &SaperaCamera::handleThreadError, Qt::QueuedConnection);
    
    connect(cameraThread_.get(), &CameraThread::operationCompleted,
            this, [this](CameraOpType type, bool success) {
                QString opName;
                switch (type) {
                    case CameraOpType::Connect: opName = "Connect"; break;
                    case CameraOpType::Disconnect: opName = "Disconnect"; break;
                    case CameraOpType::CapturePhoto: opName = "CapturePhoto"; break;
                    case CameraOpType::GetFrame: opName = "GetFrame"; break;
                    case CameraOpType::SetExposure: opName = "SetExposure"; break;
                    case CameraOpType::Custom: opName = "Custom"; break;
                }
                
                qDebug() << "Camera operation completed:" << opName << "Success:" << success;
                emit operationCompleted(opName.toStdString(), success);
            }, Qt::QueuedConnection);
}

// Synchronous operations - these will queue to the thread but block until complete
bool SaperaCamera::connectCamera()
{
    if (isConnected()) {
        return true; // Already connected
    }
    
    bool success = false;
    std::promise<bool> resultPromise;
    std::future<bool> resultFuture = resultPromise.get_future();
    
    // Queue connect operation
    cameraThread_->queueOperation(CameraOpType::Connect, 
        [this]() {
            // This runs in the camera thread
            
        #ifdef HAS_SAPERA
            // Create Sapera objects
            bool objectsCreated = createSaperaObjects();
            if (!objectsCreated) {
                qDebug() << "Failed to create Sapera objects";
                return;
            }
            
            // Start frame acquisition
            startFrameAcquisition();
        #else
            // Start the frame thread for mock implementation
            startFrameThread();
        #endif
            
            // Set state to connected
            isConnected_ = true;
        },
        [&resultPromise](bool success) {
            // Callback - this also runs in the camera thread
            resultPromise.set_value(success);
        },
        "ConnectCamera");
    
    // Wait for the operation to complete
    try {
        success = resultFuture.get();
    }
    catch (const std::exception& e) {
        qDebug() << "Exception while connecting camera:" << e.what();
        success = false;
    }
    
    // Emit operation completed event (use queued connection to avoid blocking UI)
    QMetaObject::invokeMethod(this, [this, success]() {
        emit operationCompleted("ConnectCamera", success);
        if (success) {
            emit statusChanged("Connected to camera: " + name_);
        }
    }, Qt::QueuedConnection);
    
    return success && isConnected_;
}

bool SaperaCamera::disconnectCamera()
{
    if (!isConnected()) {
        return false; // Already disconnected
    }

    bool success = false;
    std::promise<bool> resultPromise;
    std::future<bool> resultFuture = resultPromise.get_future();
    
    // Disconnect all signal connections before shutting down to prevent deadlocks
    disconnect(this, &SaperaCamera::newFrameAvailable, nullptr, nullptr);
    
    // Queue disconnect operation
    cameraThread_->queueOperation(CameraOpType::Disconnect, 
        [this]() {
            // This runs in the camera thread
            
        #ifdef HAS_SAPERA
            stopFrameAcquisition();
            destroySaperaObjects();
        #else
            stopFrameThread();
        #endif
        
            // Create a disconnected image to show
            QImage disconnectedImage(640, 480, QImage::Format_RGB32);
            disconnectedImage.fill(Qt::black);
            
            QPainter painter(&disconnectedImage);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 14));
            painter.drawText(disconnectedImage.rect(), Qt::AlignCenter, 
                            "Camera Disconnected");
            painter.end();
            
            // Update the frame with a safe lock
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                currentFrame_ = disconnectedImage;
            }
            
            // Signal that the camera is disconnected but don't use blocking connections
            QMetaObject::invokeMethod(this, [this, disconnectedImage]() {
                emit newFrameAvailable(disconnectedImage);
                emit statusChanged("Camera disconnected: " + name_);
            }, Qt::QueuedConnection);
            
            // Set state to disconnected
            isConnected_ = false;
        },
        [&resultPromise](bool success) {
            // Callback - this also runs in the camera thread
            resultPromise.set_value(success);
        },
        "DisconnectCamera");
    
    // Wait for the operation to complete
    success = resultFuture.get();
    
    emit operationCompleted("DisconnectCamera", success);
    
    return success;
}

bool SaperaCamera::capturePhoto(const std::string& savePath)
{
    if (!isConnected()) {
        return false;
    }

    bool success = false;
    std::promise<bool> resultPromise;
    std::future<bool> resultFuture = resultPromise.get_future();
    
    // Generate path if not provided
    std::string finalPath = savePath;
    if (finalPath.empty()) {
        // Create a default path with timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t_now);
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "capture_%Y%m%d_%H%M%S", &tm);
        finalPath = std::string(buffer) + ".png";
    }
    
    // Queue capture operation
    cameraThread_->queueOperation(CameraOpType::CapturePhoto, 
        [this, finalPath]() {
            // This runs in the camera thread
            
        #ifdef HAS_SAPERA
            // Try to get the current frame from Sapera
            QImage capturedFrame;
            
            // Update the frame buffer
            updateFrameFromBuffer();
            
            // Get a copy of the current frame
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                capturedFrame = currentFrame_.copy();
            }
            
            // Save the frame to file
            bool saved = saveImageToFile(capturedFrame, finalPath);
            
            if (saved) {
                // Emit the signal using queued connection to avoid deadlocks
                QMetaObject::invokeMethod(this, [this, capturedFrame, finalPath]() {
                    emit photoCaptured(capturedFrame, finalPath);
                }, Qt::QueuedConnection);
            }
            
            return saved;
        #else
            // In mock mode, just use the current generated frame
            QImage capturedFrame;
            
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                capturedFrame = currentFrame_.copy();
            }
            
            // Save the frame to file
            bool saved = saveImageToFile(capturedFrame, finalPath);
            
            if (saved) {
                // Emit signal using queued connection to avoid deadlocks
                QMetaObject::invokeMethod(this, [this, capturedFrame, finalPath]() {
                    emit photoCaptured(capturedFrame, finalPath);
                }, Qt::QueuedConnection);
            }
            
            return saved;
        #endif
        },
        [&resultPromise](bool success) {
            // Callback - this also runs in the camera thread
            resultPromise.set_value(success);
        },
        "CapturePhoto");
    
    // Wait for the operation to complete
    try {
        success = resultFuture.get();
    }
    catch (const std::exception& e) {
        qDebug() << "Exception while capturing photo:" << e.what();
        success = false;
    }
    
    return success;
}

bool SaperaCamera::setExposureTime(double microseconds)
{
    if (!isConnected()) {
        emit error("Cannot set exposure time: Camera not connected");
        return false;
    }
    
    bool success = false;
    std::promise<bool> resultPromise;
    std::future<bool> resultFuture = resultPromise.get_future();
    
    cameraThread_->queueOperation(CameraOpType::SetExposure, 
        [this, microseconds]() {
            // This runs in the camera thread
            
        #ifdef HAS_SAPERA
            // Set the exposure time in the Sapera camera
            // This is a placeholder - in a real implementation, you would
            // set the actual exposure time on the camera
        #endif
            
            // Store the new exposure time
            exposureTime_ = microseconds;
            
            QMetaObject::invokeMethod(this, [this, microseconds]() {
                emit statusChanged("Exposure time set to " + std::to_string(microseconds) + " microseconds");
            }, Qt::QueuedConnection);
        },
        [&resultPromise](bool success) {
            // Callback - this also runs in the camera thread
            resultPromise.set_value(success);
        },
        "SetExposureTime");
    
    // Wait for the operation to complete
    success = resultFuture.get();
    
    return success;
}

// Asynchronous operations - these will queue to the thread but return immediately
void SaperaCamera::connectCameraAsync(const std::function<void(bool)>& callback)
{
    if (isConnected()) {
        if (callback) {
            callback(true);
        }
        return;
    }
    
    cameraThread_->queueOperation(CameraOpType::Connect, 
        [this]() {
            // This runs in the camera thread
            
        #ifdef HAS_SAPERA
            qDebug() << "Using real Sapera SDK implementation";
            if (!createSaperaObjects()) {
                QMetaObject::invokeMethod(this, [this]() {
                    emit error("Failed to create Sapera objects");
                }, Qt::QueuedConnection);
                return;
            }
            
            // Start frame acquisition
            startFrameAcquisition();
        #else
            // Start the frame generation
            startFrameThread();
        #endif
            
            isConnected_ = true;
            
            QMetaObject::invokeMethod(this, [this]() {
                emit statusChanged("Connected to camera: " + name_);
            }, Qt::QueuedConnection);
        },
        callback,
        "ConnectCameraAsync");
}

void SaperaCamera::disconnectCameraAsync(const std::function<void(bool)>& callback)
{
    if (!isConnected()) {
        if (callback) {
            callback(true);
        }
        return;
    }
    
    cameraThread_->queueOperation(CameraOpType::Disconnect, 
        [this]() {
            // This runs in the camera thread
            
        #ifdef HAS_SAPERA
            // Stop frame acquisition
            stopFrameAcquisition();
            // Destroy Sapera objects
            destroySaperaObjects();
        #else
            // Stop the frame thread
            stopFrameThread();
        #endif
            
            isConnected_ = false;
            
            // Create a "disconnected" frame
            QImage disconnectedImage(640, 480, QImage::Format_RGB32);
            disconnectedImage.fill(Qt::black);
            
            QPainter painter(&disconnectedImage);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 20, QFont::Bold));
            painter.drawText(disconnectedImage.rect(), Qt::AlignCenter, 
                           QString::fromStdString(name_) + "\nDisconnected");
            painter.end();
            
            // Update the current frame
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                currentFrame_ = disconnectedImage;
            }
            
            QMetaObject::invokeMethod(this, [this, disconnectedImage]() {
                emit statusChanged("Disconnected from camera: " + name_);
                emit newFrameAvailable(disconnectedImage);
            }, Qt::QueuedConnection);
        },
        callback,
        "DisconnectCameraAsync");
}

void SaperaCamera::capturePhotoAsync(const std::string& savePath, const std::function<void(bool)>& callback)
{
    if (!isConnected()) {
        emit error("Cannot capture photo: Camera not connected");
        if (callback) {
            callback(false);
        }
        return;
    }
    
    cameraThread_->queueOperation(CameraOpType::CapturePhoto, 
        [this, savePath]() {
            // This runs in the camera thread
            
            // Get the current frame
            QImage capturedFrame;
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                capturedFrame = currentFrame_.copy();
            }

            if (capturedFrame.isNull()) {
                QMetaObject::invokeMethod(this, [this]() {
                    emit error("Failed to capture photo: No valid frame available");
                }, Qt::QueuedConnection);
                return;
            }

            // Generate a filename if not provided
            std::string finalPath = savePath;
            if (finalPath.empty()) {
                // Create a timestamp-based filename in the captures directory
                QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss-zzz");
                finalPath = "captures/" + name_ + "_" + timeStamp.toStdString() + ".png";
                
                // Ensure the capture directory exists
                QDir captureDir("captures");
                if (!captureDir.exists()) {
                    if (!captureDir.mkpath(".")) {
                        QMetaObject::invokeMethod(this, [this]() {
                            emit error("Failed to create 'captures' directory");
                        }, Qt::QueuedConnection);
                        return;
                    }
                }
            }

            // Save the image to file
            bool saved = saveImageToFile(capturedFrame, finalPath);
            if (saved) {
                QMetaObject::invokeMethod(this, [this, capturedFrame, finalPath]() {
                    emit statusChanged("Photo captured and saved to: " + finalPath);
                    emit photoCaptured(capturedFrame, finalPath);
                }, Qt::QueuedConnection);
            } else {
                QMetaObject::invokeMethod(this, [this, finalPath]() {
                    emit error("Failed to save photo to: " + finalPath);
                }, Qt::QueuedConnection);
            }
        },
        callback,
        "CapturePhotoAsync");
}

void SaperaCamera::getFrameAsync(const std::function<void(QImage)>& callback)
{
    if (!callback) {
        return;
    }
    
    cameraThread_->queueOperation(CameraOpType::GetFrame, 
        [this, callback]() {
            // This runs in the camera thread
            
            // Get a copy of the current frame
            QImage frameCopy;
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                frameCopy = currentFrame_.copy();
            }
            
            // Call the callback with the frame
            callback(frameCopy);
        },
        nullptr,
        "GetFrameAsync");
}

// Property getters
bool SaperaCamera::isConnected() const
{
    return isConnected_.load();
}

std::string SaperaCamera::getName() const
{
    return name_;
}

double SaperaCamera::getExposureTime() const
{
    return exposureTime_;
}

QImage SaperaCamera::getFrame() const
{
    // Create a copy of the current frame in a non-blocking way
    QImage result;
    
    // Try to lock the mutex, but don't block if it's unavailable
    if (frameMutex_.try_lock()) {
        // Make a copy of the frame while holding the lock
        result = currentFrame_.copy();
        frameMutex_.unlock();
    } else {
        // If we couldn't get the lock, return an empty image rather than blocking
        qDebug() << "Could not acquire frame lock in getFrame() - returning empty image";
        result = QImage(640, 480, QImage::Format_RGB32);
        result.fill(Qt::black);
        
        // Add text to indicate that the frame is unavailable
        QPainter painter(&result);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 14));
        painter.drawText(result.rect(), Qt::AlignCenter, "Frame Unavailable - Try Again");
        painter.end();
    }
    
    return result;
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

void SaperaCamera::handleNewFrame(const QImage& frame)
{
    // Create a copy of the frame without holding the mutex lock for too long
    QImage frameCopy = frame.copy();
    
    // Update the current frame with a brief lock
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        currentFrame_ = frameCopy;
    }
    
    // Emit the signal in a non-blocking way using a queued connection
    // This prevents deadlocks when the main thread is busy or shutting down
    QMetaObject::invokeMethod(this, [this, frameCopy]() {
        emit newFrameAvailable(frameCopy);
    }, Qt::QueuedConnection);
}

void SaperaCamera::handleThreadError(const QString& errorMessage)
{
    emit error(errorMessage.toStdString());
}

bool SaperaCamera::configureCamera()
{
    bool success = false;
    std::promise<bool> resultPromise;
    std::future<bool> resultFuture = resultPromise.get_future();
    
    cameraThread_->queueOperation(CameraOpType::Custom, 
        [this]() {
            // This runs in the camera thread
            
        #ifdef HAS_SAPERA
            if (!device_ || !isConnected_) {
                return;
            }
            
            // Configure camera settings - this is a placeholder
            printCameraInfo();
        #endif
        },
        [&resultPromise](bool success) {
            // Callback - this also runs in the camera thread
            resultPromise.set_value(success);
        },
        "ConfigureCamera");
    
    // Wait for the operation to complete
    success = resultFuture.get();
    
    return success;
}

// Camera thread management
#ifndef HAS_SAPERA
void SaperaCamera::startFrameThread()
{
    if (frameGeneratorThread_.isRunning()) {
        return;
    }
    
    // Create the frame generator if it doesn't exist
    if (!frameGenerator_) {
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
        // Use queued connection from worker to camera to avoid deadlocks
        connect(frameGenerator_, &FrameGeneratorWorker::frameGenerated,
                this, &SaperaCamera::handleNewFrame, Qt::QueuedConnection);
    }
    
    // Start the thread
    frameGeneratorThread_.start();
    qDebug() << "Frame generation thread started";
}

void SaperaCamera::stopFrameThread()
{
    if (!frameGeneratorThread_.isRunning()) {
        return;
    }
    
    // Stop the frame generator
    if (frameGenerator_) {
        frameGenerator_->stop();
    }
    
    // Stop the thread
    frameGeneratorThread_.quit();
    if (!frameGeneratorThread_.wait(1000)) {
        qDebug() << "Warning: Frame generator thread taking too long to quit, terminating...";
        frameGeneratorThread_.terminate();
    }
    
    qDebug() << "Frame generation thread stopped";
}
#endif

#ifdef HAS_SAPERA
// Sapera-specific implementation
void SaperaCamera::updateFrameFromBuffer()
{
    if (!buffer_) {
        return;
    }

    try {
        // Lock the frame to ensure thread safety
        std::lock_guard<std::mutex> lock(frameMutex_);

        // Get the buffer dimensions
        int width = buffer_->GetWidth();
        int height = buffer_->GetHeight();
        int pitch = buffer_->GetPitch();
        
        // Get buffer address - using a void pointer directly
        void* bufferAddress = nullptr;
        if (!buffer_->GetAddress(&bufferAddress) || !bufferAddress) {
            return;
        }

        // Create a QImage from the buffer data (assuming 8-bit grayscale)
        QImage newFrame(width, height, QImage::Format_Grayscale8);
        for (int y = 0; y < height; y++) {
            memcpy(newFrame.scanLine(y), 
                static_cast<unsigned char*>(bufferAddress) + y * pitch, 
                width);
        }

        // Update the current frame
        currentFrame_ = newFrame;
    }
    catch (const std::exception& e) {
        qDebug() << "Exception in updateFrameFromBuffer:" << e.what();
    }
    catch (...) {
        qDebug() << "Unknown exception in updateFrameFromBuffer";
    }
    
    // Get a copy of the current frame to emit
    QImage frameCopy;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        frameCopy = currentFrame_.copy();
    }
    
    // Emit the new frame available signal
    QMetaObject::invokeMethod(this, [this, frameCopy]() {
        emit newFrameAvailable(frameCopy);
    }, Qt::QueuedConnection);
}

void SaperaCamera::XferCallback(SapXferCallbackInfo* pInfo)
{
    try {
        // Get the camera object from the context
        SaperaCamera* camera = static_cast<SaperaCamera*>(pInfo->GetContext());
        if (camera) {
            // Use QMetaObject to safely call updateFrameFromBuffer across thread boundaries
            QMetaObject::invokeMethod(camera, &SaperaCamera::updateFrameFromBuffer, Qt::QueuedConnection);
        }
    }
    catch (const std::exception& e) {
        qDebug() << "Exception in XferCallback:" << e.what();
    }
    catch (...) {
        qDebug() << "Unknown exception in XferCallback";
    }
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
        
        // IMPORTANT: Set the transfer callback BEFORE creating the transfer object
        transfer_->SetCallbackInfo(XferCallback, this);
        
        if (!transfer_->Create()) {
            qDebug() << "Failed to create SapAcqDeviceToBuf";
            destroySaperaObjects();
            return false;
        }
        
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

void SaperaCamera::startFrameAcquisition()
{
    // Start acquisition if we have a transfer object
    if (transfer_) {
        qDebug() << "Starting camera acquisition";
        bool success = transfer_->Grab();
        if (!success) {
            qWarning() << "Failed to start acquisition";
        }
    }
}

void SaperaCamera::stopFrameAcquisition()
{
    // Stop acquisition if we have a transfer object
    if (transfer_) {
        qDebug() << "Stopping camera acquisition";
        transfer_->Freeze();
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
#endif

} // namespace cam_matrix::core::sapera 