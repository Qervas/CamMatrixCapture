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
    
    // Main layout with a clean aesthetic
    auto mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Left panel for camera feed - takes 70% of the width
    auto feedPanel = new QWidget();
    feedPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    auto feedLayout = new QVBoxLayout(feedPanel);
    feedLayout->setContentsMargins(20, 20, 20, 20);
    
    videoDisplay_ = new VideoDisplayWidget();
    videoDisplay_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoDisplay_->setStyleSheet("background-color: black; border-radius: 8px;");
    feedLayout->addWidget(videoDisplay_);
    
    // Right sidebar for camera controls - takes 30% of the width
    auto controlPanel = new QWidget();
    controlPanel->setFixedWidth(350);
    controlPanel->setStyleSheet(QString("background-color: %1;").arg(bgMedium));
    
    auto controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(20, 20, 20, 20);
    controlLayout->setSpacing(16);
    
    // Mode selection segmented control
    auto modeSelectionWidget = new QWidget();
    auto modeSelectionLayout = new QHBoxLayout(modeSelectionWidget);
    modeSelectionLayout->setContentsMargins(0, 0, 0, 0);
    modeSelectionLayout->setSpacing(0);
    
    QPushButton* singleCamMode = new QPushButton("Single Camera");
    singleCamMode->setCheckable(true);
    singleCamMode->setChecked(true);
    singleCamMode->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none; border-top-left-radius: 6px; border-bottom-left-radius: 6px; padding: 10px; }"
        "QPushButton:checked { background: %2; }"
        "QPushButton:hover:!checked { background: %3; }"
    ).arg(bgDark, accentColor, accentColorHover));
    
    QPushButton* multiCamMode = new QPushButton("Multi Camera");
    multiCamMode->setCheckable(true);
    multiCamMode->setStyleSheet(QString(
        "QPushButton { background: %1; color: white; border: none; border-top-right-radius: 6px; border-bottom-right-radius: 6px; padding: 10px; }"
        "QPushButton:checked { background: %2; }"
        "QPushButton:hover:!checked { background: %3; }"
    ).arg(bgDark, accentColor, accentColorHover));
    
    modeSelectionLayout->addWidget(singleCamMode);
    modeSelectionLayout->addWidget(multiCamMode);
    
    // Connect mode buttons
    connect(singleCamMode, &QPushButton::clicked, [this, multiCamMode]() {
        // Show single camera view
        if (syncGroup_) syncGroup_->setVisible(false);
        if (cameraControl_) cameraControl_->setVisible(true);
        multiCamMode->setChecked(false);
    });
    
    connect(multiCamMode, &QPushButton::clicked, [this, singleCamMode]() {
        // Show multi camera view
        if (syncGroup_) syncGroup_->setVisible(true);
        if (cameraControl_) cameraControl_->setVisible(false);
        singleCamMode->setChecked(false);
    });
    
    // Camera list in a clean card
    auto cameraListCard = new QWidget();
    cameraListCard->setStyleSheet(QString(
        "QWidget { background-color: %1; border-radius: 8px; }"
    ).arg(bgDark));
    
    auto cameraListLayout = new QVBoxLayout(cameraListCard);
    cameraListLayout->setContentsMargins(15, 15, 15, 15);
    cameraListLayout->setSpacing(12);
    
    // Camera list with refresh button
    auto cameraListHeader = new QHBoxLayout();
    auto cameraLabel = new QLabel("Cameras");
    cameraLabel->setStyleSheet("font-weight: bold; font-size: 15px;");
    
    refreshButton_ = new QPushButton();
    refreshButton_->setIcon(QIcon::fromTheme("view-refresh"));
    refreshButton_->setIconSize(QSize(16, 16));
    refreshButton_->setToolTip("Refresh Camera List");
    refreshButton_->setFixedSize(28, 28);
    refreshButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; border-radius: 14px; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
    ).arg(accentColor, accentColorHover, accentColorPressed));
    
    cameraListHeader->addWidget(cameraLabel);
    cameraListHeader->addStretch();
    cameraListHeader->addWidget(refreshButton_);
    
    cameraListLayout->addLayout(cameraListHeader);
    
    // Camera list
    cameraList_ = new QListWidget();
    cameraList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    cameraList_->setItemDelegate(new CameraItemDelegate(cameraList_));
    cameraList_->setStyleSheet(QString(
        "QListWidget { background-color: transparent; border: none; }"
        "QListWidget::item { padding: 8px; border-radius: 6px; margin-bottom: 4px; }"
        "QListWidget::item:selected { background-color: %1; color: white; }"
        "QListWidget::item:hover:!selected { background-color: rgba(60, 60, 67, 0.1); }"
    ).arg(accentColor));
    
    cameraListLayout->addWidget(cameraList_);
    
    // Connection buttons
    auto connectionLayout = new QHBoxLayout();
    connectionLayout->setSpacing(8);
    
    connectButton_ = new QPushButton("Connect");
    connectButton_->setCursor(Qt::PointingHandCursor);
    connectButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 0; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
        "QPushButton:disabled { background-color: #A0A0A0; }"
    ).arg(accentColor, accentColorHover, accentColorPressed));
    
    disconnectButton_ = new QPushButton("Disconnect");
    disconnectButton_->setCursor(Qt::PointingHandCursor);
    disconnectButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 0; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
        "QPushButton:disabled { background-color: #A0A0A0; }"
    ).arg(errorColor, "#FF5144", "#FF6259"));
    
    connectionLayout->addWidget(connectButton_);
    connectionLayout->addWidget(disconnectButton_);
    
    cameraListLayout->addLayout(connectionLayout);
    
    // Camera control widget
    cameraControl_ = new CameraControlWidget();
    cameraControl_->setStyleSheet(QString(
        "QWidget { background-color: %1; border-radius: 8px; padding: 4px; }"
    ).arg(bgDark));
    
    // Multi-camera sync group
    syncGroup_ = new QWidget();
    syncGroup_->setVisible(false); // Hidden by default
    syncGroup_->setStyleSheet(QString(
        "QWidget { background-color: %1; border-radius: 8px; }"
    ).arg(bgDark));
    
    auto syncLayout = new QVBoxLayout(syncGroup_);
    syncLayout->setContentsMargins(15, 15, 15, 15);
    syncLayout->setSpacing(12);
    
    auto syncHeader = new QHBoxLayout();
    auto syncLabel = new QLabel("Multi-Camera Control");
    syncLabel->setStyleSheet("font-weight: bold; font-size: 15px;");
    
    syncHeader->addWidget(syncLabel);
    syncLayout->addLayout(syncHeader);
    
    // Sync controls
    auto syncButtonsLayout = new QHBoxLayout();
    
    toggleSelectButton_ = new QPushButton("Select All");
    toggleSelectButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 0; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
    ).arg(accentColor, accentColorHover, accentColorPressed));
    
    clearSelectionButton_ = new QPushButton("Clear");
    clearSelectionButton_->setStyleSheet(QString(
        "QPushButton { background-color: #8E8E93; color: white; border-radius: 6px; padding: 8px 0; }"
        "QPushButton:hover { background-color: #7A7A81; }"
        "QPushButton:pressed { background-color: #68686E; }"
    ));
    
    syncButtonsLayout->addWidget(toggleSelectButton_);
    syncButtonsLayout->addWidget(clearSelectionButton_);
    
    syncLayout->addLayout(syncButtonsLayout);
    
    // Connect/disconnect selected
    auto syncConnectionLayout = new QHBoxLayout();
    
    connectSelectedButton_ = new QPushButton("Connect Selected");
    connectSelectedButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 0; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
        "QPushButton:disabled { background-color: #A0A0A0; }"
    ).arg(accentColor, accentColorHover, accentColorPressed));
    
    disconnectSelectedButton_ = new QPushButton("Disconnect Selected");
    disconnectSelectedButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 0; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
        "QPushButton:disabled { background-color: #A0A0A0; }"
    ).arg(errorColor, "#FF5144", "#FF6259"));
    
    syncConnectionLayout->addWidget(connectSelectedButton_);
    syncConnectionLayout->addWidget(disconnectSelectedButton_);
    
    syncLayout->addLayout(syncConnectionLayout);
    
    // Capture sync button
    captureSyncButton_ = new QPushButton("Capture Synchronized Photos");
    captureSyncButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 12px 0; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
        "QPushButton:disabled { background-color: #A0A0A0; }"
    ).arg(successColor, "#2EB250", "#29A147"));
    
    syncLayout->addWidget(captureSyncButton_);
    
    // Progress bar for sync operations
    syncProgressBar_ = new QProgressBar();
    syncProgressBar_->setRange(0, 100);
    syncProgressBar_->setValue(0);
    syncProgressBar_->setTextVisible(false);
    syncProgressBar_->setStyleSheet(QString(
        "QProgressBar { background-color: %1; border-radius: 4px; height: 8px; }"
        "QProgressBar::chunk { background-color: %2; border-radius: 4px; }"
    ).arg(bgMedium, successColor));
    
    syncLayout->addWidget(syncProgressBar_);
    
    syncStatusLabel_ = new QLabel("");
    syncStatusLabel_->setAlignment(Qt::AlignCenter);
    syncLayout->addWidget(syncStatusLabel_);
    
    // Status and debug info in collapsible section
    auto statusSection = new QWidget();
    statusSection->setStyleSheet(QString(
        "QWidget { background-color: %1; border-radius: 8px; }"
    ).arg(bgDark));
    
    auto statusLayout = new QVBoxLayout(statusSection);
    statusLayout->setContentsMargins(15, 15, 15, 15);
    statusLayout->setSpacing(12);
    
    // Collapsible debug console header
    auto debugHeader = new QHBoxLayout();
    auto debugLabel = new QLabel("Debug Console");
    debugLabel->setStyleSheet("font-weight: bold; font-size: 15px;");
    
    // Toggle button for debug console
    QPushButton* toggleDebugButton = new QPushButton();
    toggleDebugButton->setIcon(QIcon::fromTheme("go-down"));
    toggleDebugButton->setIconSize(QSize(16, 16));
    toggleDebugButton->setFixedSize(28, 28);
    toggleDebugButton->setCheckable(true);
    toggleDebugButton->setStyleSheet(QString(
        "QPushButton { background-color: %1; border-radius: 14px; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
    ).arg(accentColor, accentColorHover, accentColorPressed));
    
    debugHeader->addWidget(debugLabel);
    debugHeader->addStretch();
    debugHeader->addWidget(toggleDebugButton);
    
    statusLayout->addLayout(debugHeader);
    
    // Debug console
    debugConsole_ = new QPlainTextEdit();
    debugConsole_->setReadOnly(true);
    debugConsole_->setMaximumHeight(150);
    debugConsole_->setStyleSheet(QString(
        "QPlainTextEdit { background-color: %1; border: none; border-radius: 6px; color: %2; font-family: 'SF Mono', Consolas, 'Liberation Mono', Menlo, monospace; font-size: 11px; padding: 8px; }"
    ).arg(bgMedium, textColor));
    
    // Container for debug console to control visibility
    QWidget* debugContainer = new QWidget();
    auto debugContainerLayout = new QVBoxLayout(debugContainer);
    debugContainerLayout->setContentsMargins(0, 0, 0, 0);
    debugContainerLayout->addWidget(debugConsole_);
    
    // Clear console button
    clearConsoleButton_ = new QPushButton("Clear Console");
    clearConsoleButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 6px 0; font-size: 12px; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
    ).arg(bgLight, QString::number(bgLight.mid(1).toInt(nullptr, 16) + 0x101010, 16), 
          QString::number(bgLight.mid(1).toInt(nullptr, 16) + 0x202020, 16)));
    
    debugContainerLayout->addWidget(clearConsoleButton_);
    
    // Hide debug console by default
    debugContainer->setVisible(false);
    
    // Connect toggle button
    connect(toggleDebugButton, &QPushButton::toggled, [debugContainer, toggleDebugButton](bool checked) {
        debugContainer->setVisible(checked);
        toggleDebugButton->setIcon(checked ? QIcon::fromTheme("go-up") : 
                                          QIcon::fromTheme("go-down"));
    });
    
    statusLayout->addWidget(debugContainer);
    
    // Action buttons for direct camera access and testing
    auto actionButtonsLayout = new QHBoxLayout();
    
    directCameraButton_ = new QPushButton();
    directCameraButton_->setIcon(QIcon::fromTheme("camera-photo"));
    directCameraButton_->setText("Direct Camera Access");
    directCameraButton_->setIconSize(QSize(16, 16));
    directCameraButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 12px; text-align: left; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
    ).arg(bgLight, QString::number(bgLight.mid(1).toInt(nullptr, 16) + 0x101010, 16), 
          QString::number(bgLight.mid(1).toInt(nullptr, 16) + 0x202020, 16)));
    
    testSaperaButton_ = new QPushButton();
    testSaperaButton_->setIcon(QIcon::fromTheme("tools-check-spelling"));
    testSaperaButton_->setText("Test Sapera");
    testSaperaButton_->setIconSize(QSize(16, 16));
    testSaperaButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 12px; text-align: left; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
    ).arg(bgLight, QString::number(bgLight.mid(1).toInt(nullptr, 16) + 0x101010, 16), 
          QString::number(bgLight.mid(1).toInt(nullptr, 16) + 0x202020, 16)));
    
    actionButtonsLayout->addWidget(directCameraButton_);
    actionButtonsLayout->addWidget(testSaperaButton_);
    
    statusLayout->addLayout(actionButtonsLayout);
    
    // Sapera SDK status
    saperaStatus_ = new SaperaStatusWidget();
    saperaStatus_->setStyleSheet(QString(
        "QWidget { background-color: transparent; }"
    ));
    
    statusLayout->addWidget(saperaStatus_);
    
    // Add all components to the control panel
    controlLayout->addWidget(modeSelectionWidget);
    controlLayout->addWidget(cameraListCard);
    controlLayout->addWidget(cameraControl_);
    controlLayout->addWidget(syncGroup_);
    controlLayout->addWidget(statusSection);
    controlLayout->addStretch();
    
    // Main horizontal layout
    mainLayout->addWidget(feedPanel, 7);  // 70% width
    mainLayout->addWidget(controlPanel, 3); // 30% width
    
    // Initially disable buttons
    disconnectButton_->setEnabled(false);
    connectSelectedButton_->setEnabled(false);
    disconnectSelectedButton_->setEnabled(false);
    captureSyncButton_->setEnabled(false);
    
    // Create loading overlay
    loadingOverlay_ = new QWidget(this);
    loadingOverlay_->setStyleSheet("background-color: rgba(0, 0, 0, 0.7);");
    loadingOverlay_->setVisible(false);
    
    auto overlayLayout = new QVBoxLayout(loadingOverlay_);
    overlayLayout->setAlignment(Qt::AlignCenter);
    
    auto loadingLabel = new QLabel("Loading...");
    loadingLabel->setStyleSheet("color: white; font-size: 18px; font-weight: bold;");
    overlayLayout->addWidget(loadingLabel);
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
            
            logDebugMessage(QString("Setting camera controls - Format: %1")
                .arg(QString::fromStdString(format)));
                
            // Update the control values
            cameraControl_->setFormat(QString::fromStdString(format));
            
            // Force update the UI to ensure controls are enabled
            QApplication::processEvents();
            
            logDebugMessage(QString("Loaded camera settings - Format: %1")
                .arg(QString::fromStdString(format)));
                
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
