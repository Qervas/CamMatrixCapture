#include "ui/widgets/sapera_status_widget.hpp"
#include <QMessageBox>
#include <vector>

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
    , autoRefreshTimer_(nullptr)
    , isSaperaConnected_(false)
    , isGigeConnected_(false)
{
    setupUi();
    refresh(); // Only refresh status, don't show dialog

    // Set up auto-refresh timer (check every 5 seconds)
    autoRefreshTimer_ = new QTimer(this);
    connect(autoRefreshTimer_, &QTimer::timeout, this, &SaperaStatusWidget::refresh);
    autoRefreshTimer_->start(5000);
}

void SaperaStatusWidget::setupUi()
{
    auto* layout = new QGridLayout(this);
    
    // Title label
    titleLabel_ = new QLabel(tr("Camera SDK Status"), this);
    QFont titleFont = titleLabel_->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel_->setFont(titleFont);
    layout->addWidget(titleLabel_, 0, 0, 1, 2);
    
    // Status labels
    QLabel* saperaTextLabel = new QLabel(tr("Sapera SDK:"), this);
    saperaStatusLabel_ = new QLabel(tr("Checking..."), this);
    layout->addWidget(saperaTextLabel, 1, 0);
    layout->addWidget(saperaStatusLabel_, 1, 1);
    
    QLabel* saperaVersionTextLabel = new QLabel(tr("Sapera Version:"), this);
    saperaVersionLabel_ = new QLabel(tr("N/A"), this);
    layout->addWidget(saperaVersionTextLabel, 2, 0);
    layout->addWidget(saperaVersionLabel_, 2, 1);

    QLabel* gigeTextLabel = new QLabel(tr("GigE Vision:"), this);
    gigeStatusLabel_ = new QLabel(tr("Checking..."), this);
    layout->addWidget(gigeTextLabel, 3, 0);
    layout->addWidget(gigeStatusLabel_, 3, 1);
    
    QLabel* gigeVersionTextLabel = new QLabel(tr("GigE Version:"), this);
    gigeVersionLabel_ = new QLabel(tr("N/A"), this);
    layout->addWidget(gigeVersionTextLabel, 4, 0);
    layout->addWidget(gigeVersionLabel_, 4, 1);
    
    QLabel* cameraCountTextLabel = new QLabel(tr("Available Cameras:"), this);
    cameraCountLabel_ = new QLabel(tr("0"), this);
    layout->addWidget(cameraCountTextLabel, 5, 0);
    layout->addWidget(cameraCountLabel_, 5, 1);
    
    // Buttons
    refreshButton_ = new QPushButton(tr("Refresh"), this);
    testConnectionButton_ = new QPushButton(tr("Test Connection"), this);
    
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(refreshButton_);
    buttonLayout->addWidget(testConnectionButton_);
    layout->addLayout(buttonLayout, 6, 0, 1, 2);
    
    // Connect signals
    connect(refreshButton_, &QPushButton::clicked, this, &SaperaStatusWidget::refresh);
    connect(testConnectionButton_, &QPushButton::clicked, this, &SaperaStatusWidget::showStatusDetails);
    
    // Set fixed width for labels
    saperaTextLabel->setFixedWidth(150);
    saperaVersionTextLabel->setFixedWidth(150);
    gigeTextLabel->setFixedWidth(150);
    gigeVersionTextLabel->setFixedWidth(150);
    cameraCountTextLabel->setFixedWidth(150);
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

} // namespace cam_matrix::ui 