#include "ui/widgets/direct_camera_widget.hpp"
#if defined(SAPERA_FOUND)
#include "core/sapera_direct_access.hpp"
#endif
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDebug>
#include <QPixmap>
#include <QApplication>
#include <QIcon>
#include <QSizePolicy>

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
    // Get system palette for theme-aware colors
    QPalette systemPalette = QApplication::palette();
    bool isDarkTheme = systemPalette.color(QPalette::Window).lightness() < 128;
    
    // Modern color palette
    QString accentColor = "#007AFF";         // Apple-inspired blue
    QString accentColorHover = "#0069D9";    // Slightly darker when hovering
    QString accentColorPressed = "#0062CC";  // Even darker when pressed
    QString borderColor = isDarkTheme ? "#3A3A3C" : "#D1D1D6";
    QString bgDark = isDarkTheme ? "#1C1C1E" : "#F2F2F7";
    QString bgMedium = isDarkTheme ? "#2C2C2E" : "#E5E5EA";
    
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(16);
    
    // Camera selection card
    auto* selectionCard = new QWidget(this);
    selectionCard->setStyleSheet(QString(
        "QWidget { background-color: %1; border-radius: 8px; }"
    ).arg(bgDark));
    
    auto* selectionLayout = new QVBoxLayout(selectionCard);
    selectionLayout->setContentsMargins(16, 16, 16, 16);
    selectionLayout->setSpacing(12);
    
    // Title
    auto* titleLabel = new QLabel("Camera Selection", selectionCard);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 15px;");
    selectionLayout->addWidget(titleLabel);
    
    // Camera selection row
    auto* cameraSelectionLayout = new QHBoxLayout();
    cameraSelectionLayout->setSpacing(10);
    
    cameraComboBox_ = new QComboBox(selectionCard);
    cameraComboBox_->setMinimumWidth(250);
    cameraComboBox_->setStyleSheet(QString(
        "QComboBox { padding: 6px; border-radius: 6px; background-color: %1; }"
        "QComboBox::drop-down { width: 20px; border: none; }"
    ).arg(bgMedium));
    
    refreshButton_ = new QPushButton(selectionCard);
    refreshButton_->setIcon(QIcon::fromTheme("view-refresh"));
    refreshButton_->setIconSize(QSize(16, 16));
    refreshButton_->setToolTip("Refresh Camera List");
    refreshButton_->setFixedSize(32, 32);
    refreshButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; border-radius: 16px; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
    ).arg(accentColor, accentColorHover, accentColorPressed));
    
    cameraSelectionLayout->addWidget(cameraComboBox_);
    cameraSelectionLayout->addWidget(refreshButton_);
    
    selectionLayout->addLayout(cameraSelectionLayout);
    
    // Connect/Disconnect buttons
    auto* connectionLayout = new QHBoxLayout();
    connectionLayout->setSpacing(10);
    
    connectButton_ = new QPushButton("Connect", selectionCard);
    connectButton_->setEnabled(false);
    connectButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 0; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
        "QPushButton:disabled { background-color: #A0A0A0; }"
    ).arg(accentColor, accentColorHover, accentColorPressed));
    
    disconnectButton_ = new QPushButton("Disconnect", selectionCard);
    disconnectButton_->setEnabled(false);
    disconnectButton_->setStyleSheet(QString(
        "QPushButton { background-color: #FF3B30; color: white; border-radius: 6px; padding: 8px 0; }"
        "QPushButton:hover { background-color: #FF5144; }"
        "QPushButton:pressed { background-color: #FF6259; }"
        "QPushButton:disabled { background-color: #A0A0A0; }"
    ));
    
    connectionLayout->addWidget(connectButton_);
    connectionLayout->addWidget(disconnectButton_);
    
    selectionLayout->addLayout(connectionLayout);
    
    // Add camera status
    cameraStatusLabel_ = new QLabel("Not connected", selectionCard);
    cameraStatusLabel_->setAlignment(Qt::AlignCenter);
    cameraStatusLabel_->setStyleSheet(QString(
        "padding: 6px; border-radius: 6px; background-color: %1; margin-top: 8px;"
    ).arg(bgMedium));
    
    selectionLayout->addWidget(cameraStatusLabel_);
    
    // Add card to main layout
    mainLayout->addWidget(selectionCard);
    
    // Video feed card
    auto* videoCard = new QWidget(this);
    videoCard->setStyleSheet(QString(
        "QWidget { background-color: %1; border-radius: 8px; }"
    ).arg(bgDark));
    videoCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    auto* videoLayout = new QVBoxLayout(videoCard);
    videoLayout->setContentsMargins(16, 16, 16, 16);
    videoLayout->setSpacing(12);
    
    // Title
    auto* videoTitle = new QLabel("Camera Feed", videoCard);
    videoTitle->setStyleSheet("font-weight: bold; font-size: 15px;");
    videoLayout->addWidget(videoTitle);
    
    // Video display
    videoFeedLabel_ = new QLabel(videoCard);
    videoFeedLabel_->setAlignment(Qt::AlignCenter);
    videoFeedLabel_->setMinimumSize(640, 480);
    videoFeedLabel_->setStyleSheet("background-color: black; border-radius: 6px;");
    videoFeedLabel_->setScaledContents(false);
    
    videoLayout->addWidget(videoFeedLabel_);
    
    // Live checkbox
    liveCheckbox_ = new QCheckBox("Live View", videoCard);
    liveCheckbox_->setEnabled(false);
    liveCheckbox_->setStyleSheet(QString(
        "QCheckBox { padding: 4px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; }"
        "QCheckBox::indicator:unchecked { border: 2px solid %1; border-radius: 4px; background-color: transparent; }"
        "QCheckBox::indicator:checked { border: 2px solid %2; border-radius: 4px; background-color: %2; }"
    ).arg(borderColor, accentColor));
    
    videoLayout->addWidget(liveCheckbox_);
    
    mainLayout->addWidget(videoCard);
    
    // Camera controls card
    auto* controlsCard = new QWidget(this);
    controlsCard->setStyleSheet(QString(
        "QWidget { background-color: %1; border-radius: 8px; }"
    ).arg(bgDark));
    
    auto* controlsLayout = new QVBoxLayout(controlsCard);
    controlsLayout->setContentsMargins(16, 16, 16, 16);
    controlsLayout->setSpacing(12);
    
    // Title
    auto* controlsTitle = new QLabel("Camera Controls", controlsCard);
    controlsTitle->setStyleSheet("font-weight: bold; font-size: 15px;");
    controlsLayout->addWidget(controlsTitle);
    
    // Exposure control
    auto* exposureLayout = new QVBoxLayout();
    exposureLayout->setSpacing(4);
    
    auto* exposureLabelLayout = new QHBoxLayout();
    auto* exposureLabel = new QLabel("Exposure:", controlsCard);
    exposureValueLabel_ = new QLabel("10000 μs", controlsCard);
    exposureValueLabel_->setAlignment(Qt::AlignRight);
    
    exposureLabelLayout->addWidget(exposureLabel);
    exposureLabelLayout->addWidget(exposureValueLabel_);
    
    exposureSlider_ = new QSlider(Qt::Horizontal, controlsCard);
    exposureSlider_->setRange(100, 100000); // 100µs to 100ms
    exposureSlider_->setValue(10000);
    exposureSlider_->setEnabled(false);
    exposureSlider_->setStyleSheet(QString(
        "QSlider::groove:horizontal { height: 6px; background-color: %1; border-radius: 3px; }"
        "QSlider::handle:horizontal { background-color: %2; width: 18px; margin: -6px 0; border-radius: 9px; }"
        "QSlider::handle:horizontal:hover { background-color: %3; }"
        "QSlider::handle:horizontal:disabled { background-color: #A0A0A0; }"
    ).arg(bgMedium, accentColor, accentColorHover));
    
    exposureLayout->addLayout(exposureLabelLayout);
    exposureLayout->addWidget(exposureSlider_);
    
    controlsLayout->addLayout(exposureLayout);
    
    // Format selection
    auto* formatLayout = new QHBoxLayout();
    formatLayout->setSpacing(10);
    
    auto* formatLabel = new QLabel("Pixel Format:", controlsCard);
    formatComboBox_ = new QComboBox(controlsCard);
    formatComboBox_->setEnabled(false);
    formatComboBox_->addItem("Mono8");
    formatComboBox_->addItem("Mono16");
    formatComboBox_->addItem("RGB24");
    formatComboBox_->setStyleSheet(QString(
        "QComboBox { padding: 6px; border-radius: 6px; background-color: %1; }"
        "QComboBox::drop-down { width: 20px; border: none; }"
        "QComboBox:disabled { color: #A0A0A0; }"
    ).arg(bgMedium));
    
    formatLayout->addWidget(formatLabel);
    formatLayout->addWidget(formatComboBox_);
    
    controlsLayout->addLayout(formatLayout);
    
    mainLayout->addWidget(controlsCard);
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
        
        // No longer adding mock cameras for testing
        
        // Update UI to reflect no cameras found
        connectButton_->setEnabled(false);
        
        // Emit empty camera list
        emit camerasFound(cameraList_);
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
    // No longer adding mock cameras even without Sapera
    cameraList_.clear();
    cameraStatusLabel_->setText(tr("Sapera SDK not available"));
    emit statusChanged(tr("Sapera SDK not available"));
    emit camerasFound(cameraList_);
    connectButton_->setEnabled(false);
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