#include "ui/widgets/direct_camera_widget.hpp"
#if defined(SAPERA_FOUND)
#include "core/sapera_direct_access.hpp"
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDebug>
#include <QPixmap>

namespace cam_matrix::ui {

DirectCameraWidget::DirectCameraWidget(QWidget* parent)
    : QWidget(parent)
    , cameraStatusLabel_(nullptr)
    , cameraComboBox_(nullptr)
    , refreshButton_(nullptr)
    , connectButton_(nullptr)
    , disconnectButton_(nullptr)
    , videoFeedLabel_(nullptr)
    , exposureSlider_(nullptr)
    , exposureValueLabel_(nullptr)
    , formatComboBox_(nullptr)
    , liveCheckbox_(nullptr)
#if defined(SAPERA_FOUND)
    // Initialize camera_ in initialization list only when Sapera is available
#endif
    , isConnected_(false)
    , isStreaming_(false)
{
    setupUi();
    createConnections();
    
#if defined(SAPERA_FOUND)
    // Create the camera access object
    camera_ = std::make_unique<core::SaperaDirectAccess>(this);
    
    // Connect camera signals
    connect(camera_.get(), &core::SaperaDirectAccess::statusChanged,
            this, &DirectCameraWidget::statusChanged);
    
    connect(camera_.get(), &core::SaperaDirectAccess::error,
            this, &DirectCameraWidget::error);
    
    connect(camera_.get(), &core::SaperaDirectAccess::newFrameAvailable,
            this, &DirectCameraWidget::onNewFrame);
    
    // Initial refresh
    refreshCameras();
#else
    cameraStatusLabel_->setText(tr("Sapera SDK not available"));
    emit statusChanged(tr("Sapera SDK not available"));
#endif
}

DirectCameraWidget::~DirectCameraWidget() {
#if defined(SAPERA_FOUND)
    if (camera_ && isConnected_) {
        camera_->stopAcquisition();
    }
#endif
}

void DirectCameraWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    
    // Camera selection
    auto* selectionGroup = new QGroupBox(tr("Camera Selection"), this);
    auto* selectionLayout = new QHBoxLayout(selectionGroup);
    
    cameraComboBox_ = new QComboBox(selectionGroup);
    cameraComboBox_->setMinimumWidth(250);
    
    refreshButton_ = new QPushButton(tr("Refresh"), selectionGroup);
    connectButton_ = new QPushButton(tr("Connect"), selectionGroup);
    disconnectButton_ = new QPushButton(tr("Disconnect"), selectionGroup);
    
    // Disable buttons initially
    connectButton_->setEnabled(false);
    disconnectButton_->setEnabled(false);
    
    selectionLayout->addWidget(cameraComboBox_);
    selectionLayout->addWidget(refreshButton_);
    selectionLayout->addWidget(connectButton_);
    selectionLayout->addWidget(disconnectButton_);
    
    mainLayout->addWidget(selectionGroup);
    
    // Camera status
    cameraStatusLabel_ = new QLabel(tr("Not connected"), this);
    cameraStatusLabel_->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(cameraStatusLabel_);
    
    // Video feed
    videoFeedLabel_ = new QLabel(this);
    videoFeedLabel_->setAlignment(Qt::AlignCenter);
    videoFeedLabel_->setMinimumSize(640, 480);
    videoFeedLabel_->setStyleSheet("background-color: black;");
    mainLayout->addWidget(videoFeedLabel_);
    
    // Camera controls
    auto* controlsGroup = new QGroupBox(tr("Camera Controls"), this);
    auto* controlsLayout = new QVBoxLayout(controlsGroup);
    
    // Exposure control
    auto* exposureLayout = new QHBoxLayout();
    auto* exposureLabel = new QLabel(tr("Exposure:"), controlsGroup);
    exposureSlider_ = new QSlider(Qt::Horizontal, controlsGroup);
    exposureSlider_->setRange(100, 100000); // 100µs to 100ms
    exposureSlider_->setTickInterval(10000);
    exposureSlider_->setTickPosition(QSlider::TicksBelow);
    exposureValueLabel_ = new QLabel(tr("10000 µs"), controlsGroup);
    exposureValueLabel_->setMinimumWidth(80);
    
    exposureLayout->addWidget(exposureLabel);
    exposureLayout->addWidget(exposureSlider_);
    exposureLayout->addWidget(exposureValueLabel_);
    
    controlsLayout->addLayout(exposureLayout);
    
    // Format selection
    auto* formatLayout = new QHBoxLayout();
    auto* formatLabel = new QLabel(tr("Pixel Format:"), controlsGroup);
    formatComboBox_ = new QComboBox(controlsGroup);
    
    formatLayout->addWidget(formatLabel);
    formatLayout->addWidget(formatComboBox_);
    
    controlsLayout->addLayout(formatLayout);
    
    // Live checkbox
    liveCheckbox_ = new QCheckBox(tr("Live View"), controlsGroup);
    controlsLayout->addWidget(liveCheckbox_);
    
    mainLayout->addWidget(controlsGroup);
    
    // Disable controls initially
    exposureSlider_->setEnabled(false);
    formatComboBox_->setEnabled(false);
    liveCheckbox_->setEnabled(false);
}

void DirectCameraWidget::createConnections() {
    connect(refreshButton_, &QPushButton::clicked,
            this, &DirectCameraWidget::refreshCameras);
    
    connect(connectButton_, &QPushButton::clicked,
            this, &DirectCameraWidget::onConnectClicked);
    
    connect(disconnectButton_, &QPushButton::clicked,
            this, &DirectCameraWidget::onDisconnectClicked);
    
    connect(cameraComboBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DirectCameraWidget::onCameraSelected);
    
    connect(exposureSlider_, &QSlider::valueChanged,
            this, &DirectCameraWidget::onExposureChanged);
    
    connect(formatComboBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DirectCameraWidget::onFormatChanged);
    
    connect(liveCheckbox_, &QCheckBox::toggled, [this](bool checked) {
#if defined(SAPERA_FOUND)
        if (camera_ && isConnected_) {
            if (checked && !isStreaming_) {
                // Start acquisition
                if (camera_->startAcquisition()) {
                    isStreaming_ = true;
                }
            } else if (!checked && isStreaming_) {
                // Stop acquisition
                if (camera_->stopAcquisition()) {
                    isStreaming_ = false;
                }
            }
        }
#endif
    });
}

void DirectCameraWidget::refreshCameras() {
#if defined(SAPERA_FOUND)
    cameraComboBox_->clear();
    cameraList_.clear();
    
    // Get available cameras
    cameraList_ = core::SaperaDirectAccess::getAvailableCameras();
    
    if (cameraList_.empty()) {
        cameraStatusLabel_->setText(tr("No cameras found"));
        emit statusChanged(tr("No Sapera cameras found"));
        
        // Add a mock camera for testing the interface
        cameraList_.push_back("Mock Camera");
        cameraComboBox_->addItem("Mock Camera");
        emit statusChanged(tr("Added mock camera for testing"));
        
        // Always emit the cameras found signal, even if just mock cameras
        emit camerasFound(cameraList_);
        connectButton_->setEnabled(true);
    } else {
        for (const auto& camera : cameraList_) {
            cameraComboBox_->addItem(QString::fromStdString(camera));
        }
        
        cameraStatusLabel_->setText(tr("Found %1 cameras").arg(cameraList_.size()));
        emit statusChanged(tr("Found %1 Sapera cameras").arg(cameraList_.size()));
        
        // Emit the cameras found signal so main view can update
        emit camerasFound(cameraList_);
        
        // Enable connect button if a camera is selected
        connectButton_->setEnabled(true);
    }
#else
    // Even without Sapera, add a mock camera for UI testing
    cameraList_.push_back("Mock Camera");
    cameraComboBox_->addItem("Mock Camera");
    cameraStatusLabel_->setText(tr("Sapera SDK not available - using mock camera"));
    emit statusChanged(tr("Sapera SDK not available - using mock camera"));
    emit camerasFound(cameraList_);
    connectButton_->setEnabled(true);
#endif
}

void DirectCameraWidget::onConnectClicked() {
#if defined(SAPERA_FOUND)
    int index = cameraComboBox_->currentIndex();
    if (index >= 0 && index < static_cast<int>(cameraList_.size())) {
        const std::string& serverName = cameraList_[index];
        
        if (camera_->initialize(serverName)) {
            isConnected_ = true;
            updateControls();
            
            // Emit signal that this camera is connected and detected
            emit cameraDetected(serverName);
            
            // Update exposure slider to match camera value
            double exposureTime = camera_->getExposureTime();
            exposureSlider_->setValue(static_cast<int>(exposureTime));
            exposureValueLabel_->setText(tr("%1 µs").arg(exposureTime));
            
            // Get available pixel formats
            formatComboBox_->clear();
            formatList_ = camera_->getAvailablePixelFormats();
            for (const auto& format : formatList_) {
                formatComboBox_->addItem(QString::fromStdString(format));
            }
            
            // Update camera status
            cameraStatusLabel_->setText(tr("Connected to %1").arg(QString::fromStdString(serverName)));
            
            // Enable live checkbox
            liveCheckbox_->setEnabled(true);
        }
    }
#endif
}

void DirectCameraWidget::onDisconnectClicked() {
#if defined(SAPERA_FOUND)
    if (camera_ && isConnected_) {
        // Stop acquisition if running
        if (isStreaming_) {
            camera_->stopAcquisition();
            isStreaming_ = false;
            liveCheckbox_->setChecked(false);
        }
        
        // Clear video display
        videoFeedLabel_->clear();
        videoFeedLabel_->setStyleSheet("background-color: black;");
        
        isConnected_ = false;
        updateControls();
        
        cameraStatusLabel_->setText(tr("Disconnected"));
        emit statusChanged(tr("Camera disconnected"));
    }
#endif
}

void DirectCameraWidget::onCameraSelected(int index) {
    // Enable connect button if a valid camera is selected
    connectButton_->setEnabled(index >= 0);
}

void DirectCameraWidget::onExposureChanged(int value) {
#if defined(SAPERA_FOUND)
    if (camera_ && isConnected_) {
        // Update exposure value
        double exposureTime = static_cast<double>(value);
        if (camera_->setExposureTime(exposureTime)) {
            exposureValueLabel_->setText(tr("%1 µs").arg(exposureTime));
        }
    }
#endif
}

void DirectCameraWidget::onFormatChanged(int index) {
#if defined(SAPERA_FOUND)
    if (camera_ && isConnected_ && index >= 0 && index < static_cast<int>(formatList_.size())) {
        // Need to stop acquisition first
        bool wasStreaming = isStreaming_;
        if (wasStreaming) {
            camera_->stopAcquisition();
            isStreaming_ = false;
        }
        
        // Change format
        const std::string& format = formatList_[index];
        if (camera_->setPixelFormat(format)) {
            emit statusChanged(tr("Pixel format changed to %1").arg(QString::fromStdString(format)));
        }
        
        // Restart if needed
        if (wasStreaming) {
            if (camera_->startAcquisition()) {
                isStreaming_ = true;
            } else {
                liveCheckbox_->setChecked(false);
            }
        }
    }
#endif
}

void DirectCameraWidget::onNewFrame(const QImage& frame) {
    // Convert QImage to QPixmap and display it
    QPixmap pixmap = QPixmap::fromImage(frame);
    
    // Scale to fit if needed
    if (pixmap.width() > videoFeedLabel_->width() || pixmap.height() > videoFeedLabel_->height()) {
        pixmap = pixmap.scaled(videoFeedLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    videoFeedLabel_->setPixmap(pixmap);
}

void DirectCameraWidget::updateControls() {
    // Update UI based on connection state
    connectButton_->setEnabled(!isConnected_);
    disconnectButton_->setEnabled(isConnected_);
    cameraComboBox_->setEnabled(!isConnected_);
    refreshButton_->setEnabled(!isConnected_);
    
    exposureSlider_->setEnabled(isConnected_);
    formatComboBox_->setEnabled(isConnected_);
}

} // namespace cam_matrix::ui 