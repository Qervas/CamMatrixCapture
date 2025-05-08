#include "camera_control_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QApplication>
#include <QDebug>

namespace cam_matrix::ui {

CameraControlWidget::CameraControlWidget(QWidget* parent)
    : QWidget(parent)
    , formatGroup_(nullptr)
    , formatCombo_(nullptr)
    , captureGroup_(nullptr)
    , captureButton_(nullptr)
    , currentCameraIndex_(-1)
{
    setupUi();
    createConnections();
}

void CameraControlWidget::setupUi()
{
    // Get system palette for theme-aware colors
    QPalette systemPalette = QApplication::palette();
    bool isDarkTheme = systemPalette.color(QPalette::Window).lightness() < 128;
    
    // Theme-aware colors
    QString borderColor = isDarkTheme ? "#555555" : "#cccccc";
    QString textColor = isDarkTheme ? "#e0e0e0" : "#202020";
    
    auto* mainLayout = new QVBoxLayout(this);

    // Create theme-aware group box style
    QString groupBoxStyle = QString(
        "QGroupBox { font-weight: bold; border: 1px solid %1; border-radius: 5px; margin-top: 10px; padding-top: 10px; color: %2; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
    ).arg(borderColor, textColor);
    
    // Format control
    formatGroup_ = new QGroupBox(tr("Format"), this);
    formatGroup_->setStyleSheet(groupBoxStyle);
    auto* formatLayout = new QVBoxLayout(formatGroup_);

    formatCombo_ = new QComboBox(this);
    QString comboStyle = QString(
        "QComboBox { border: 1px solid %1; border-radius: 3px; padding: 3px; color: %2; background: transparent; }"
    ).arg(borderColor, textColor);
    formatCombo_->setStyleSheet(comboStyle);
    
    formatCombo_->addItems({
        "1920x1080 (MJPEG)",
        "1280x720 (MJPEG)",
        "640x480 (MJPEG)",
        "1920x1080 (YUV)",
        "1280x720 (YUV)",
        "640x480 (YUV)"
    });
    formatLayout->addWidget(formatCombo_);

    mainLayout->addWidget(formatGroup_);
    
    // Capture photo control
    captureGroup_ = new QGroupBox(tr("Photo Capture"), this);
    captureGroup_->setStyleSheet(groupBoxStyle);
    auto* captureLayout = new QVBoxLayout(captureGroup_);
    
    // Theme-aware button style
    QString buttonBgColor = isDarkTheme ? "#444444" : "#f0f0f0";
    QString buttonStyle = QString(
        "QPushButton { background-color: %1; border: 1px solid %2; border-radius: 4px; padding: 6px 12px; color: %3; }"
    ).arg(buttonBgColor, borderColor, textColor);
    
    captureButton_ = new QPushButton(tr("Capture Photo"), this);
    captureButton_->setStyleSheet(buttonStyle);
    captureButton_->setEnabled(false); // Initially disabled until camera connected
    captureLayout->addWidget(captureButton_);
    
    mainLayout->addWidget(captureGroup_);

    // Add stretch at the end
    mainLayout->addStretch();
}

void CameraControlWidget::createConnections()
{
    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraControlWidget::onFormatChanged);
            
    connect(captureButton_, &QPushButton::clicked,
            this, &CameraControlWidget::onCapturePhotoClicked);
}

void CameraControlWidget::setCameraIndex(int index)
{
    if (currentCameraIndex_ == index)
        return;

    currentCameraIndex_ = index;
    
    // Enable capture button when a camera is selected
    captureButton_->setEnabled(index >= 0);

    // In a real implementation, we would load the camera's current settings
    // and update the UI controls accordingly
    emit statusChanged(tr("Camera %1 selected").arg(index));
}

void CameraControlWidget::onFormatChanged(int index)
{
    if (currentCameraIndex_ < 0)
        return;

    QString format = formatCombo_->itemText(index);
    emit controlValueChanged("format", index);
    emit statusChanged(tr("Format changed to %1").arg(format));
    
    // Add new signal emission
    emit formatChanged(format);
}

void CameraControlWidget::onCapturePhotoClicked()
{
    if (currentCameraIndex_ < 0)
        return;
        
    emit statusChanged(tr("Capturing photo from camera %1...").arg(currentCameraIndex_));
    emit capturePhotoRequested(currentCameraIndex_);
    emit photoCaptureRequested();
}

void CameraControlWidget::setFormat(const QString& format)
{
    int index = formatCombo_->findText(format);
    if (index >= 0 && formatCombo_->currentIndex() != index) {
        formatCombo_->setCurrentIndex(index);
    }
}

// Override the setEnabled method to ensure proper state of capture button
void CameraControlWidget::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);
    
    // Make sure the capture button is enabled appropriately
    captureButton_->setEnabled(enabled && currentCameraIndex_ >= 0);
    
    // Debug message
    qDebug() << "CameraControlWidget::setEnabled(" << enabled 
             << ") with currentCameraIndex=" << currentCameraIndex_
             << "- captureButton enabled:" << captureButton_->isEnabled();
}

} // namespace cam_matrix::ui
