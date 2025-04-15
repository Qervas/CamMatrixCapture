#include "camera_control_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>

namespace cam_matrix::ui {

CameraControlWidget::CameraControlWidget(QWidget* parent)
    : QWidget(parent)
    , exposureGroup_(nullptr)
    , exposureSlider_(nullptr)
    , gainGroup_(nullptr)
    , gainSlider_(nullptr)
    , formatGroup_(nullptr)
    , formatCombo_(nullptr)
    , autoExposureCheck_(nullptr)
    , captureGroup_(nullptr)
    , captureButton_(nullptr)
    , currentCameraIndex_(-1)
{
    setupUi();
    createConnections();
}

void CameraControlWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Exposure control
    exposureGroup_ = new QGroupBox(tr("Exposure"), this);
    auto* exposureLayout = new QVBoxLayout(exposureGroup_);

    autoExposureCheck_ = new QCheckBox(tr("Auto Exposure"), this);
    exposureLayout->addWidget(autoExposureCheck_);

    exposureSlider_ = new QSlider(Qt::Horizontal, this);
    exposureSlider_->setRange(1, 1000);
    exposureSlider_->setValue(100);
    exposureLayout->addWidget(exposureSlider_);

    auto* exposureLabel = new QLabel(tr("Value: 100 ms"), this);
    exposureLayout->addWidget(exposureLabel);

    mainLayout->addWidget(exposureGroup_);

    // Gain control
    gainGroup_ = new QGroupBox(tr("Gain"), this);
    auto* gainLayout = new QVBoxLayout(gainGroup_);

    gainSlider_ = new QSlider(Qt::Horizontal, this);
    gainSlider_->setRange(0, 100);
    gainSlider_->setValue(50);
    gainLayout->addWidget(gainSlider_);

    auto* gainLabel = new QLabel(tr("Value: 50%"), this);
    gainLayout->addWidget(gainLabel);

    mainLayout->addWidget(gainGroup_);

    // Format control
    formatGroup_ = new QGroupBox(tr("Format"), this);
    auto* formatLayout = new QVBoxLayout(formatGroup_);

    formatCombo_ = new QComboBox(this);
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
    auto* captureLayout = new QVBoxLayout(captureGroup_);
    
    captureButton_ = new QPushButton(tr("Capture Photo"), this);
    captureButton_->setEnabled(false); // Initially disabled until camera connected
    captureLayout->addWidget(captureButton_);
    
    mainLayout->addWidget(captureGroup_);

    // Add stretch at the end
    mainLayout->addStretch();
}

void CameraControlWidget::createConnections()
{
    connect(exposureSlider_, &QSlider::valueChanged,
            this, &CameraControlWidget::onExposureChanged);

    connect(gainSlider_, &QSlider::valueChanged,
            this, &CameraControlWidget::onGainChanged);

    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraControlWidget::onFormatChanged);

    connect(autoExposureCheck_, &QCheckBox::toggled,
            [this](bool checked) {
                exposureSlider_->setEnabled(!checked);
                emit statusChanged(tr("Auto exposure %1").arg(checked ? "enabled" : "disabled"));
            });
            
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

void CameraControlWidget::onExposureChanged(int value)
{
    if (currentCameraIndex_ < 0)
        return;

    emit controlValueChanged("exposure", value);
    emit statusChanged(tr("Exposure set to %1 ms").arg(value));
}

void CameraControlWidget::onGainChanged(int value)
{
    if (currentCameraIndex_ < 0)
        return;

    emit controlValueChanged("gain", value);
    emit statusChanged(tr("Gain set to %1%").arg(value));
}

void CameraControlWidget::onFormatChanged(int index)
{
    if (currentCameraIndex_ < 0)
        return;

    QString format = formatCombo_->itemText(index);
    emit controlValueChanged("format", index);
    emit statusChanged(tr("Format changed to %1").arg(format));
}

void CameraControlWidget::onCapturePhotoClicked()
{
    if (currentCameraIndex_ < 0)
        return;
        
    emit statusChanged(tr("Capturing photo from camera %1...").arg(currentCameraIndex_));
    emit capturePhotoRequested(currentCameraIndex_);
}

} // namespace cam_matrix::ui
