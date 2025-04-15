#include "ui/widgets/sapera_status_widget.hpp"
#include <QMessageBox>
#include <vector>

namespace cam_matrix::ui {

SaperaStatusWidget::SaperaStatusWidget(QWidget* parent)
    : QWidget(parent)
    , titleLabel_(nullptr)
    , statusLabel_(nullptr)
    , versionLabel_(nullptr)
    , cameraCountLabel_(nullptr)
    , refreshButton_(nullptr)
    , testConnectionButton_(nullptr)
    , autoRefreshTimer_(nullptr)
    , isConnected_(false)
{
    setupUi();
    updateStatus();

    // Set up auto-refresh timer (check every 5 seconds)
    autoRefreshTimer_ = new QTimer(this);
    connect(autoRefreshTimer_, &QTimer::timeout, this, &SaperaStatusWidget::updateStatus);
    autoRefreshTimer_->start(5000);
}

void SaperaStatusWidget::setupUi()
{
    auto* layout = new QGridLayout(this);
    
    // Title label
    titleLabel_ = new QLabel(tr("Sapera SDK Status"), this);
    QFont titleFont = titleLabel_->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel_->setFont(titleFont);
    layout->addWidget(titleLabel_, 0, 0, 1, 2);
    
    // Status labels
    QLabel* statusTextLabel = new QLabel(tr("Status:"), this);
    statusLabel_ = new QLabel(tr("Checking..."), this);
    layout->addWidget(statusTextLabel, 1, 0);
    layout->addWidget(statusLabel_, 1, 1);
    
    QLabel* versionTextLabel = new QLabel(tr("Version:"), this);
    versionLabel_ = new QLabel(tr("N/A"), this);
    layout->addWidget(versionTextLabel, 2, 0);
    layout->addWidget(versionLabel_, 2, 1);
    
    QLabel* cameraCountTextLabel = new QLabel(tr("Available Cameras:"), this);
    cameraCountLabel_ = new QLabel(tr("0"), this);
    layout->addWidget(cameraCountTextLabel, 3, 0);
    layout->addWidget(cameraCountLabel_, 3, 1);
    
    // Buttons
    refreshButton_ = new QPushButton(tr("Refresh"), this);
    testConnectionButton_ = new QPushButton(tr("Test Connection"), this);
    
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(refreshButton_);
    buttonLayout->addWidget(testConnectionButton_);
    layout->addLayout(buttonLayout, 4, 0, 1, 2);
    
    // Connect signals
    connect(refreshButton_, &QPushButton::clicked, this, &SaperaStatusWidget::refresh);
    connect(testConnectionButton_, &QPushButton::clicked, this, [this]() {
        if (isConnected_) {
            QMessageBox::information(this, tr("Sapera Connection Test"),
                tr("Sapera SDK is connected and working properly.\nVersion: %1")
                .arg(QString::fromStdString(core::SaperaUtils::getSaperaVersion())));
        } else {
            QMessageBox::warning(this, tr("Sapera Connection Test"),
                tr("Sapera SDK is not available or not initialized properly."));
        }
    });
    
    // Set fixed width for labels
    statusTextLabel->setFixedWidth(150);
    versionTextLabel->setFixedWidth(150);
    cameraCountTextLabel->setFixedWidth(150);
}

void SaperaStatusWidget::refresh()
{
    updateStatus();
    emit statusChanged(tr("Sapera status refreshed"));
}

void SaperaStatusWidget::updateStatus()
{
    // Check if Sapera SDK is available
    isConnected_ = core::SaperaUtils::isSaperaAvailable();
    
    if (isConnected_) {
        statusLabel_->setText(tr("Available"));
        statusLabel_->setStyleSheet("color: green;");
        
        // Get version
        std::string versionStr = core::SaperaUtils::getSaperaVersion();
        versionLabel_->setText(QString::fromStdString(versionStr));
        
        // Get camera count
        std::vector<std::string> cameras;
        core::SaperaUtils::getAvailableCameras(cameras);
        cameraCountLabel_->setText(QString::number(cameras.size()));
        
        testConnectionButton_->setEnabled(true);
    } else {
        statusLabel_->setText(tr("Not Available"));
        statusLabel_->setStyleSheet("color: red;");
        versionLabel_->setText(tr("N/A"));
        cameraCountLabel_->setText("0");
        testConnectionButton_->setEnabled(false);
    }
}

} // namespace cam_matrix::ui 