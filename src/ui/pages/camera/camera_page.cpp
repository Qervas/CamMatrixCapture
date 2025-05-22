#include "camera_page.hpp"
#include "ui/widgets/video_display_widget.hpp"
#include "ui/widgets/camera_control_widget.hpp"
#include "ui/dialogs/photo_preview_dialog.hpp"
#include "core/camera_manager.hpp"
#include "core/settings.hpp"
#include "core/sapera/sapera_camera_real.hpp"
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
#include <QTime>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QMimeData>
#include <QPainter>

namespace cam_matrix::ui
{

    // Custom delegate for nicer list items
    class CameraItemDelegate : public QStyledItemDelegate
    {
    public:
        CameraItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);

            // Theme-aware selection background
            if (opt.state & QStyle::State_Selected)
            {
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

        QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            QSize size = QStyledItemDelegate::sizeHint(option, index);
            return QSize(size.width(), 36); // Taller row height
        }
    };

    CameraPage::CameraPage(QWidget *parent)
        : Page(parent),
          cameraManager_(std::make_unique<core::CameraManager>()),
          selectedCameraIndex_(-1)
    {
        setupUi();
        createConnections();
        initialize();
    }

    CameraPage::~CameraPage()
    {
        cleanup();
    }

    void CameraPage::setupUi()
    {
        // Get system palette for theme-aware colors
        QPalette systemPalette = QApplication::palette();
        bool isDarkTheme = systemPalette.color(QPalette::Window).lightness() < 128;

        // Modern color palette
        QString accentColor = "#007AFF";        // Apple-inspired blue
        QString accentColorHover = "#0069D9";   // Slightly darker when hovering
        QString accentColorPressed = "#0062CC"; // Even darker when pressed
        QString successColor = "#34C759";       // Green for success states
        QString warningColor = "#FF9500";       // Orange for warnings
        QString errorColor = "#FF3B30";         // Red for errors

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

        QPushButton *singleCamMode = new QPushButton("Single Camera");
        singleCamMode->setCheckable(true);
        singleCamMode->setChecked(true);
        singleCamMode->setStyleSheet(QString(
                                         "QPushButton { background: %1; color: white; border: none; border-top-left-radius: 6px; border-bottom-left-radius: 6px; padding: 10px; }"
                                         "QPushButton:checked { background: %2; }"
                                         "QPushButton:hover:!checked { background: %3; }")
                                         .arg(bgDark, accentColor, accentColorHover));

        QPushButton *multiCamMode = new QPushButton("Multi Camera");
        multiCamMode->setCheckable(true);
        multiCamMode->setStyleSheet(QString(
                                        "QPushButton { background: %1; color: white; border: none; border-top-right-radius: 6px; border-bottom-right-radius: 6px; padding: 10px; }"
                                        "QPushButton:checked { background: %2; }"
                                        "QPushButton:hover:!checked { background: %3; }")
                                        .arg(bgDark, accentColor, accentColorHover));

        modeSelectionLayout->addWidget(singleCamMode);
        modeSelectionLayout->addWidget(multiCamMode);

        // Connect mode buttons
        connect(singleCamMode, &QPushButton::clicked, [this, multiCamMode]()
                {
        // Show single camera view
        if (syncGroup_) syncGroup_->setVisible(false);
        if (cameraControl_) cameraControl_->setVisible(true);
        multiCamMode->setChecked(false); });

        connect(multiCamMode, &QPushButton::clicked, [this, singleCamMode]()
                {
        // Show multi camera view
        if (syncGroup_) syncGroup_->setVisible(true);
        if (cameraControl_) cameraControl_->setVisible(false);
        singleCamMode->setChecked(false); });

        // Camera list in a clean card
        auto cameraListCard = new QWidget();
        cameraListCard->setStyleSheet(QString(
                                          "QWidget { background-color: %1; border-radius: 8px; }")
                                          .arg(bgDark));

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
                                          "QPushButton:pressed { background-color: %3; }")
                                          .arg(accentColor, accentColorHover, accentColorPressed));

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
                                       "QListWidget::item:hover:!selected { background-color: rgba(60, 60, 67, 0.1); }")
                                       .arg(accentColor));

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
                                          "QPushButton:disabled { background-color: #A0A0A0; }")
                                          .arg(accentColor, accentColorHover, accentColorPressed));

        disconnectButton_ = new QPushButton("Disconnect");
        disconnectButton_->setCursor(Qt::PointingHandCursor);
        disconnectButton_->setStyleSheet(QString(
                                             "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 0; }"
                                             "QPushButton:hover { background-color: %2; }"
                                             "QPushButton:pressed { background-color: %3; }"
                                             "QPushButton:disabled { background-color: #A0A0A0; }")
                                             .arg(errorColor, "#FF5144", "#FF6259"));

        connectionLayout->addWidget(connectButton_);
        connectionLayout->addWidget(disconnectButton_);

        cameraListLayout->addLayout(connectionLayout);

        // Camera control widget
        cameraControl_ = new CameraControlWidget();
        cameraControl_->setStyleSheet(QString(
                                          "QWidget { background-color: %1; border-radius: 8px; padding: 4px; }")
                                          .arg(bgDark));

        // Multi-camera sync group
        syncGroup_ = new QWidget();
        syncGroup_->setVisible(false); // Hidden by default
        syncGroup_->setStyleSheet(QString(
                                      "QWidget { background-color: %1; border-radius: 8px; }")
                                      .arg(bgDark));

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
                                               "QPushButton:pressed { background-color: %3; }")
                                               .arg(accentColor, accentColorHover, accentColorPressed));

        clearSelectionButton_ = new QPushButton("Clear");
        clearSelectionButton_->setStyleSheet(QString(
            "QPushButton { background-color: #8E8E93; color: white; border-radius: 6px; padding: 8px 0; }"
            "QPushButton:hover { background-color: #7A7A81; }"
            "QPushButton:pressed { background-color: #68686E; }"));

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
                                                  "QPushButton:disabled { background-color: #A0A0A0; }")
                                                  .arg(accentColor, accentColorHover, accentColorPressed));

        disconnectSelectedButton_ = new QPushButton("Disconnect Selected");
        disconnectSelectedButton_->setStyleSheet(QString(
                                                     "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 0; }"
                                                     "QPushButton:hover { background-color: %2; }"
                                                     "QPushButton:pressed { background-color: %3; }"
                                                     "QPushButton:disabled { background-color: #A0A0A0; }")
                                                     .arg(errorColor, "#FF5144", "#FF6259"));

        syncConnectionLayout->addWidget(connectSelectedButton_);
        syncConnectionLayout->addWidget(disconnectSelectedButton_);

        syncLayout->addLayout(syncConnectionLayout);

        // Capture sync button
        captureSyncButton_ = new QPushButton("Capture Synchronized Photos");
        captureSyncButton_->setStyleSheet(QString(
                                              "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 12px 0; font-size: 13px; font-weight: bold; }"
                                              "QPushButton:hover { background-color: %2; }"
                                              "QPushButton:pressed { background-color: %3; }"
                                              "QPushButton:disabled { background-color: #A0A0A0; }")
                                              .arg(successColor, "#2EB250", "#29A147"));

        syncLayout->addWidget(captureSyncButton_);

        // Progress bar for sync operations
        syncProgressBar_ = new QProgressBar();
        syncProgressBar_->setRange(0, 100);
        syncProgressBar_->setValue(0);
        syncProgressBar_->setTextVisible(false);
        syncProgressBar_->setStyleSheet(QString(
                                            "QProgressBar { background-color: %1; border-radius: 4px; height: 8px; }"
                                            "QProgressBar::chunk { background-color: %2; border-radius: 4px; }")
                                            .arg(bgMedium, successColor));

        syncLayout->addWidget(syncProgressBar_);

        syncStatusLabel_ = new QLabel("");
        syncStatusLabel_->setAlignment(Qt::AlignCenter);
        syncLayout->addWidget(syncStatusLabel_);

        // Create a container for debug sections
        debugSectionsContainer_ = new QWidget();
        auto debugSectionsLayout = new QVBoxLayout(debugSectionsContainer_);
        debugSectionsLayout->setContentsMargins(0, 10, 0, 0);
        debugSectionsLayout->setSpacing(10);

        // Status and debug info in collapsible section
        auto statusSection = new QWidget();
        statusSection->setStyleSheet(QString(
                                         "QWidget { background-color: %1; border-radius: 8px; }")
                                         .arg(bgDark));

        auto statusLayout = new QVBoxLayout(statusSection);
        statusLayout->setContentsMargins(15, 15, 15, 15);
        statusLayout->setSpacing(12);

        // Collapsible debug console header
        auto debugHeader = new QHBoxLayout();
        auto debugLabel = new QLabel("Debug Console");
        debugLabel->setStyleSheet("font-weight: bold; font-size: 15px;");

        // Toggle button for debug console
        QPushButton *toggleDebugButton = new QPushButton();
        toggleDebugButton->setIcon(QIcon::fromTheme("go-down"));
        toggleDebugButton->setIconSize(QSize(16, 16));
        toggleDebugButton->setFixedSize(28, 28);
        toggleDebugButton->setCheckable(true);
        toggleDebugButton->setStyleSheet(QString(
                                             "QPushButton { background-color: %1; border-radius: 14px; }"
                                             "QPushButton:hover { background-color: %2; }"
                                             "QPushButton:pressed { background-color: %3; }")
                                             .arg(accentColor, accentColorHover, accentColorPressed));

        debugHeader->addWidget(debugLabel);
        debugHeader->addStretch();
        debugHeader->addWidget(toggleDebugButton);

        statusLayout->addLayout(debugHeader);

        // Debug console
        debugConsole_ = new QPlainTextEdit();
        debugConsole_->setReadOnly(true);
        debugConsole_->setMaximumHeight(150);
        debugConsole_->setStyleSheet(QString(
                                         "QPlainTextEdit { background-color: %1; border: none; border-radius: 6px; color: %2; font-family: 'SF Mono', Consolas, 'Liberation Mono', Menlo, monospace; font-size: 11px; padding: 8px; }")
                                         .arg(bgMedium, textColor));

        // Container for debug console to control visibility
        QWidget *debugContainer = new QWidget();
        auto debugContainerLayout = new QVBoxLayout(debugContainer);
        debugContainerLayout->setContentsMargins(0, 0, 0, 0);
        debugContainerLayout->addWidget(debugConsole_);

        // Clear console button
        clearConsoleButton_ = new QPushButton("Clear Console");
        clearConsoleButton_->setStyleSheet(QString(
                                               "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 6px 0; font-size: 12px; }"
                                               "QPushButton:hover { background-color: %2; }"
                                               "QPushButton:pressed { background-color: %3; }")
                                               .arg(bgLight, QString::number(bgLight.mid(1).toInt(nullptr, 16) + 0x101010, 16),
                                                    QString::number(bgLight.mid(1).toInt(nullptr, 16) + 0x202020, 16)));

        debugContainerLayout->addWidget(clearConsoleButton_);

        // Hide debug console by default
        debugContainer->setVisible(false);

        // Connect toggle button
        connect(toggleDebugButton, &QPushButton::toggled, [debugContainer, toggleDebugButton](bool checked)
                {
        debugContainer->setVisible(checked);
        toggleDebugButton->setIcon(checked ? QIcon::fromTheme("go-up") : 
                                          QIcon::fromTheme("go-down")); });

        statusLayout->addWidget(debugContainer);

        // Storage path widget
        auto storageWidget = new QWidget();
        storageWidget->setStyleSheet(QString(
                                         "QWidget { background-color: %1; border-radius: 8px; }")
                                         .arg(bgDark));

        auto storageLayout = new QVBoxLayout(storageWidget);
        storageLayout->setContentsMargins(15, 15, 15, 15);
        storageLayout->setSpacing(12);

        auto storageHeader = new QHBoxLayout();
        auto storageLabel = new QLabel("Image Storage");
        storageLabel->setStyleSheet("font-weight: bold; font-size: 15px;");

        storageHeader->addWidget(storageLabel);
        storageLayout->addLayout(storageHeader);

        storagePath_ = new QLabel();
        storagePath_->setWordWrap(true);
        storagePath_->setStyleSheet(QString("color: %1; font-size: 11px;").arg(dimTextColor));

        // Update displayed path
        QString path = core::Settings::getPhotoSaveDirectory();
        storagePath_->setText(path);

        auto browseButton = new QPushButton("Change Storage Location");
        browseButton->setStyleSheet(QString(
                                        "QPushButton { background-color: %1; color: white; border-radius: 6px; padding: 8px 0; }"
                                        "QPushButton:hover { background-color: %2; }"
                                        "QPushButton:pressed { background-color: %3; }")
                                        .arg(accentColor, accentColorHover, accentColorPressed));

        connect(browseButton, &QPushButton::clicked, this, &CameraPage::onChangeStorageLocation);

        storageLayout->addWidget(storagePath_);
        storageLayout->addWidget(browseButton);

        // Add all components to the control panel
        controlLayout->addWidget(modeSelectionWidget);
        controlLayout->addWidget(cameraListCard);
        controlLayout->addWidget(cameraControl_);
        controlLayout->addWidget(syncGroup_);
        // Debug sections are now always visible
        controlLayout->addWidget(debugSectionsContainer_);
        controlLayout->addWidget(storageWidget);
        controlLayout->addStretch();

        // Main horizontal layout
        mainLayout->addWidget(feedPanel, 7);    // 70% width
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

    void CameraPage::createConnections()
    {
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
        // Debug sections are now always visible - removed toggle button connection

        // Camera manager signals
        connect(cameraManager_.get(), &core::CameraManager::cameraStatusChanged,
                this, &CameraPage::onCameraStatusChanged);
        connect(cameraManager_.get(), &core::CameraManager::managerStatusChanged,
                this, &CameraPage::onManagerStatusChanged);

        // Fix photoCaptured signal connection - use a lambda with matching signature
        // and explicitly cast the signal to the correct overload.
        connect(cameraManager_.get(),
                static_cast<void (core::CameraManager::*)(const QImage &, const QString &)>(&core::CameraManager::photoCaptured),
                this,
                [this](const QImage &img, const QString &path)
                {
                    onPhotoCaptured(img, path.toStdString());
                });

        connect(cameraManager_.get(), &core::CameraManager::syncCaptureProgress,
                this, &CameraPage::onSyncCaptureProgress);
        connect(cameraManager_.get(), &core::CameraManager::syncCaptureComplete,
                this, &CameraPage::onSyncCaptureComplete);

        // Connect newFrameAvailable signal to onNewFrame slot
        connect(cameraManager_.get(), &core::CameraManager::newFrameAvailable,
                this, &CameraPage::onNewFrame);

        // Camera control signals

        connect(cameraControl_, &CameraControlWidget::photoCaptureRequested,
                [this]()
                {
                    if (selectedCameraIndex_ >= 0)
                    {
                        onCapturePhotoRequested(selectedCameraIndex_);
                    }
                });
    }

    void CameraPage::initialize()
    {
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
        QTimer *frameTimer = new QTimer(this);
        connect(frameTimer, &QTimer::timeout, this, [this]()
                {
        try {
            // Safety checks for a valid camera index
            if (selectedCameraIndex_ < 0 || selectedCameraIndex_ >= cameraList_->count()) {
                return;
            }
            
            // First check if camera is connected according to manager
            bool isConnected = false;
            try {
                isConnected = cameraManager_->isCameraConnected(selectedCameraIndex_);
            } catch (...) {
                // Silently handle exceptions during connection check
                return;
            }
            
            if (!isConnected) {
                // Do not attempt to fetch frames from disconnected cameras
                return;
            }
            
            // Get the camera pointer safely
            SaperaCameraReal* camera = nullptr;
            try {
                camera = cameraManager_->getCameraByIndex(selectedCameraIndex_);
            } catch (...) {
                // Silently handle exceptions when getting camera pointer
                return;
            }
            
            // Verify we have a valid camera object and it reports as connected
            if (!camera || !camera->isConnected()) {
                return;
            }
            
            // Now try to get a frame
            try {
                QImage frame = camera->getLatestFrame();
                if (!frame.isNull()) {
                    videoDisplay_->updateFrame(frame);
                }
            } catch (const std::exception& e) {
                // Log exceptions during frame fetch with low frequency to avoid spam
                static QDateTime lastErrorTime = QDateTime::currentDateTime();
                if (lastErrorTime.msecsTo(QDateTime::currentDateTime()) > 5000) {  // Only log errors every 5 seconds
                    logDebugMessage(QString("Frame timer: Exception getting frame: %1").arg(e.what()), "WARNING");
                    lastErrorTime = QDateTime::currentDateTime();
                }
            } catch (...) {
                // Silently ignore other exceptions during frame fetch - don't crash the app
                // This could happen during photo capture or connection changes
            }
        } catch (...) {
            // Top level catch to ensure the timer never crashes the application
        } });
        frameTimer->start(16); // Poll at ~60Hz for smooth animation

        // Setup a watchdog timer that checks every 2 seconds to ensure the camera is still connected
        QTimer *watchdogTimer = new QTimer(this);
        connect(watchdogTimer, &QTimer::timeout, this, [this]()
                {
            try {
                // Only proceed if we have a valid selected camera index
                if (selectedCameraIndex_ < 0) {
                    return;
                }
                
                // Verify the camera index is within bounds
                if (selectedCameraIndex_ >= cameraList_->count()) {
                    logDebugMessage("Watchdog: Invalid camera index", "WARNING");
                    return;
                }
                
                // Check if the camera is still responsive
                bool isConnectedByManager = false;
                try {
                    isConnectedByManager = cameraManager_->isCameraConnected(selectedCameraIndex_);
                } catch (...) {
                    logDebugMessage("Watchdog: Exception checking camera connection status", "WARNING");
                    return;
                }
                
                bool isEnabledInUI = disconnectButton_->isEnabled();
                
                // Only attempt to reconnect if the UI shows connected but manager shows disconnected
                // This fixes the issue where watchdog would trigger redundant connection attempts
                if (!isConnectedByManager && isEnabledInUI) {
                    logDebugMessage("Watchdog: Camera connection potentially lost (UI/manager mismatch), attempting to reconnect...", "WARNING");
                    
                    // Instead of calling onConnectCamera directly (which can cause double connection issues), 
                    // update the UI state first and let the user decide if they want to reconnect
                    disconnectButton_->setEnabled(false);
                    connectButton_->setEnabled(true);
                    cameraControl_->setEnabled(false);
                    videoDisplay_->clear();
                    
                    logDebugMessage("Watchdog: Please click Connect to reconnect camera if needed.", "HINT");
                    return; // Exit early to avoid trying to get frames
                }
                
                // Only try to get a frame if the manager reports that the camera is connected
                if (!isConnectedByManager) {
                    return;
                }
                
                // Send a "keepalive" signal/operation to the camera to prevent timeout
                try {
                    auto* camera = cameraManager_->getCameraByIndex(selectedCameraIndex_);
                    if (!camera) {
                        logDebugMessage("Watchdog: Camera pointer is null despite being reported as connected", "WARNING");
                        return;
                    }
                    
                    // Verify camera reports itself as connected before trying to get a frame
                    if (!camera->isConnected()) {
                        logDebugMessage("Watchdog: Camera reports as not connected despite manager saying it is", "WARNING");
                        return;
                    }
                    
                    // Get a frame to keep the connection active, but don't log every ping
                    // to reduce debug console spam
                    camera->getLatestFrame();
                } catch (const std::exception& e) {
                    logDebugMessage(QString("Watchdog: Exception during frame fetch: %1").arg(e.what()), "WARNING");
                } catch (...) {
                    logDebugMessage("Watchdog: Unknown exception during frame fetch", "WARNING");
                }
            } catch (...) {
                logDebugMessage("Watchdog: Unhandled exception in watchdog timer", "ERROR");
            } });
        watchdogTimer->start(5000); // Check every 5 seconds
    }

    void CameraPage::cleanup()
    {
        // Keep a strong reference to parent window to prevent deletion
        QWidget *parentWindow = window();

        // Clear selected camera index to prevent frame timer from accessing disconnected camera
        selectedCameraIndex_ = -1;

        // Disconnect cameras and save settings
        cameraManager_->disconnectAllCameras();
        saveSettings();
        logDebugMessage("Camera page cleanup completed", "INFO");

        // Ensure the main window stays visible
        if (parentWindow)
        {
            parentWindow->activateWindow();
        }
    }

    void CameraPage::onRefreshCameras()
    {
        logDebugMessage("Refreshing camera list...");
        updateCameraList();
    }

    void CameraPage::onConnectCamera()
    {
        int index = cameraList_->currentRow();
        if (index >= 0)
        {
            logDebugMessage(QString("Connecting to camera at index %1...").arg(index));

            // Get the name of the camera for better logging
            QString cameraName = cameraList_->item(index)->text();
            logDebugMessage(QString("Connecting to camera: %1").arg(cameraName));

            // Show loading overlay
            if (loadingOverlay_)
            {
                loadingOverlay_->show();
                QApplication::processEvents(); // Ensure overlay appears immediately
            }

            // Connect the camera
            bool success = cameraManager_->connectCamera(index);

            // Hide loading overlay
            if (loadingOverlay_)
            {
                loadingOverlay_->hide();
            }

            if (success)
            {
                logDebugMessage(QString("Successfully connected to camera: %1").arg(cameraName), "SUCCESS");

                // Update UI state immediately
                disconnectButton_->setEnabled(true);
                cameraControl_->setEnabled(true);

                // Automatically select this camera for sync operations
                QListWidgetItem *item = cameraList_->item(index);
                if (item)
                {
                    item->setCheckState(Qt::Checked);
                    cameraManager_->selectCameraForSync(index, true);
                }

                // Get the camera object directly and request an initial frame
                try
                {
                    auto *camera = cameraManager_->getCameraByIndex(index);
                    if (camera && camera->isConnected())
                    {
                        // Give a small delay to allow the camera to start streaming
                        QApplication::processEvents();

                        // Try to get a current frame
                        try
                        {
                            QImage currentFrame = camera->getLatestFrame();
                            if (!currentFrame.isNull())
                            {
                                videoDisplay_->updateFrame(currentFrame);
                                logDebugMessage("Updated video display with initial frame");
                            }
                        }
                        catch (const std::exception &e)
                        {
                            logDebugMessage(QString("Exception when getting initial frame: %1").arg(e.what()), "WARNING");
                        }
                        catch (...)
                        {
                            logDebugMessage("Unknown exception when getting initial frame", "WARNING");
                        }
                    }
                }
                catch (...)
                {
                    logDebugMessage("Exception when accessing camera after connecting", "WARNING");
                }

                // Make sure to update UI after all changes
                QApplication::processEvents();

                // Update sync UI to reflect selection changes
                updateSyncUI();
            }
            else
            {
                logDebugMessage(QString("Failed to connect to camera: %1").arg(cameraName), "ERROR");
            }
        }
    }

    void CameraPage::onDisconnectCamera()
    {
        int index = cameraList_->currentRow();
        if (index >= 0)
        {
            logDebugMessage(QString("Disconnecting camera at index %1...").arg(index));

            // Update UI state immediately to prevent double-clicks or further interactions
            disconnectButton_->setEnabled(false);
            connectButton_->setEnabled(false); // Will be enabled once disconnection is fully complete
            cameraControl_->setEnabled(false);

            // Clear selected camera index to prevent frame timer from accessing it during disconnect
            int previousSelectedIndex = selectedCameraIndex_;
            selectedCameraIndex_ = -1;

            // Show loading overlay
            if (loadingOverlay_)
            {
                loadingOverlay_->show();
                QApplication::processEvents(); // Ensure overlay appears immediately
            }

            // Clear display before disconnect to avoid accessing a disconnecting camera
            videoDisplay_->clear();

            // Process events to ensure UI updates before disconnecting
            QApplication::processEvents();

            // Keep a strong reference to parent window to prevent deletion
            QWidget *parentWindow = window();

            // Disconnect camera using a short QTimer to allow the UI to update first
            QTimer::singleShot(50, this, [this, index, previousSelectedIndex, parentWindow]()
                               {
                // Use try-catch to handle any exceptions
                try {
                    bool success = cameraManager_->disconnectCamera(index);
                    
                    // Update UI after disconnection
                    QListWidgetItem *item = cameraList_->item(index);
                    if (item) {
                        item->setCheckState(Qt::Unchecked);
                        item->setIcon(QIcon::fromTheme("network-offline"));
                        item->setForeground(QBrush(QApplication::palette().text()));
                    }
                    
                    // Hide loading overlay with a brief delay to ensure disconnect completed
                    QTimer::singleShot(100, this, [this, index, success, parentWindow]() {
                        if (loadingOverlay_) {
                            loadingOverlay_->hide();
                        }
                        
                        // Re-enable the connect button
                        connectButton_->setEnabled(true);
                        
                        // Log result
                        if (success) {
                            logDebugMessage(QString("Camera %1 successfully disconnected").arg(index), "SUCCESS");
                        } else {
                            logDebugMessage(QString("Camera %1 disconnect may have had issues").arg(index), "WARNING");
                        }
                        
                        // Update sync UI after reconnecting the camera item
                        updateSyncUI();
                        
                        // Ensure the main window stays visible
                        if (parentWindow) {
                            parentWindow->activateWindow();
                        }
                    });
                }
                catch (...) {
                    // Handle exceptions during disconnect
                    logDebugMessage("Exception during camera disconnect", "ERROR");
                    if (loadingOverlay_) {
                        loadingOverlay_->hide();
                    }
                    connectButton_->setEnabled(true);
                    
                    // Restore selected camera index
                    selectedCameraIndex_ = previousSelectedIndex;
                    
                    // Ensure the main window stays visible
                    if (parentWindow) {
                        parentWindow->activateWindow();
                    }
                } });
        }
    }

    void CameraPage::onCameraSelected(int index)
    {
        selectedCameraIndex_ = index;
        bool cameraSelected = (index >= 0);
        // auto* camera = cameraSelected ? cameraManager_->getCameraByIndex(index) : nullptr; // camera ptr already obtained if needed

        connectButton_->setEnabled(cameraSelected && !cameraManager_->isCameraConnected(index));
        disconnectButton_->setEnabled(cameraSelected && cameraManager_->isCameraConnected(index));
        cameraControl_->setEnabled(cameraSelected && cameraManager_->isCameraConnected(index));
        cameraControl_->setCameraIndex(cameraSelected && cameraManager_->isCameraConnected(index) ? index : -1);

        if (cameraSelected)
        {
            logDebugMessage(QString("Camera selected: %1").arg(cameraList_->item(index)->text()));

            if (cameraManager_->isCameraConnected(index))
            {
                auto *camera = cameraManager_->getCameraByIndex(index);
                if (camera)
                {
                    QImage currentFrame = camera->getLatestFrame();
                    if (!currentFrame.isNull())
                    {
                        videoDisplay_->updateFrame(currentFrame);
                        logDebugMessage("Updated video display with current frame on selection");
                    }
                }
            }
            else
            {
                videoDisplay_->clear(); // Clear display if selected camera is not connected
                logDebugMessage("Selected camera not connected - controls updated, display cleared");
            }
        }
        else
        {
            videoDisplay_->clear(); // Clear display if no camera is selected
        }
    }

    void CameraPage::onCameraStatusChanged(const QString &status)
    {
        logDebugMessage(QString("Camera status: %1").arg(status));

        // Instead of calling updateCameraList() which can cause unwanted selection changes,
        // just update the UI state for the currently selected camera

        // Check if we have a valid camera selection
        if (selectedCameraIndex_ >= 0 && selectedCameraIndex_ < cameraList_->count())
        {
            QListWidgetItem *item = cameraList_->item(selectedCameraIndex_);
            if (!item)
                return;

            bool isConnected = cameraManager_->isCameraConnected(selectedCameraIndex_);

            // Update item appearance
            if (isConnected)
            {
                item->setIcon(QIcon::fromTheme("network-wired"));
                item->setForeground(QBrush(QColor(46, 139, 87))); // SeaGreen
                item->setCheckState(Qt::Checked);
            }
            else
            {
                item->setIcon(QIcon::fromTheme("network-offline"));
                item->setCheckState(Qt::Unchecked);
                item->setForeground(QBrush(QApplication::palette().text()));
            }

            // Update control buttons
            connectButton_->setEnabled(!isConnected);
            disconnectButton_->setEnabled(isConnected);
            cameraControl_->setEnabled(isConnected);
            cameraControl_->setCameraIndex(isConnected ? selectedCameraIndex_ : -1);

            // Update video display if needed
            if (!isConnected)
            {
                videoDisplay_->clear();
            }

            // Update sync UI
            updateSyncUI();
        }
    }

    void CameraPage::onManagerStatusChanged(const QString &status)
    {
        logDebugMessage(QString("Manager status: %1").arg(status));
    }

    void CameraPage::onCapturePhotoRequested(int cameraIndex)
    {
        if (cameraIndex >= 0 && cameraManager_->isCameraConnected(cameraIndex))
        {
            // Auto-generate filepath to bypass file dialog completely
            QString timeStamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
            QString basePath = core::Settings::getPhotoSaveDirectory();
            QString fileName = QString("%1/capture_%2.png").arg(basePath).arg(timeStamp);

            // Make sure the directory exists
            QDir dir(basePath);
            if (!dir.exists())
            {
                dir.mkpath(".");
                logDebugMessage(QString("Created directory %1").arg(basePath), "INFO");
            }

            // Send empty path to trigger the auto-save in SaperaCameraReal::capturePhoto
            logDebugMessage(QString("Capturing photo from camera %1 (auto-save)").arg(cameraIndex), "ACTION");
            bool success = cameraManager_->capturePhoto(cameraIndex, "");

            if (success)
            {
                logDebugMessage(QString("Photo capture initiated successfully"), "SUCCESS");
            }
            else
            {
                logDebugMessage(QString("Photo capture failed"), "ERROR");
            }
        }
        else
        {
            logDebugMessage(QString("Cannot capture - camera %1 not connected").arg(cameraIndex), "ERROR");
        }
    }

    void CameraPage::onPhotoCaptured(const QImage &image, const std::string &path)
    {
        logDebugMessage(QString("Photo captured to %1").arg(QString::fromStdString(path)), "SUCCESS");

        // Don't show preview dialog if image is null
        if (image.isNull())
        {
            logDebugMessage("Received null image - skipping preview dialog", "WARNING");
            return;
        }

        try
        {
            // Create dialog on heap and use deleteLater for safer cleanup
            PhotoPreviewDialog *dialog = new PhotoPreviewDialog(image, QString::fromStdString(path), this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setModal(true);
            dialog->show();
        }
        catch (const std::exception &e)
        {
            logDebugMessage(QString("Error showing preview dialog: %1").arg(e.what()), "ERROR");
        }
        catch (...)
        {
            logDebugMessage("Unknown error showing preview dialog", "ERROR");
        }
    }

    void CameraPage::loadSettings()
    {
        QSettings settings;
        QVariant savePath = settings.value("camera/savePath");
        if (savePath.isNull())
        {
            settings.setValue("camera/savePath", QDir::homePath());
        }
    }

    void CameraPage::saveSettings()
    {
        // Save any settings if needed
    }

    void CameraPage::updateCameraList()
    {
        // Block signals temporarily to prevent selection change signals during list update
        bool oldState = cameraList_->signalsBlocked();
        cameraList_->blockSignals(true);

        cameraList_->clear();

        std::vector<std::string> cameras = cameraManager_->getAvailableCameras();

        // If no cameras found, try to scan for cameras again
        if (cameras.empty())
        {
            logDebugMessage("No cameras found, trying to scan again...", "WARNING");
            cameraManager_->scanForCameras();
            cameras = cameraManager_->getAvailableCameras();
        }

        for (size_t i = 0; i < cameras.size(); ++i)
        {
            QString cameraName = QString::fromStdString(cameras[i]);
            QListWidgetItem *item = new QListWidgetItem(cameraName);

            bool isActuallyConnected = cameraManager_->isCameraConnected(i);

            if (isActuallyConnected)
            {
                item->setIcon(QIcon::fromTheme("network-wired"));
                item->setForeground(QBrush(QColor(46, 139, 87))); // SeaGreen
                item->setCheckState(Qt::Checked);
            }
            else
            {
                item->setIcon(QIcon::fromTheme("network-offline"));
                item->setCheckState(Qt::Unchecked);
            }
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            cameraList_->addItem(item);
        }

        logDebugMessage(QString("Found %1 cameras").arg(cameras.size()));

        // Update UI based on selected camera, but avoid auto-selecting the first camera
        // which could trigger unwanted connect operations
        if (selectedCameraIndex_ >= 0 && selectedCameraIndex_ < cameraList_->count())
        {
            cameraList_->setCurrentRow(selectedCameraIndex_);

            // Update button states manually since we're blocking signals
            connectButton_->setEnabled(!cameraManager_->isCameraConnected(selectedCameraIndex_));
            disconnectButton_->setEnabled(cameraManager_->isCameraConnected(selectedCameraIndex_));
            cameraControl_->setEnabled(cameraManager_->isCameraConnected(selectedCameraIndex_));
            cameraControl_->setCameraIndex(cameraManager_->isCameraConnected(selectedCameraIndex_) ? selectedCameraIndex_ : -1);
        }
        else
        {
            // Don't auto-select any camera if none was selected before
            // This prevents the issue where "System" camera gets auto-selected
            selectedCameraIndex_ = -1;
            connectButton_->setEnabled(false);
            disconnectButton_->setEnabled(false);
            cameraControl_->setEnabled(false);
        }

        updateSyncUI();

        // Restore original signal blocking state
        cameraList_->blockSignals(oldState);
    }

    // Multi-camera synchronization methods
    void CameraPage::onCameraSelectionChanged(QListWidgetItem *item)
    {
        // Find the index of the changed item
        int itemIndex = cameraList_->row(item);
        if (itemIndex >= 0)
        {
            bool selected = item->checkState() == Qt::Checked;
            // Update the camera manager's selection state
            cameraManager_->selectCameraForSync(itemIndex, selected);
            logDebugMessage(QString("Camera %1 %2 for synchronized capture")
                                .arg(itemIndex)
                                .arg(selected ? "selected" : "deselected"));
        }

        updateSyncUI();
    }

    void CameraPage::onConnectSelectedCameras()
    {
        std::vector<int> selectedIndices;
        for (int i = 0; i < cameraList_->count(); ++i)
        {
            if (cameraList_->item(i)->checkState() == Qt::Checked)
            {
                selectedIndices.push_back(i);
                // Ensure the camera is selected in the manager
                cameraManager_->selectCameraForSync(i, true);
            }
        }

        if (!selectedIndices.empty())
        {
            logDebugMessage(QString("Connecting %1 selected cameras...").arg(selectedIndices.size()));
            // Use the camera manager's functionality to connect selected cameras
            if (cameraManager_->connectSelectedCameras())
            {
                logDebugMessage("All selected cameras connected successfully", "SUCCESS");
            }
            else
            {
                logDebugMessage("Some cameras failed to connect", "WARNING");
            }
            updateCameraList(); // Update UI to reflect new connection status
        }
    }

    void CameraPage::onDisconnectSelectedCameras()
    {
        std::vector<int> selectedIndices;
        for (int i = 0; i < cameraList_->count(); ++i)
        {
            if (cameraList_->item(i)->checkState() == Qt::Checked)
            {
                selectedIndices.push_back(i);
                // Ensure the camera is selected in the manager
                cameraManager_->selectCameraForSync(i, true);
            }
        }

        if (!selectedIndices.empty())
        {
            logDebugMessage(QString("Disconnecting %1 selected cameras...").arg(selectedIndices.size()));

            // Update UI state and clear selected camera index
            int previousSelectedIndex = selectedCameraIndex_;
            selectedCameraIndex_ = -1;

            // Show loading overlay
            if (loadingOverlay_)
            {
                loadingOverlay_->show();
                QApplication::processEvents(); // Ensure overlay appears immediately
            }

            // Clear display before disconnect
            videoDisplay_->clear();

            // Keep a strong reference to parent window to prevent deletion
            QWidget *parentWindow = window();

            // Use a timer to allow UI to update first
            QTimer::singleShot(50, this, [this, selectedIndices, previousSelectedIndex, parentWindow]()
                               {
                try {
                    // Use the camera manager's functionality to disconnect selected cameras
                    bool success = cameraManager_->disconnectSelectedCameras();
                    
                    // Hide loading overlay with a brief delay
                    QTimer::singleShot(100, this, [this, success, previousSelectedIndex, parentWindow]() {
                        if (loadingOverlay_) {
                            loadingOverlay_->hide();
                        }
                        
                        if (success) {
                            logDebugMessage("All selected cameras disconnected successfully", "SUCCESS");
                        } else {
                            logDebugMessage("Some cameras failed to disconnect", "WARNING");
                        }
                        
                        updateCameraList(); // Update UI to reflect new connection status
                        
                        // Ensure the main window stays visible
                        if (parentWindow) {
                            parentWindow->activateWindow();
                        }
                    });
                }
                catch (...) {
                    // Handle exceptions during disconnect
                    logDebugMessage("Exception during camera disconnect", "ERROR");
                    if (loadingOverlay_) {
                        loadingOverlay_->hide();
                    }
                    
                    // Restore selected camera index
                    selectedCameraIndex_ = previousSelectedIndex;
                    
                    // Ensure the main window stays visible
                    if (parentWindow) {
                        parentWindow->activateWindow();
                    }
                } });
        }
    }

    void CameraPage::onCaptureSync()
    {
        QString timeStamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        QString basePath = core::Settings::getPhotoSaveDirectory();
        QString dirPath = basePath + "/sync_" + timeStamp;

        QDir dir;
        if (!dir.exists(dirPath))
        {
            dir.mkpath(dirPath);
        }

        logDebugMessage(QString("Starting synchronized capture to %1").arg(dirPath), "ACTION");
        syncProgressBar_->setValue(0);
        syncProgressBar_->show();

        // Call the capturePhotosSync method
        bool success = cameraManager_->capturePhotosSync(dirPath);

        if (!success)
        {
            logDebugMessage("Failed to start synchronized capture", "ERROR");
            syncProgressBar_->hide();
        }
    }

    void CameraPage::onClearSelection()
    {
        for (int i = 0; i < cameraList_->count(); ++i)
        {
            cameraList_->item(i)->setCheckState(Qt::Unchecked);
            // Make sure to deselect in the camera manager as well
            cameraManager_->selectCameraForSync(i, false);
        }

        // Clear all selections in the manager
        cameraManager_->clearCameraSelection();

        updateSyncUI();
        logDebugMessage("Cleared camera selection");
    }

    void CameraPage::onToggleSelectAll()
    {
        bool allSelected = areAllCamerasSelected();

        // First clear all selections in the manager if we're toggling all to selected
        if (!allSelected)
        {
            cameraManager_->clearCameraSelection();
        }

        for (int i = 0; i < cameraList_->count(); ++i)
        {
            bool newState = !allSelected;
            cameraList_->item(i)->setCheckState(newState ? Qt::Checked : Qt::Unchecked);
            // Update selection in camera manager
            cameraManager_->selectCameraForSync(i, newState);
        }

        updateSyncUI();
        logDebugMessage(allSelected ? "Deselected all cameras" : "Selected all cameras");
    }

    void CameraPage::onSyncCaptureProgress(int current, int total)
    {
        syncProgressBar_->setValue(current);
        syncStatusLabel_->setText(tr("Capturing %1 of %2...").arg(current).arg(total));
        logDebugMessage(QString("Sync capture progress: %1/%2").arg(current).arg(total));
    }

    void CameraPage::onSyncCaptureComplete(int successCount, int total)
    {
        syncProgressBar_->setValue(total);
        syncStatusLabel_->setText(tr("Captured %1 of %2 successfully").arg(successCount).arg(total));

        if (successCount == total)
        {
            logDebugMessage(QString("Synchronized capture completed successfully: %1 images").arg(total), "SUCCESS");
        }
        else
        {
            logDebugMessage(QString("Synchronized capture completed with issues: %1/%2 successful")
                                .arg(successCount)
                                .arg(total),
                            "WARNING");
        }

        QTimer::singleShot(3000, [this]()
                           {
        syncProgressBar_->hide();
        syncStatusLabel_->clear(); });
    }

    void CameraPage::updateSyncUI()
    {
        int selectedCount = 0;
        int connectedSelectedCount = 0;

        for (int i = 0; i < cameraList_->count(); ++i)
        {
            if (cameraList_->item(i)->checkState() == Qt::Checked)
            {
                selectedCount++;
                if (cameraManager_->isCameraConnected(i))
                {
                    connectedSelectedCount++;
                }
            }
        }

        connectSelectedButton_->setEnabled(selectedCount > 0);
        disconnectSelectedButton_->setEnabled(connectedSelectedCount > 0);
        captureSyncButton_->setEnabled(connectedSelectedCount > 1);

        toggleSelectButton_->setText(areAllCamerasSelected() ? tr("Deselect All") : tr("Select All"));
    }

    bool CameraPage::areAllCamerasSelected() const
    {
        for (int i = 0; i < cameraList_->count(); ++i)
        {
            if (cameraList_->item(i)->checkState() != Qt::Checked)
            {
                return false;
            }
        }
        return cameraList_->count() > 0;
    }

    // Debug console methods
    void CameraPage::logDebugMessage(const QString &message, const QString &type)
    {
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

        if (type == "INFO")
        {
            formattedMessage = QString("<span style='color:%1'>[%2] [INFO] %3</span>")
                                   .arg(infoColor, timestamp, message);
        }
        else if (type == "WARNING")
        {
            formattedMessage = QString("<span style='color:%1'>[%2] [WARN] %3</span>")
                                   .arg(warningColor, timestamp, message);
        }
        else if (type == "ERROR")
        {
            formattedMessage = QString("<span style='color:%1'>[%2] [ERROR] %3</span>")
                                   .arg(errorColor, timestamp, message);
        }
        else if (type == "SUCCESS")
        {
            formattedMessage = QString("<span style='color:%1'>[%2] [SUCCESS] %3</span>")
                                   .arg(successColor, timestamp, message);
        }
        else if (type == "ACTION")
        {
            formattedMessage = QString("<span style='color:%1'>[%2] [ACTION] %3</span>")
                                   .arg(actionColor, timestamp, message);
        }
        else if (type == "HINT")
        {
            formattedMessage = QString("<span style='color:%1'>[%2] [HINT] %3</span>")
                                   .arg(hintColor, timestamp, message);
        }
        else
        {
            formattedMessage = QString("<span style='color:%1'>[%2] [%3] %4</span>")
                                   .arg(infoColor, timestamp, type, message);
        }

        debugConsole_->appendHtml(formattedMessage);

        // Scroll to bottom
        QScrollBar *scrollBar = debugConsole_->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }

    void CameraPage::clearDebugConsole()
    {
        debugConsole_->clear();
        logDebugMessage("Console cleared", "INFO");
    }

    bool CameraPage::eventFilter(QObject *watched, QEvent *event)
    {
        if (watched == videoDisplay_ && event->type() == QEvent::Resize)
        {
            // Find and resize the loading overlay to match the video display
            QWidget *loadingOverlay = videoDisplay_->findChild<QWidget *>("loadingOverlay");
            if (loadingOverlay)
            {
                loadingOverlay->setGeometry(videoDisplay_->rect());
            }
            return false; // Don't stop event propagation
        }
        return QWidget::eventFilter(watched, event);
    }

    void CameraPage::refreshCameras()
    {
        logDebugMessage("Refreshing cameras from external request...", "ACTION");
        // Scan for cameras
        cameraManager_->scanForCameras();
        // Then update the UI
        updateCameraList();
    }

    void CameraPage::onChangeStorageLocation()
    {
        QString newPath = QFileDialog::getExistingDirectory(this, tr("Select New Storage Location"),
                                                            core::Settings::getPhotoSaveDirectory());
        if (!newPath.isEmpty())
        {
            core::Settings::setPhotoSaveDirectory(newPath);
            storagePath_->setText(newPath);
            logDebugMessage(QString("Storage location changed to %1").arg(newPath), "INFO");
        }
    }

    void CameraPage::onToggleDebugSections(bool checked)
    {
        areDebugSectionsVisible_ = !checked;

        if (debugSectionsContainer_)
        {
            debugSectionsContainer_->setVisible(areDebugSectionsVisible_);
        }

        // Update the button text
        if (checked)
        {
            toggleDebugSectionsButton_->setText("Show Debug Sections");
        }
        else
        {
            toggleDebugSectionsButton_->setText("Hide Debug Sections");
        }

        logDebugMessage(QString("Debug sections %1").arg(areDebugSectionsVisible_ ? "shown" : "hidden"));
    }

    // Add the implementation for onNewFrame
    void CameraPage::onNewFrame(const QImage &frame)
    {
        logDebugMessage(QString("CameraPage::onNewFrame received frame.isNull: %1, size: %2x%3")
                            .arg(frame.isNull())
                            .arg(frame.width())
                            .arg(frame.height()),
                        "DEBUG");
        if (videoDisplay_)
        {
            videoDisplay_->updateFrame(frame);
        }
    }

} // namespace cam_matrix::ui
