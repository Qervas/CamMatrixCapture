#include "camera_page.hpp"
#include "ui/widgets/video_display_widget.hpp"
#include "ui/widgets/camera_control_widget.hpp"
#include "ui/widgets/sapera_status_widget.hpp"
#include "ui/dialogs/direct_camera_dialog.hpp"
#include "ui/dialogs/photo_preview_dialog.hpp"
#include "core/settings.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QTabWidget>
#include <QDateTime>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QScroller>
#include <QApplication>
#include <QTimer>

namespace cam_matrix::ui {

// Custom delegate for nicer list items
class CameraItemDelegate : public QStyledItemDelegate {
public:
    CameraItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        
        // Theme-aware selection background
        if (opt.state & QStyle::State_Selected) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(Qt::NoPen);
            
            // Get theme-appropriate selection color
            QColor selectionColor = QApplication::palette().color(QPalette::Highlight);
            selectionColor.setAlpha(128); // Semi-transparent
            painter->setBrush(selectionColor);
            
            painter->drawRoundedRect(opt.rect.adjusted(2, 2, -2, -2), 5, 5);
            painter->restore();
            
            // Use theme-appropriate text color
            opt.palette.setColor(QPalette::HighlightedText, QApplication::palette().color(QPalette::HighlightedText));
            opt.palette.setColor(QPalette::Highlight, Qt::transparent);
        }
        
        QStyledItemDelegate::paint(painter, opt, index);
    }
    
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        return QSize(size.width(), 36); // Taller row height
    }
};

CameraPage::CameraPage(QWidget* parent)
    : Page(parent),
      cameraManager_(std::make_unique<core::CameraManager>()),
      selectedCameraIndex_(-1)
{
    setupUi();
    createConnections();
    initialize();
}

CameraPage::~CameraPage() {
    cleanup();
}

void CameraPage::setupUi() {
    // Get system palette for theme-aware colors
    QPalette systemPalette = QApplication::palette();
    bool isDarkTheme = systemPalette.color(QPalette::Window).lightness() < 128;
    
    // Modern color palette
    QString accentColor = "#007AFF";         // Apple-inspired blue
    QString accentColorHover = "#0069D9";    // Slightly darker when hovering
    QString accentColorPressed = "#0062CC";  // Even darker when pressed
    QString successColor = "#34C759";        // Green for success states
    QString warningColor = "#FF9500";        // Orange for warnings
    QString errorColor = "#FF3B30";          // Red for errors
    
    // Border and background colors based on theme
    QString borderColor = isDarkTheme ? "#3A3A3C" : "#D1D1D6";
    QString bgDark = isDarkTheme ? "#1C1C1E" : "#F2F2F7";
    QString bgMedium = isDarkTheme ? "#2C2C2E" : "#E5E5EA";
    QString bgLight = isDarkTheme ? "#3A3A3C" : "#F9F9F9";
    QString textColor = isDarkTheme ? "#FFFFFF" : "#000000";
    QString dimTextColor = isDarkTheme ? "#8E8E93" : "#8E8E93";
    
    // Set application-wide font
    QFont appFont("SF Pro Text, -apple-system, BlinkMacSystemFont, Segoe UI, Roboto, Helvetica, Arial, sans-serif", 10);
    QApplication::setFont(appFont);
    
    // Main layout with a modern aesthetic
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Create a header bar with app title and controls
    auto headerBar = new QWidget();
    headerBar->setFixedHeight(50);
    headerBar->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid %2;")
                            .arg(bgDark, borderColor));
    
    auto headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(20, 0, 20, 0);
    
    auto titleLabel = new QLabel("Camera Matrix Capture");
    titleLabel->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;").arg(textColor));
    
    // Camera tools in header
    auto headerButtonsLayout = new QHBoxLayout();
    headerButtonsLayout->setSpacing(12);
    
    refreshButton_ = new QPushButton(QIcon::fromTheme("view-refresh"), "");
    refreshButton_->setToolTip(tr("Refresh Camera List"));
    refreshButton_->setFixedSize(32, 32);
    refreshButton_->setCursor(Qt::PointingHandCursor);
    
    directCameraButton_ = new QPushButton(QIcon::fromTheme("preferences-system"), "");
    directCameraButton_->setToolTip(tr("Direct Camera Access"));
    directCameraButton_->setFixedSize(32, 32);
    directCameraButton_->setCursor(Qt::PointingHandCursor);
    
    headerButtonsLayout->addWidget(refreshButton_);
    headerButtonsLayout->addWidget(directCameraButton_);
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addLayout(headerButtonsLayout);
    
    // Create a modern content area with main splitter
    auto contentWidget = new QWidget();
    contentWidget->setStyleSheet(QString("background-color: %1;").arg(bgMedium));
    
    auto contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    
    // Create a beautiful camera feed area
    videoDisplay_ = new VideoDisplayWidget();
    videoDisplay_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoDisplay_->setStyleSheet(QString("border: none; background-color: %1;").arg(bgDark));
    
    // Create a sleek sidebar for controls
    auto sidebarWidget = new QWidget();
    sidebarWidget->setFixedWidth(320);
    sidebarWidget->setStyleSheet(QString(
        "QWidget { background-color: %1; border-left: 1px solid %2; }"
    ).arg(bgLight, borderColor));
    
    auto sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(20, 20, 20, 20);
    sidebarLayout->setSpacing(20);
    
    // Camera selection with modern styling
    auto cameraSelectionWidget = new QWidget();
    cameraSelectionWidget->setStyleSheet(QString(
        "QWidget { background-color: %1; border-radius: 10px; }"
    ).arg(bgDark));
    
    auto cameraSelectionLayout = new QVBoxLayout(cameraSelectionWidget);
    cameraSelectionLayout->setContentsMargins(15, 15, 15, 15);
    cameraSelectionLayout->setSpacing(12);
    
    auto cameraHeaderLayout = new QHBoxLayout();
    auto cameraLabel = new QLabel(tr("Cameras"));
    cameraLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 14px;").arg(textColor));
    
    cameraHeaderLayout->addWidget(cameraLabel);
    cameraHeaderLayout->addStretch();
    
    cameraList_ = new QListWidget();
    cameraList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    cameraList_->setItemDelegate(new CameraItemDelegate(cameraList_));
    cameraList_->setStyleSheet(QString(
        "QListWidget { background-color: %1; border: none; border-radius: 8px; color: %2; }"
        "QListWidget::item { padding: 8px; margin-bottom: 4px; border-radius: 6px; }"
        "QListWidget::item:selected { background-color: %3; color: white; }"
        "QListWidget::item:hover:!selected { background-color: %4; }"
    ).arg(bgMedium, textColor, accentColor, bgLight));
    
    QScroller::grabGesture(cameraList_, QScroller::TouchGesture);
    
    // Camera action buttons with modern styling
    auto cameraActionLayout = new QHBoxLayout();
    cameraActionLayout->setSpacing(8);
    
    connectButton_ = new QPushButton(tr("Connect"));
    connectButton_->setCursor(Qt::PointingHandCursor);
    
    disconnectButton_ = new QPushButton(tr("Disconnect"));
    disconnectButton_->setCursor(Qt::PointingHandCursor);
    
    cameraActionLayout->addWidget(connectButton_);
    cameraActionLayout->addWidget(disconnectButton_);
    
    cameraSelectionLayout->addLayout(cameraHeaderLayout);
    cameraSelectionLayout->addWidget(cameraList_);
    cameraSelectionLayout->addLayout(cameraActionLayout);
    
    // Camera controls with elegant styling
    cameraControl_ = new CameraControlWidget();
    cameraControl_->setEnabled(false);
    cameraControl_->setStyleSheet(QString(
        "QWidget { background-color: %1; border-radius: 10px; padding: 0px; }"
    ).arg(bgDark));
    
    // Multi-camera operations with elegant styling
    syncGroup_ = new QGroupBox(tr("Multi-Camera"));
    syncGroup_->setStyleSheet(QString(
        "QGroupBox { background-color: %1; border-radius: 10px; padding-top: 12px; color: %2; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 15px; padding: 0 8px; }"
    ).arg(bgDark, textColor));
    
    auto syncLayout = new QVBoxLayout(syncGroup_);
    syncLayout->setContentsMargins(15, 20, 15, 15);
    syncLayout->setSpacing(12);
    
    // Selection controls with modern styling
    auto selectionControlsLayout = new QHBoxLayout();
    selectionControlsLayout->setSpacing(8);
    
    toggleSelectButton_ = new QPushButton(tr("Select All"));
    toggleSelectButton_->setCursor(Qt::PointingHandCursor);
    
    clearSelectionButton_ = new QPushButton(tr("Clear"));
    clearSelectionButton_->setCursor(Qt::PointingHandCursor);
    
    selectionControlsLayout->addWidget(toggleSelectButton_);
    selectionControlsLayout->addWidget(clearSelectionButton_);
    
    // Connection controls with modern styling
    auto connectionControlsLayout = new QHBoxLayout();
    connectionControlsLayout->setSpacing(8);
    
    connectSelectedButton_ = new QPushButton(tr("Connect Selected"));
    connectSelectedButton_->setCursor(Qt::PointingHandCursor);
    
    disconnectSelectedButton_ = new QPushButton(tr("Disconnect Selected"));
    disconnectSelectedButton_->setCursor(Qt::PointingHandCursor);
    
    connectionControlsLayout->addWidget(connectSelectedButton_);
    connectionControlsLayout->addWidget(disconnectSelectedButton_);
    
    // Capture button with prominent styling
    captureSyncButton_ = new QPushButton(tr("Synchronized Capture"));
    captureSyncButton_->setCursor(Qt::PointingHandCursor);
    captureSyncButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; font-weight: bold; padding: 10px; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
        "QPushButton:disabled { background-color: %4; color: rgba(255,255,255,0.5); }"
    ).arg(successColor, "#2EAF4F", "#27994A", "#BBE5C9"));
    
    syncProgressBar_ = new QProgressBar();
    syncProgressBar_->setRange(0, 100);
    syncProgressBar_->setValue(0);
    syncProgressBar_->setTextVisible(true);
    syncProgressBar_->setFormat("%v/%m");
    syncProgressBar_->hide();
    syncProgressBar_->setStyleSheet(QString(
        "QProgressBar { background-color: %1; border-radius: 3px; text-align: center; }"
        "QProgressBar::chunk { background-color: %2; border-radius: 3px; }"
    ).arg(bgMedium, accentColor));
    
    syncStatusLabel_ = new QLabel();
    syncStatusLabel_->setAlignment(Qt::AlignCenter);
    syncStatusLabel_->setStyleSheet(QString("color: %1; font-size: 12px;").arg(textColor));
    
    syncLayout->addLayout(selectionControlsLayout);
    syncLayout->addLayout(connectionControlsLayout);
    syncLayout->addWidget(captureSyncButton_);
    syncLayout->addWidget(syncProgressBar_);
    syncLayout->addWidget(syncStatusLabel_);
    
    // Debug console accessible from sidebar
    auto consoleExpanderButton = new QPushButton(tr("Debug Console"));
    consoleExpanderButton->setCheckable(true);
    consoleExpanderButton->setStyleSheet(QString(
        "QPushButton { text-align: left; padding: 8px 12px; background-color: %1; border-radius: 6px; }"
        "QPushButton:checked { background-color: %2; color: white; }"
    ).arg(bgDark, accentColor));
    
    auto debugWidget = new QWidget();
    debugWidget->setVisible(false);
    
    auto debugLayout = new QVBoxLayout(debugWidget);
    debugLayout->setContentsMargins(0, 12, 0, 0);
    
    debugConsole_ = new QPlainTextEdit();
    debugConsole_->setReadOnly(true);
    debugConsole_->setLineWrapMode(QPlainTextEdit::NoWrap);
    debugConsole_->setFixedHeight(200);
    
    QString consoleBackgroundColor = isDarkTheme ? "#272729" : "#F0F0F2";
    QString consoleTextColor = isDarkTheme ? "#DEDEDE" : "#333333";
    QString consoleStyle = QString(
        "QPlainTextEdit { font-family: Menlo, Monaco, 'Courier New', monospace; font-size: 12px; "
        "background-color: %1; color: %2; border: none; border-radius: 6px; padding: 8px; }"
    ).arg(consoleBackgroundColor, consoleTextColor);
    
    debugConsole_->setStyleSheet(consoleStyle);
    
    auto consoleToolbar = new QHBoxLayout();
    clearConsoleButton_ = new QPushButton(tr("Clear"));
    clearConsoleButton_->setCursor(Qt::PointingHandCursor);
    consoleToolbar->addStretch();
    consoleToolbar->addWidget(clearConsoleButton_);
    
    debugLayout->addWidget(debugConsole_);
    debugLayout->addLayout(consoleToolbar);
    
    connect(consoleExpanderButton, &QPushButton::toggled, debugWidget, &QWidget::setVisible);
    
    // Status indicator at the bottom
    auto statusWidget = new QWidget();
    statusWidget->setFixedHeight(30);
    statusWidget->setStyleSheet(QString("background-color: %1; border-top: 1px solid %2;")
                              .arg(bgDark, borderColor));
    
    auto statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(20, 0, 20, 0);
    
    auto statusLabel = new QLabel(tr("Ready"));
    statusLabel->setStyleSheet(QString("color: %1;").arg(dimTextColor));
    
    // Add SDK status to the status bar
    testSaperaButton_ = new QPushButton(tr("Test SDK"));
    testSaperaButton_->setStyleSheet(QString(
        "QPushButton { background: none; border: none; color: %1; }"
        "QPushButton:hover { color: %2; text-decoration: underline; }"
    ).arg(accentColor, accentColorHover));
    
    statusLayout->addWidget(statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(testSaperaButton_);
    
    // Add widgets to the sidebar
    sidebarLayout->addWidget(cameraSelectionWidget);
    sidebarLayout->addWidget(cameraControl_);
    sidebarLayout->addWidget(syncGroup_);
    sidebarLayout->addWidget(consoleExpanderButton);
    sidebarLayout->addWidget(debugWidget);
    sidebarLayout->addStretch();
    
    // Create a modern hidden drawer for SDK status
    saperaStatus_ = new SaperaStatusWidget();
    saperaStatus_->setVisible(false);
    saperaStatus_->setStyleSheet(QString("background-color: %1; border-left: 1px solid %2;")
                              .arg(bgLight, borderColor));
    
    // Add main components to content area
    contentLayout->addWidget(videoDisplay_);
    contentLayout->addWidget(sidebarWidget);
    contentLayout->addWidget(saperaStatus_);
    
    // Set main layout
    mainLayout->addWidget(headerBar);
    mainLayout->addWidget(contentWidget, 1);
    mainLayout->addWidget(statusWidget);
    
    // Set unified button styling
    QString buttonStyle = QString(
        "QPushButton { background-color: %1; color: white; border: none; border-radius: 6px; padding: 8px 12px; font-weight: medium; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
        "QPushButton:disabled { background-color: %4; color: rgba(255,255,255,0.5); }"
    ).arg(accentColor, accentColorHover, accentColorPressed, isDarkTheme ? "#353538" : "#D8D8DD");
    
    QString secondaryButtonStyle = QString(
        "QPushButton { background-color: %1; color: %2; border: none; border-radius: 6px; padding: 8px 12px; font-weight: medium; }"
        "QPushButton:hover { background-color: %3; }"
        "QPushButton:pressed { background-color: %4; }"
        "QPushButton:disabled { background-color: %5; color: rgba(%6, 0.5); }"
    ).arg(bgMedium, textColor, bgLight, bgDark, isDarkTheme ? "#252527" : "#EAEAED", isDarkTheme ? "255,255,255" : "0,0,0");
    
    // Apply button styles contextually
    connectButton_->setStyleSheet(buttonStyle);
    disconnectButton_->setStyleSheet(secondaryButtonStyle);
    
    toggleSelectButton_->setStyleSheet(secondaryButtonStyle);
    clearSelectionButton_->setStyleSheet(secondaryButtonStyle);
    
    connectSelectedButton_->setStyleSheet(buttonStyle);
    disconnectSelectedButton_->setStyleSheet(secondaryButtonStyle);
    
    refreshButton_->setStyleSheet(QString(
        "QPushButton { background-color: transparent; border-radius: 16px; }"
        "QPushButton:hover { background-color: rgba(128, 128, 128, 0.2); }"
        "QPushButton:pressed { background-color: rgba(128, 128, 128, 0.3); }"
    ));
    
    directCameraButton_->setStyleSheet(QString(
        "QPushButton { background-color: transparent; border-radius: 16px; }"
        "QPushButton:hover { background-color: rgba(128, 128, 128, 0.2); }"
        "QPushButton:pressed { background-color: rgba(128, 128, 128, 0.3); }"
    ));
    
    clearConsoleButton_->setStyleSheet(secondaryButtonStyle);
    testSaperaButton_->setStyleSheet(buttonStyle);
    
    // Connect SDK drawer toggle
    connect(testSaperaButton_, &QPushButton::clicked, [this]() {
        bool isVisible = saperaStatus_->isVisible();
        saperaStatus_->setVisible(!isVisible);
        // Only show when needed
        if (!isVisible) {
            saperaStatus_->setFixedWidth(300);
        }
    });
    
    // Add loading indicator overlay
    auto loadingOverlay = new QWidget(videoDisplay_);
    loadingOverlay->setObjectName("loadingOverlay");
    loadingOverlay->setStyleSheet("background-color: rgba(0, 0, 0, 0.7); border-radius: 10px;");
    loadingOverlay->hide();
    
    // Store reference to loading overlay for later use
    loadingOverlay_ = loadingOverlay;
    
    auto overlayLayout = new QVBoxLayout(loadingOverlay);
    
    auto spinnerLabel = new QLabel();
    // Use a loading animation GIF or progress indicator here
    
    auto loadingLabel = new QLabel("Connecting to camera...");
    loadingLabel->setStyleSheet("color: white; font-size: 16px;");
    loadingLabel->setAlignment(Qt::AlignCenter);
    
    overlayLayout->addStretch();
    overlayLayout->addWidget(spinnerLabel, 0, Qt::AlignCenter);
    overlayLayout->addWidget(loadingLabel, 0, Qt::AlignCenter);
    overlayLayout->addStretch();
    
    // Ensure overlay resizes with video display
    videoDisplay_->installEventFilter(this);
    loadingOverlay->setGeometry(videoDisplay_->rect());
}

void CameraPage::createConnections() {
    // Camera list and control buttons
    connect(refreshButton_, &QPushButton::clicked, this, &CameraPage::onRefreshCameras);
    connect(connectButton_, &QPushButton::clicked, this, &CameraPage::onConnectCamera);
    connect(disconnectButton_, &QPushButton::clicked, this, &CameraPage::onDisconnectCamera);
    connect(cameraList_, &QListWidget::currentRowChanged, this, &CameraPage::onCameraSelected);
    connect(cameraList_, &QListWidget::itemChanged, this, &CameraPage::onCameraSelectionChanged);
    
    // Multi-camera sync
    connect(clearSelectionButton_, &QPushButton::clicked, this, &CameraPage::onClearSelection);
    connect(toggleSelectButton_, &QPushButton::clicked, this, &CameraPage::onToggleSelectAll);
    connect(connectSelectedButton_, &QPushButton::clicked, this, &CameraPage::onConnectSelectedCameras);
    connect(disconnectSelectedButton_, &QPushButton::clicked, this, &CameraPage::onDisconnectSelectedCameras);
    connect(captureSyncButton_, &QPushButton::clicked, this, &CameraPage::onCaptureSync);
    
    // Debug console
    connect(clearConsoleButton_, &QPushButton::clicked, this, &CameraPage::clearDebugConsole);
    
    // Additional buttons
    connect(testSaperaButton_, &QPushButton::clicked, this, &CameraPage::onTestSaperaCamera);
    connect(directCameraButton_, &QPushButton::clicked, this, &CameraPage::onDirectCameraAccess);
    
    // Camera manager signals
    connect(cameraManager_.get(), &core::CameraManager::cameraStatusChanged, 
            this, &CameraPage::onCameraStatusChanged);
    connect(cameraManager_.get(), &core::CameraManager::managerStatusChanged, 
            this, &CameraPage::onManagerStatusChanged);
    connect(cameraManager_.get(), &core::CameraManager::newFrameAvailable, 
            this, &CameraPage::onNewFrame);
    
    // Fix photoCaptured signal connection - use the correct signal type
    // We need to use a lambda as an adapter since the signatures don't exactly match
    QObject::connect(cameraManager_.get(), 
                   qOverload<const QImage&, const std::string&>(&core::CameraManager::photoCaptured),
                   this, &CameraPage::onPhotoCaptured);
    
    connect(cameraManager_.get(), &core::CameraManager::syncCaptureProgress, 
            this, &CameraPage::onSyncCaptureProgress);
    connect(cameraManager_.get(), &core::CameraManager::syncCaptureComplete, 
            this, &CameraPage::onSyncCaptureComplete);
    
    // Camera control signals
    connect(cameraControl_, &CameraControlWidget::exposureChanged, 
            [this](double value) {
                if (selectedCameraIndex_ >= 0) {
                    cameraManager_->setExposureTime(selectedCameraIndex_, value);
                    logDebugMessage(QString("Set exposure time to %1 ms for camera %2")
                        .arg(value).arg(selectedCameraIndex_));
                }
            });
    
    connect(cameraControl_, &CameraControlWidget::gainChanged, 
            [this](double value) {
                if (selectedCameraIndex_ >= 0) {
                    cameraManager_->setGain(selectedCameraIndex_, value);
                    logDebugMessage(QString("Set gain to %1 for camera %2")
                        .arg(value).arg(selectedCameraIndex_));
                }
            });
    
    connect(cameraControl_, &CameraControlWidget::formatChanged, 
            [this](const QString& format) {
                if (selectedCameraIndex_ >= 0) {
                    cameraManager_->setFormat(selectedCameraIndex_, format.toStdString());
                    logDebugMessage(QString("Set format to %1 for camera %2")
                        .arg(format).arg(selectedCameraIndex_));
                }
            });
    
    connect(cameraControl_, &CameraControlWidget::photoCaptureRequested, 
            [this]() {
                if (selectedCameraIndex_ >= 0) {
                    onCapturePhotoRequested(selectedCameraIndex_);
                }
            });
}

void CameraPage::initialize() {
    // Set initial button states
    disconnectButton_->setEnabled(false);
    connectSelectedButton_->setEnabled(false);
    disconnectSelectedButton_->setEnabled(false);
    captureSyncButton_->setEnabled(false);
    
    // Load settings and update camera list
    loadSettings();
    
    // Scan for cameras immediately upon initialization
    cameraManager_->scanForCameras();
    updateCameraList();
    
    // Initial status log
    logDebugMessage("Camera page initialized", "INFO");
    
    // Automatically refresh camera list after a short delay to ensure all cameras are detected
    QTimer::singleShot(1000, this, &CameraPage::onRefreshCameras);
    
    // Set up a timer to request frames from the camera periodically (every 50ms)
    QTimer* frameTimer = new QTimer(this);
    connect(frameTimer, &QTimer::timeout, this, [this]() {
        // If we have a selected camera and it's connected, get its frame
        if (selectedCameraIndex_ >= 0 && cameraManager_->isCameraConnected(selectedCameraIndex_)) {
            auto* camera = cameraManager_->getSaperaCameraByIndex(selectedCameraIndex_);
            if (camera && camera->isConnected()) {
                QImage currentFrame = camera->getFrame();
                if (!currentFrame.isNull()) {
                    videoDisplay_->updateFrame(currentFrame);
                    
                    // Keep track of successful frame updates
                    static int frameCount = 0;
                    frameCount++;
                    
                    // Log only occasionally
                    if (frameCount % 60 == 0) {
                        logDebugMessage(QString("Camera feed active - frame %1 received").arg(frameCount), "DEBUG");
                    }
                }
            }
        }
    });
    frameTimer->start(16); // Poll at ~60Hz for smooth animation
    
    // Setup a watchdog timer that checks every 2 seconds to ensure the camera is still connected
    QTimer* watchdogTimer = new QTimer(this);
    connect(watchdogTimer, &QTimer::timeout, this, [this]() {
        if (selectedCameraIndex_ >= 0) {
            // Check if the camera is still connected
            bool isConnected = cameraManager_->isCameraConnected(selectedCameraIndex_);
            
            // If we think it's connected but actually it's not, attempt to reconnect
            if (!isConnected && disconnectButton_->isEnabled()) {
                logDebugMessage("Watchdog: Camera connection lost, attempting to reconnect...", "WARNING");
                onConnectCamera();
            }
            
            // Send a "keepalive" signal to the camera to prevent timeout
            auto* camera = cameraManager_->getSaperaCameraByIndex(selectedCameraIndex_);
            if (camera && camera->isConnected()) {
                // Get a frame to keep the connection active
                camera->getFrame();
            }
        }
    });
    watchdogTimer->start(2000); // Check every 2 seconds
}

void CameraPage::cleanup() {
    // Disconnect cameras and save settings
    cameraManager_->disconnectAllCameras();
    saveSettings();
    logDebugMessage("Camera page cleanup completed", "INFO");
}

void CameraPage::onRefreshCameras() {
    logDebugMessage("Refreshing camera list...");
    updateCameraList();
}

void CameraPage::onConnectCamera() {
    int index = cameraList_->currentRow();
    if (index >= 0) {
        logDebugMessage(QString("Connecting to camera at index %1...").arg(index));
        
        // Get the name of the camera for better logging
        QString cameraName = cameraList_->item(index)->text();
        logDebugMessage(QString("Connecting to camera: %1").arg(cameraName));
        
        // Show loading overlay
        if (loadingOverlay_) {
            loadingOverlay_->show();
            QApplication::processEvents(); // Ensure overlay appears immediately
        }
        
        // Connect the camera
        bool success = cameraManager_->connectCamera(index);
        
        // Hide loading overlay
        if (loadingOverlay_) {
            loadingOverlay_->hide();
        }
        
        if (success) {
            logDebugMessage(QString("Successfully connected to camera: %1").arg(cameraName), "SUCCESS");
            
            // Update UI state immediately
            disconnectButton_->setEnabled(true);
            cameraControl_->setEnabled(true);
            
            // Get the camera object directly and request an initial frame
            auto* camera = cameraManager_->getSaperaCameraByIndex(index);
            if (camera && camera->isConnected()) {
                // Try to get a current frame
                QImage currentFrame = camera->getFrame();
                if (!currentFrame.isNull()) {
                    videoDisplay_->updateFrame(currentFrame);
                    logDebugMessage("Updated video display with initial frame");
                }
                
                // Make sure to update UI after all changes
                QApplication::processEvents();
            }
        } else {
            logDebugMessage(QString("Failed to connect to camera: %1").arg(cameraName), "ERROR");
        }
    }
}

void CameraPage::onDisconnectCamera() {
    int index = cameraList_->currentRow();
    if (index >= 0) {
        logDebugMessage(QString("Disconnecting camera at index %1...").arg(index));
        
        // Show loading overlay
        if (loadingOverlay_) {
            loadingOverlay_->show();
            QApplication::processEvents(); // Ensure overlay appears immediately
        }
        
        cameraManager_->disconnectCamera(index);
        videoDisplay_->clear();
        
        // Hide loading overlay
        if (loadingOverlay_) {
            loadingOverlay_->hide();
        }
    }
}

void CameraPage::onCameraSelected(int index) {
    selectedCameraIndex_ = index;

    bool cameraSelected = (index >= 0);
    connectButton_->setEnabled(cameraSelected);
    disconnectButton_->setEnabled(cameraSelected && cameraManager_->isCameraConnected(index));
    
    if (cameraSelected) {
        logDebugMessage(QString("Camera selected: %1").arg(cameraList_->item(index)->text()));
        
        // Update camera control widget with camera settings
        bool isConnected = cameraManager_->isCameraConnected(index);
        
        // Force set enabled state for all controls
        cameraControl_->setEnabled(isConnected);
        
        // IMPORTANT: Set the camera index to enable the capture button
        cameraControl_->setCameraIndex(isConnected ? index : -1);
        
        // Log the connection status
        logDebugMessage(QString("Camera connected status: %1").arg(isConnected ? "Connected" : "Not Connected"));
        
        if (isConnected) {
            double exposure = cameraManager_->getExposureTime(index);
            double gain = cameraManager_->getGain(index);
            std::string format = cameraManager_->getFormat(index);
            
            logDebugMessage(QString("Setting camera controls - Exposure: %1ms, Gain: %2, Format: %3")
                .arg(exposure).arg(gain).arg(QString::fromStdString(format)));
                
            // Update the control values
            cameraControl_->setExposure(exposure);
            cameraControl_->setGain(gain);
            cameraControl_->setFormat(QString::fromStdString(format));
            
            // Force update the UI to ensure controls are enabled
            QApplication::processEvents();
            
            logDebugMessage(QString("Loaded camera settings - Exposure: %1ms, Gain: %2, Format: %3")
                .arg(exposure).arg(gain).arg(QString::fromStdString(format)));
                
            // Get a current frame to show in the video display
            auto* camera = cameraManager_->getSaperaCameraByIndex(index);
            if (camera) {
                QImage currentFrame = camera->getFrame();
                if (!currentFrame.isNull()) {
                    videoDisplay_->updateFrame(currentFrame);
                    logDebugMessage("Updated video display with current frame");
                }
            }
        } else {
            logDebugMessage("Camera not connected - controls disabled");
            cameraControl_->setEnabled(false);
        }
    } else {
        cameraControl_->setEnabled(false);
        cameraControl_->setCameraIndex(-1); // Disable capture button
    }
}

void CameraPage::onCameraStatusChanged(const QString& status) {
    logDebugMessage(QString("Camera status: %1").arg(status));
    updateCameraList();
}

void CameraPage::onManagerStatusChanged(const std::string& status) {
    logDebugMessage(QString("Manager status: %1").arg(QString::fromStdString(status)));
}

void CameraPage::onNewFrame(const QImage& frame) {
    // Update the video display with the new frame
    videoDisplay_->updateFrame(frame);
    
    // Debug: log frame updates periodically
    static int frameCount = 0;
    frameCount++;
    
    if (frameCount % 30 == 0) {  // Log every 30 frames to avoid flooding
        logDebugMessage(QString("Received frame %1 (%2x%3)")
            .arg(frameCount)
            .arg(frame.width())
            .arg(frame.height()), "DEBUG");
    }
}

void CameraPage::onTestSaperaCamera() {
    logDebugMessage("Testing Sapera camera...", "ACTION");
    // Implementation remains the same
}

void CameraPage::onDirectCameraAccess() {
    logDebugMessage("Opening direct camera access dialog...", "ACTION");
    DirectCameraDialog dialog(this);
    
    // Connect signals from dialog to update our camera list
    connect(&dialog, &DirectCameraDialog::camerasFound, 
            [this](const std::vector<std::string>& cameraNames) {
                logDebugMessage(QString("Direct access found %1 cameras").arg(cameraNames.size()), "INFO");
                // Signal to CameraManager to update its camera list
                cameraManager_->updateCamerasFromDirectAccess(cameraNames);
                // Update our UI
                updateCameraList();
            });
            
    connect(&dialog, &DirectCameraDialog::cameraDetected,
            [this](const std::string& cameraName) {
                logDebugMessage(QString("Direct access connected to camera: %1")
                    .arg(QString::fromStdString(cameraName)), "INFO");
            });
            
    connect(&dialog, &DirectCameraDialog::refreshMainCameraView,
            [this]() {
                logDebugMessage("Refreshing main camera view...", "ACTION");
                onRefreshCameras();
            });
    
    if (dialog.exec() == QDialog::Accepted) {
        logDebugMessage("Direct camera access settings applied");
        
        // Refresh our camera list to show any newly detected cameras
        onRefreshCameras();
    }
}

void CameraPage::onCapturePhotoRequested(int cameraIndex) {
    if (cameraIndex >= 0 && cameraManager_->isCameraConnected(cameraIndex)) {
        QString timeStamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString basePath = QSettings().value("camera/savePath", QDir::homePath()).toString();
        QString fileName = QString("%1/capture_%2.png").arg(basePath).arg(timeStamp);
        
        QString path = QFileDialog::getSaveFileName(
            this, tr("Save Photo"), fileName, tr("Image Files (*.png *.jpg)"));
            
        if (!path.isEmpty()) {
            logDebugMessage(QString("Capturing photo from camera %1 to %2").arg(cameraIndex).arg(path));
            cameraManager_->capturePhoto(cameraIndex, path.toStdString());
        }
    }
}

void CameraPage::onPhotoCaptured(const QImage& image, const std::string& path) {
    logDebugMessage(QString("Photo captured to %1").arg(QString::fromStdString(path)), "SUCCESS");
    PhotoPreviewDialog dialog(image, QString::fromStdString(path), this);
    dialog.exec();
}

void CameraPage::loadSettings() {
    QSettings settings;
    QVariant savePath = settings.value("camera/savePath");
    if (savePath.isNull()) {
        settings.setValue("camera/savePath", QDir::homePath());
    }
}

void CameraPage::saveSettings() {
    // Save any settings if needed
}

void CameraPage::updateCameraList() {
    cameraList_->clear();

    std::vector<std::string> cameras = cameraManager_->getAvailableCameras();
    
    // If no cameras found, try to scan for cameras again
    if (cameras.empty()) {
        logDebugMessage("No cameras found, trying to scan again...", "WARNING");
        cameraManager_->scanForCameras();
        cameras = cameraManager_->getAvailableCameras();
    }
    
    for (size_t i = 0; i < cameras.size(); ++i) {
        QString cameraName = QString::fromStdString(cameras[i]);
        QListWidgetItem* item = new QListWidgetItem(cameraName);
        
        // Add connection status
        if (cameraManager_->isCameraConnected(i)) {
            item->setIcon(QIcon::fromTheme("network-wired"));
            item->setText(QString("%1 (Connected)").arg(cameraName));
            item->setForeground(QBrush(QColor(46, 139, 87))); // SeaGreen
        } else {
            item->setIcon(QIcon::fromTheme("network-offline"));
        }
        
        // Make items checkable for multi-selection
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        
        cameraList_->addItem(item);
    }
    
    logDebugMessage(QString("Found %1 cameras").arg(cameras.size()));
    
    // Update UI based on selected camera
    if (selectedCameraIndex_ >= 0 && selectedCameraIndex_ < cameraList_->count()) {
        cameraList_->setCurrentRow(selectedCameraIndex_);
    } else if (cameraList_->count() > 0) {
        cameraList_->setCurrentRow(0);
    } else {
        selectedCameraIndex_ = -1;
        connectButton_->setEnabled(false);
        disconnectButton_->setEnabled(false);
        cameraControl_->setEnabled(false);
    }
    
    updateSyncUI();
}

// Multi-camera synchronization methods
void CameraPage::onCameraSelectionChanged(QListWidgetItem* item) {
    updateSyncUI();
}

void CameraPage::onConnectSelectedCameras() {
    std::vector<int> selectedIndices;
    for (int i = 0; i < cameraList_->count(); ++i) {
        if (cameraList_->item(i)->checkState() == Qt::Checked) {
            selectedIndices.push_back(i);
        }
    }
    
    if (!selectedIndices.empty()) {
        logDebugMessage(QString("Connecting %1 selected cameras...").arg(selectedIndices.size()));
        for (int index : selectedIndices) {
            if (!cameraManager_->isCameraConnected(index)) {
                cameraManager_->connectCamera(index);
            }
        }
    }
}

void CameraPage::onDisconnectSelectedCameras() {
    std::vector<int> selectedIndices;
    for (int i = 0; i < cameraList_->count(); ++i) {
        if (cameraList_->item(i)->checkState() == Qt::Checked) {
            selectedIndices.push_back(i);
        }
    }
    
    if (!selectedIndices.empty()) {
        logDebugMessage(QString("Disconnecting %1 selected cameras...").arg(selectedIndices.size()));
        for (int index : selectedIndices) {
            if (cameraManager_->isCameraConnected(index)) {
                cameraManager_->disconnectCamera(index);
            }
        }
        videoDisplay_->clear();
    }
}

void CameraPage::onCaptureSync() {
    QString timeStamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString basePath = core::Settings::getPhotoSaveDirectory();
    QString dirPath = basePath + "/sync_" + timeStamp;
    
    QDir dir;
    if (!dir.exists(dirPath)) {
        dir.mkpath(dirPath);
    }
    
    logDebugMessage(QString("Starting synchronized capture to %1").arg(dirPath), "ACTION");
    syncProgressBar_->setValue(0);
    syncProgressBar_->show();
    
    // Call the capturePhotosSync method
    bool success = cameraManager_->capturePhotosSync(dirPath.toStdString());
    
    if (!success) {
        logDebugMessage("Failed to start synchronized capture", "ERROR");
        syncProgressBar_->hide();
    }
}

void CameraPage::onClearSelection() {
    for (int i = 0; i < cameraList_->count(); ++i) {
        cameraList_->item(i)->setCheckState(Qt::Unchecked);
    }
    updateSyncUI();
    logDebugMessage("Cleared camera selection");
}

void CameraPage::onToggleSelectAll() {
    bool allSelected = areAllCamerasSelected();
    
    for (int i = 0; i < cameraList_->count(); ++i) {
        cameraList_->item(i)->setCheckState(allSelected ? Qt::Unchecked : Qt::Checked);
    }
    
    updateSyncUI();
    logDebugMessage(allSelected ? "Deselected all cameras" : "Selected all cameras");
}

void CameraPage::onSyncCaptureProgress(int current, int total) {
    syncProgressBar_->setValue(current);
    syncStatusLabel_->setText(tr("Capturing %1 of %2...").arg(current).arg(total));
    logDebugMessage(QString("Sync capture progress: %1/%2").arg(current).arg(total));
}

void CameraPage::onSyncCaptureComplete(int successCount, int total) {
    syncProgressBar_->setValue(total);
    syncStatusLabel_->setText(tr("Captured %1 of %2 successfully").arg(successCount).arg(total));
    
    if (successCount == total) {
        logDebugMessage(QString("Synchronized capture completed successfully: %1 images").arg(total), "SUCCESS");
    } else {
        logDebugMessage(QString("Synchronized capture completed with issues: %1/%2 successful")
            .arg(successCount).arg(total), "WARNING");
    }
    
    QTimer::singleShot(3000, [this]() {
        syncProgressBar_->hide();
        syncStatusLabel_->clear();
    });
}

void CameraPage::updateSyncUI() {
    int selectedCount = 0;
    int connectedSelectedCount = 0;
    
    for (int i = 0; i < cameraList_->count(); ++i) {
        if (cameraList_->item(i)->checkState() == Qt::Checked) {
            selectedCount++;
            if (cameraManager_->isCameraConnected(i)) {
                connectedSelectedCount++;
            }
        }
    }
    
    connectSelectedButton_->setEnabled(selectedCount > 0);
    disconnectSelectedButton_->setEnabled(connectedSelectedCount > 0);
    captureSyncButton_->setEnabled(connectedSelectedCount > 1);
    
    toggleSelectButton_->setText(areAllCamerasSelected() ? tr("Deselect All") : tr("Select All"));
}

bool CameraPage::areAllCamerasSelected() const {
    for (int i = 0; i < cameraList_->count(); ++i) {
        if (cameraList_->item(i)->checkState() != Qt::Checked) {
            return false;
        }
    }
    return cameraList_->count() > 0;
}

// Debug console methods
void CameraPage::logDebugMessage(const QString& message, const QString& type) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    
    // Theme-aware colors for message types
    bool isDarkTheme = QApplication::palette().color(QPalette::Window).lightness() < 128;
    
    QString infoColor = isDarkTheme ? "#abb2bf" : "#333333";
    QString warningColor = isDarkTheme ? "#e5c07b" : "#b58900";
    QString errorColor = isDarkTheme ? "#e06c75" : "#dc322f";
    QString successColor = isDarkTheme ? "#98c379" : "#2aa198";
    QString actionColor = isDarkTheme ? "#61afef" : "#268bd2";
    QString hintColor = isDarkTheme ? "#c678dd" : "#6c71c4";
    
    QString formattedMessage;
    
    if (type == "INFO") {
        formattedMessage = QString("<span style='color:%1'>[%2] [INFO] %3</span>")
            .arg(infoColor, timestamp, message);
    } else if (type == "WARNING") {
        formattedMessage = QString("<span style='color:%1'>[%2] [WARN] %3</span>")
            .arg(warningColor, timestamp, message);
    } else if (type == "ERROR") {
        formattedMessage = QString("<span style='color:%1'>[%2] [ERROR] %3</span>")
            .arg(errorColor, timestamp, message);
    } else if (type == "SUCCESS") {
        formattedMessage = QString("<span style='color:%1'>[%2] [SUCCESS] %3</span>")
            .arg(successColor, timestamp, message);
    } else if (type == "ACTION") {
        formattedMessage = QString("<span style='color:%1'>[%2] [ACTION] %3</span>")
            .arg(actionColor, timestamp, message);
    } else if (type == "HINT") {
        formattedMessage = QString("<span style='color:%1'>[%2] [HINT] %3</span>")
            .arg(hintColor, timestamp, message);
    } else {
        formattedMessage = QString("<span style='color:%1'>[%2] [%3] %4</span>")
            .arg(infoColor, timestamp, type, message);
    }
    
    debugConsole_->appendHtml(formattedMessage);
    
    // Scroll to bottom
    QScrollBar* scrollBar = debugConsole_->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void CameraPage::clearDebugConsole() {
    debugConsole_->clear();
    logDebugMessage("Console cleared", "INFO");
}

bool CameraPage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == videoDisplay_ && event->type() == QEvent::Resize) {
        // Find and resize the loading overlay to match the video display
        QWidget* loadingOverlay = videoDisplay_->findChild<QWidget*>("loadingOverlay");
        if (loadingOverlay) {
            loadingOverlay->setGeometry(videoDisplay_->rect());
        }
        return false; // Don't stop event propagation
    }
    return QWidget::eventFilter(watched, event);
}

void CameraPage::refreshCameras() {
    logDebugMessage("Refreshing cameras from external request...", "ACTION");
    // Scan for cameras
    cameraManager_->scanForCameras();
    // Then update the UI
    updateCameraList();
}

} // namespace cam_matrix::ui
