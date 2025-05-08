#include "ui/widgets/sapera_status_widget.hpp"
#include <QMessageBox>
#include <vector>
#include <QIcon>
#include <QApplication>

namespace cam_matrix::ui {

SaperaStatusWidget::SaperaStatusWidget(QWidget* parent)
    : QWidget(parent)
    , titleLabel_(nullptr)
    , saperaStatusLabel_(nullptr)
    , saperaVersionLabel_(nullptr)
    , gigeStatusLabel_(nullptr)
    , gigeVersionLabel_(nullptr)
    , cameraCountLabel_(nullptr)
    , refreshButton_(nullptr)
    , testConnectionButton_(nullptr)
    , toggleButton_(nullptr)
    , contentContainer_(nullptr)
    , autoRefreshTimer_(nullptr)
    , isSaperaConnected_(false)
    , isGigeConnected_(false)
    , isExpanded_(false)
{
    setupUi();
    refresh(); // Only refresh status, don't show dialog

    // Set up auto-refresh timer (check every 5 seconds)
    autoRefreshTimer_ = new QTimer(this);
    connect(autoRefreshTimer_, &QTimer::timeout, this, &SaperaStatusWidget::refresh);
    autoRefreshTimer_->start(5000);
    
    // Initially collapsed
    setExpanded(false);
}

void SaperaStatusWidget::setupUi()
{
    // Get system palette for theme-aware colors
    QPalette systemPalette = QApplication::palette();
    bool isDarkTheme = systemPalette.color(QPalette::Window).lightness() < 128;
    
    // Theme-aware colors
    QString accentColor = "#007AFF";         // Apple-inspired blue
    QString accentColorHover = "#0069D9";    // Slightly darker when hovering
    QString accentColorPressed = "#0062CC";  // Even darker when pressed
    
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);
    
    // Header with title and toggle button
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    
    // Title label
    titleLabel_ = new QLabel(tr("Camera SDK Status"), this);
    QFont titleFont = titleLabel_->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel_->setFont(titleFont);
    
    // Toggle button
    toggleButton_ = new QPushButton(this);
    toggleButton_->setIcon(QIcon::fromTheme("go-down"));
    toggleButton_->setIconSize(QSize(16, 16));
    toggleButton_->setFixedSize(28, 28);
    toggleButton_->setCheckable(true);
    toggleButton_->setStyleSheet(QString(
        "QPushButton { background-color: %1; border-radius: 14px; }"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
    ).arg(accentColor, accentColorHover, accentColorPressed));
    
    headerLayout->addWidget(titleLabel_);
    headerLayout->addStretch();
    headerLayout->addWidget(toggleButton_);
    
    mainLayout->addLayout(headerLayout);
    
    // Content container - will be shown/hidden
    contentContainer_ = new QWidget(this);
    auto* contentLayout = new QGridLayout(contentContainer_);
    contentLayout->setContentsMargins(8, 8, 8, 8);
    contentLayout->setVerticalSpacing(6);
    contentLayout->setHorizontalSpacing(12);
    
    // Status labels
    QLabel* saperaTextLabel = new QLabel(tr("Sapera SDK:"), contentContainer_);
    saperaStatusLabel_ = new QLabel(tr("Checking..."), contentContainer_);
    contentLayout->addWidget(saperaTextLabel, 0, 0);
    contentLayout->addWidget(saperaStatusLabel_, 0, 1);
    
    QLabel* saperaVersionTextLabel = new QLabel(tr("Sapera Version:"), contentContainer_);
    saperaVersionLabel_ = new QLabel(tr("N/A"), contentContainer_);
    contentLayout->addWidget(saperaVersionTextLabel, 1, 0);
    contentLayout->addWidget(saperaVersionLabel_, 1, 1);

    QLabel* gigeTextLabel = new QLabel(tr("GigE Vision:"), contentContainer_);
    gigeStatusLabel_ = new QLabel(tr("Checking..."), contentContainer_);
    contentLayout->addWidget(gigeTextLabel, 2, 0);
    contentLayout->addWidget(gigeStatusLabel_, 2, 1);
    
    QLabel* gigeVersionTextLabel = new QLabel(tr("GigE Version:"), contentContainer_);
    gigeVersionLabel_ = new QLabel(tr("N/A"), contentContainer_);
    contentLayout->addWidget(gigeVersionTextLabel, 3, 0);
    contentLayout->addWidget(gigeVersionLabel_, 3, 1);
    
    QLabel* cameraCountTextLabel = new QLabel(tr("Available Cameras:"), contentContainer_);
    cameraCountLabel_ = new QLabel(tr("0"), contentContainer_);
    contentLayout->addWidget(cameraCountTextLabel, 4, 0);
    contentLayout->addWidget(cameraCountLabel_, 4, 1);
    
    // Buttons
    refreshButton_ = new QPushButton(tr("Refresh"), contentContainer_);
    testConnectionButton_ = new QPushButton(tr("Test Connection"), contentContainer_);
    
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(refreshButton_);
    buttonLayout->addWidget(testConnectionButton_);
    contentLayout->addLayout(buttonLayout, 5, 0, 1, 2);
    
    // Add the content container to the main layout
    mainLayout->addWidget(contentContainer_);
    
    // Connect signals
    connect(refreshButton_, &QPushButton::clicked, this, &SaperaStatusWidget::refresh);
    connect(testConnectionButton_, &QPushButton::clicked, this, &SaperaStatusWidget::showStatusDetails);
    connect(toggleButton_, &QPushButton::toggled, this, &SaperaStatusWidget::setExpanded);
    
    // Set fixed width for labels
    saperaTextLabel->setFixedWidth(130);
    saperaVersionTextLabel->setFixedWidth(130);
    gigeTextLabel->setFixedWidth(130);
    gigeVersionTextLabel->setFixedWidth(130);
    cameraCountTextLabel->setFixedWidth(130);
}

void SaperaStatusWidget::refresh()
{
    bool saperaAvailable = core::SaperaUtils::isSaperaAvailable();
    
    // Update Sapera SDK status
    if (saperaAvailable) {
        saperaStatusLabel_->setText("Available");
        saperaStatusLabel_->setStyleSheet("color: green;");
        isSaperaConnected_ = true;
        
        // Update Sapera version
        std::string versionStr = core::SaperaUtils::getSaperaVersion();
        saperaVersionLabel_->setText(QString::fromStdString(versionStr));
    } else {
        saperaStatusLabel_->setText("Not Available");
        saperaStatusLabel_->setStyleSheet("color: red;");
        saperaVersionLabel_->setText("N/A");
        isSaperaConnected_ = false;
    }
    
    // Update GigE Vision status
    bool gigeAvailable = core::SaperaUtils::isGigeVisionAvailable();
    if (gigeAvailable) {
        gigeStatusLabel_->setText("Available");
        gigeStatusLabel_->setStyleSheet("color: green;");
        isGigeConnected_ = true;
        
        // Update GigE Vision version
        std::string gigeVersionStr = core::SaperaUtils::getGigeVisionVersion();
        gigeVersionLabel_->setText(QString::fromStdString(gigeVersionStr));
    } else {
        gigeStatusLabel_->setText("Not Available");
        gigeStatusLabel_->setStyleSheet("color: red;");
        gigeVersionLabel_->setText("N/A");
        isGigeConnected_ = false;
    }
    
    // Get camera count
    std::vector<std::string> cameraNames;
    if (core::SaperaUtils::getAvailableCameras(cameraNames)) {
        cameraCountLabel_->setText(QString::number(cameraNames.size()));
    } else {
        cameraCountLabel_->setText("0");
    }
    
    // Enable/disable test button
    testConnectionButton_->setEnabled(saperaAvailable || gigeAvailable);
}

void SaperaStatusWidget::showStatusDetails()
{
    // Show connection test dialog based on what's available
    if (isSaperaConnected_) {
        QMessageBox::information(this, tr("Sapera Connection Test"),
            tr("Sapera SDK is connected and working properly.\nVersion: %1")
            .arg(QString::fromStdString(core::SaperaUtils::getSaperaVersion())));
    } else if (isGigeConnected_) {
        QMessageBox::information(this, tr("GigE Vision Connection Test"),
            tr("GigE Vision Interface is connected and working properly.\nVersion: %1")
            .arg(QString::fromStdString(core::SaperaUtils::getGigeVisionVersion())));
    } else {
        QMessageBox::warning(this, tr("Camera SDK Connection Test"),
            tr("Neither Sapera SDK nor GigE Vision Interface is available."));
    }
    
    // Emit status change
    emit statusChanged("Camera SDK details viewed");
}

void SaperaStatusWidget::setExpanded(bool expanded)
{
    if (isExpanded_ == expanded)
        return;

    isExpanded_ = expanded;
    contentContainer_->setVisible(expanded);
    toggleButton_->setIcon(expanded ? QIcon::fromTheme("go-up") : QIcon::fromTheme("go-down"));
    
    emit expandedChanged(expanded);
}

bool SaperaStatusWidget::isExpanded() const
{
    return isExpanded_;
}

} // namespace cam_matrix::ui 