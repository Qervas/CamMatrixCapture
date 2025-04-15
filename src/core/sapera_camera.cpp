#include "core/sapera_camera.hpp"
#include <QDebug>
#include <QColor>
#include <QString>
#include <QRandomGenerator>
#include <QDateTime>
#include <QPainter>
#include <cmath>

namespace cam_matrix::core {

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
    
    // Start frame timer (we'll use our own simulated images even in full Sapera mode)
    frameTimer_ = std::make_unique<QTimer>();
    frameTimer_->setInterval(33); // ~30 FPS
    connect(frameTimer_.get(), &QTimer::timeout, this, &SaperaCamera::generateMockFrame);
}

SaperaCamera::~SaperaCamera() {
    if (isConnected_) {
        disconnectCamera();
    }
    
    if (frameTimer_) {
        frameTimer_->stop();
        frameTimer_.reset();
    }
}

bool SaperaCamera::connectCamera() {
    if (isConnected_) {
        return true;
    }

    // Create Sapera objects
    try {
        device_ = std::make_unique<SapAcqDevice>(name_.c_str());
        if (!device_->Create()) {
            emit error("Failed to create acquisition device");
            return false;
        }
        
        buffer_ = std::make_unique<SapBufferWithTrash>(2, device_.get());
        if (!buffer_->Create()) {
            emit error("Failed to create buffer");
            destroySaperaObjects();
            return false;
        }
        
        transfer_ = std::make_unique<SapAcqDeviceToBuf>(device_.get(), buffer_.get());
        transfer_->SetCallbackInfo(XferCallback, this);
        if (!transfer_->Create()) {
            emit error("Failed to create transfer");
            destroySaperaObjects();
            return false;
        }
        
        view_ = std::make_unique<SapView>(buffer_.get());
        if (!view_->Create()) {
            emit error("Failed to create view");
            destroySaperaObjects();
            return false;
        }
        
        isConnected_ = true;
        emit statusChanged("Connected to camera: " + name_);
        
        // Start our simulation timer
        if (frameTimer_) {
            frameTimer_->start();
        }
        
        return true;
    }
    catch (const std::exception& e) {
        emit error(std::string("Exception connecting to camera: ") + e.what());
        return false;
    }
}

bool SaperaCamera::disconnectCamera() {
    if (!isConnected_) {
        return true;
    }

    // Stop the timer
    if (frameTimer_) {
        frameTimer_->stop();
    }

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
    
    std::lock_guard<std::mutex> lock(frameMutex_);
    currentFrame_ = disconnectedImage;
    emit newFrameAvailable(currentFrame_);
    
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

            // Lock the frame mutex
            std::lock_guard<std::mutex> lock(frameMutex_);

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

            currentFrame_ = newFrame;
        }
    }
    catch (const std::exception& e) {
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

void SaperaCamera::generateMockFrame() {
    // Only generate frames when connected
    if (!isConnected_) {
        return;
    }
    
    // Create a simulated camera frame with test patterns and overlay
    std::lock_guard<std::mutex> lock(frameMutex_);
    
    QImage newFrame(640, 480, QImage::Format_RGB32);
    
    // Get current timestamp for animation
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    
    // Apply exposure time simulation - longer exposure = brighter image with more noise
    double exposureFactor = std::min(1.0, exposureTime_ / 50000.0); // Normalize to 0-1 range
    
    // Create different test patterns based on camera name
    if (name_.find("Camera 1") != std::string::npos) {
        // Test pattern 1: Moving test chart
        newFrame.fill(Qt::black);
        
        // Create resolution chart with circles
        QPainter painter(&newFrame);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // Calculate animated position
        double phase = timestamp * 0.001;
        int centerX = 320 + int(100 * sin(phase));
        int centerY = 240 + int(80 * cos(phase * 0.7));
        
        // Draw concentric circles
        painter.setPen(QPen(Qt::white, 2));
        for (int radius = 20; radius < 200; radius += 20) {
            painter.drawEllipse(QPoint(centerX, centerY), radius, radius);
        }
        
        // Draw crosshair
        painter.drawLine(centerX - 200, centerY, centerX + 200, centerY);
        painter.drawLine(centerX, centerY - 200, centerX, centerY + 200);
        
        // Draw resolution bars in each corner
        int barWidth = 10;
        for (int corner = 0; corner < 4; corner++) {
            int x = (corner % 2 == 0) ? 50 : newFrame.width() - 150;
            int y = (corner < 2) ? 50 : newFrame.height() - 100;
            
            for (int i = 0; i < 10; i++) {
                painter.fillRect(x + i * barWidth, y, barWidth / 2, 50, 
                                (i % 2 == 0) ? Qt::white : Qt::black);
            }
        }
        
        // Simulate exposure effect - add noise proportional to exposure
        if (exposureFactor > 0.1) {
            // Brightness adjustment
            for (int y = 0; y < newFrame.height(); y++) {
                for (int x = 0; x < newFrame.width(); x++) {
                    QColor pixelColor = newFrame.pixelColor(x, y);
                    int r = std::min(255, int(pixelColor.red() * (0.5 + exposureFactor)));
                    int g = std::min(255, int(pixelColor.green() * (0.5 + exposureFactor)));
                    int b = std::min(255, int(pixelColor.blue() * (0.5 + exposureFactor)));
                    
                    // Add exposure-dependent noise
                    int noise = QRandomGenerator::global()->bounded(int(30 * exposureFactor));
                    r = qBound(0, r + noise, 255);
                    g = qBound(0, g + noise, 255);
                    b = qBound(0, b + noise, 255);
                    
                    newFrame.setPixelColor(x, y, QColor(r, g, b));
                }
            }
        }
    } 
    else {
        // Test pattern 2: Color bars with motion
        int barWidth = newFrame.width() / 8;
        int offset = (timestamp / 50) % barWidth; // Scrolling effect
        
        for (int x = 0; x < newFrame.width(); x++) {
            int adjX = (x + offset) % newFrame.width();
            int barIndex = adjX / barWidth;
            QColor barColor;
            
            switch (barIndex) {
                case 0: barColor = Qt::white; break;
                case 1: barColor = Qt::yellow; break;
                case 2: barColor = Qt::cyan; break;
                case 3: barColor = Qt::green; break;
                case 4: barColor = Qt::magenta; break;
                case 5: barColor = Qt::red; break;
                case 6: barColor = Qt::blue; break;
                case 7: barColor = Qt::black; break;
            }
            
            for (int y = 0; y < newFrame.height(); y++) {
                newFrame.setPixelColor(x, y, barColor);
            }
        }
        
        // Add a bouncing object
        QPainter painter(&newFrame);
        painter.setRenderHint(QPainter::Antialiasing);
        
        int ballSize = 40;
        int ballX = (newFrame.width() - ballSize) * 
            (0.5 + 0.4 * sin(timestamp * 0.001));
        int ballY = (newFrame.height() - ballSize) * 
            (0.5 + 0.4 * cos(timestamp * 0.0013));
        
        painter.setPen(Qt::black);
        painter.setBrush(Qt::white);
        painter.drawEllipse(ballX, ballY, ballSize, ballSize);
        
        // Draw horizontal resolution bars at bottom
        int yPos = newFrame.height() - 60;
        int height = 40;
        
        for (int i = 0; i < 40; i++) {
            int xPos = i * (newFrame.width() / 40);
            int width = newFrame.width() / 80;
            if (i % 2 == 0) {
                painter.fillRect(xPos, yPos, width, height, Qt::black);
            }
        }
        
        // Simulate exposure effect - add blooming and noise
        if (exposureFactor > 0.3) {
            QPainter effectPainter(&newFrame);
            effectPainter.setCompositionMode(QPainter::CompositionMode_Plus);
            
            // Blooming around bright areas
            QRadialGradient gradient(ballX + ballSize/2, ballY + ballSize/2, 
                                     ballSize * (0.8 + exposureFactor));
            gradient.setColorAt(0, QColor(255, 255, 255, 150 * exposureFactor));
            gradient.setColorAt(1, QColor(255, 255, 255, 0));
            
            effectPainter.setBrush(gradient);
            effectPainter.setPen(Qt::NoPen);
            effectPainter.drawEllipse(ballX - ballSize/2, ballY - ballSize/2, 
                                      ballSize * 2, ballSize * 2);
            
            // Add exposure-dependent noise
            for (int y = 0; y < newFrame.height(); y += 4) { // Sample every 4 pixels for performance
                for (int x = 0; x < newFrame.width(); x += 4) {
                    if (QRandomGenerator::global()->bounded(100) < exposureFactor * 40) {
                        int noise = QRandomGenerator::global()->bounded(int(50 * exposureFactor));
                        newFrame.setPixelColor(x, y, QColor(255, 255, 255, noise));
                    }
                }
            }
        }
    }
    
    // Add common camera HUD elements
    QPainter overlayPainter(&newFrame);
    
    // Camera info at top
    overlayPainter.setPen(Qt::white);
    overlayPainter.setFont(QFont("Arial", 12));
    overlayPainter.drawText(10, 20, QString::fromStdString("Mock " + name_));
    
    // Recording indicator - blinking red dot
    if ((timestamp / 500) % 2 == 0) {
        overlayPainter.setBrush(Qt::red);
        overlayPainter.setPen(Qt::white);
        overlayPainter.drawEllipse(newFrame.width() - 30, 15, 15, 15);
        overlayPainter.drawText(newFrame.width() - 90, 25, "REC");
    }
    
    // Timestamp
    QDateTime dt = QDateTime::currentDateTime();
    overlayPainter.drawText(10, newFrame.height() - 10, 
                           dt.toString("yyyy-MM-dd hh:mm:ss.zzz"));
    
    // Exposure info
    overlayPainter.drawText(newFrame.width() - 150, newFrame.height() - 10, 
                           QString("Exp: %1 μs").arg(exposureTime_, 0, 'f', 1));
    
    // Frame counter
    static int frameCounter = 0;
    frameCounter++;
    overlayPainter.drawText(10, 40, QString("Frame: %1").arg(frameCounter));
    
    // Apply the new frame
    currentFrame_ = newFrame;
    emit newFrameAvailable(currentFrame_);
}

#elif HAS_GIGE_VISION
// GigE Vision Interface implementation

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

    #if HAS_GIGE_VISION_HEADERS
    // Attempt to connect using GigE Vision Interface
    try {
        // TODO: Implement GigE Vision camera connection
        // This is a temporary implementation that simulates connection
        isConnected_ = true;
        emit statusChanged("Camera connected via GigE Vision: " + name_);
        
        // Start a timer to simulate streaming
        if (!frameTimer_) {
            frameTimer_ = std::make_unique<QTimer>();
            frameTimer_->setInterval(33); // ~30 FPS
            connect(frameTimer_.get(), &QTimer::timeout, this, &SaperaCamera::generateDummyFrame);
            frameTimer_->start();
        }
        return true;
    } catch (const std::exception& e) {
        emit error(std::string("Failed to connect to camera: ") + e.what());
        return false;
    }
    #else
    // No GigE Vision headers available, simulate connection
    isConnected_ = true;
    emit statusChanged("Camera connected (simulation): " + name_);
    
    // Start a timer to simulate streaming
    if (!frameTimer_) {
        frameTimer_ = std::make_unique<QTimer>();
        frameTimer_->setInterval(33); // ~30 FPS
        connect(frameTimer_.get(), &QTimer::timeout, this, &SaperaCamera::generateDummyFrame);
        frameTimer_->start();
    }
    return true;
    #endif
}

bool SaperaCamera::disconnectCamera() {
    if (!isConnected_) {
        return true;
    }

    // Stop the frame timer if it exists
    if (frameTimer_) {
        frameTimer_->stop();
        frameTimer_.reset();
    }

    #if HAS_GIGE_VISION_HEADERS
    // TODO: Implement actual GigE Vision camera disconnection
    #endif

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

void SaperaCamera::generateDummyFrame() {
    // Create a dummy frame for testing
    std::lock_guard<std::mutex> lock(frameMutex_);
    
    // Generate a test pattern or moving image
    QImage newFrame(640, 480, QImage::Format_RGB32);
    
    // Get current timestamp for animation
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    int pattern = (timestamp / 1000) % 4; // Change pattern every second
    
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
    painter.drawText(10, 30, QString::fromStdString(name_));
    painter.drawText(10, 60, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    painter.drawText(10, 90, QString("Exposure: %1 μs").arg(exposureTime_));
    painter.end();
    
    currentFrame_ = newFrame;
    emit newFrameAvailable(currentFrame_);
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

    // Simulate connection
    isConnected_ = true;
    emit statusChanged("Camera connected (simulation): " + name_);
    
    // Start a timer to simulate streaming
    if (!frameTimer_) {
        frameTimer_ = std::make_unique<QTimer>();
        frameTimer_->setInterval(33); // ~30 FPS
        connect(frameTimer_.get(), &QTimer::timeout, this, &SaperaCamera::generateDummyFrame);
        frameTimer_->start();
    }
    
    return true;
}

bool SaperaCamera::disconnectCamera() {
    if (!isConnected_) {
        return true;
    }

    // Stop the frame timer if it exists
    if (frameTimer_) {
        frameTimer_->stop();
        frameTimer_.reset();
    }

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

void SaperaCamera::generateDummyFrame() {
    // Same implementation as in the GigE Vision version
    // Create a dummy frame for testing
    std::lock_guard<std::mutex> lock(frameMutex_);
    
    // Generate a test pattern or moving image
    QImage newFrame(640, 480, QImage::Format_RGB32);
    
    // Get current timestamp for animation
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    int pattern = (timestamp / 1000) % 4; // Change pattern every second
    
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
    painter.drawText(10, 30, QString::fromStdString(name_));
    painter.drawText(10, 60, QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
    painter.drawText(10, 90, QString("Exposure: %1 μs").arg(exposureTime_));
    
    // Add "No SDK Available" notice
    painter.setPen(Qt::red);
    painter.drawText(10, newFrame.height() - 10, "No Camera SDK Available - Simulation Only");
    painter.end();
    
    currentFrame_ = newFrame;
    emit newFrameAvailable(currentFrame_);
}

#endif

} // namespace cam_matrix::core


