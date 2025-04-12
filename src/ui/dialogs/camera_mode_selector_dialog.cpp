#include "ui/dialogs/camera_mode_selector_dialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QButtonGroup>

namespace cam_matrix::ui {

CameraModeSelector::CameraModeSelector(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle("Select Camera Mode");
    resize(400, 300);
}

void CameraModeSelector::setupUi() {
    auto* layout = new QVBoxLayout(this);

    // Mode selection group
    auto* modeGroup = new QGroupBox("Camera Mode", this);
    auto* modeLayout = new QVBoxLayout(modeGroup);

    mockRadio_ = new QRadioButton("Mock Cameras (Single Thread Test)", modeGroup);
    simulatedRadio_ = new QRadioButton("Simulated Camera Matrix (Multi-Thread Test)", modeGroup);
    saperaRadio_ = new QRadioButton("Real Cameras (Sapera SDK)", modeGroup);

    mockRadio_->setChecked(true);

    auto* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(mockRadio_, Mode_Mock);
    buttonGroup->addButton(simulatedRadio_, Mode_Simulated);
    buttonGroup->addButton(saperaRadio_, Mode_Sapera);

    modeLayout->addWidget(mockRadio_);
    modeLayout->addWidget(simulatedRadio_);
    modeLayout->addWidget(saperaRadio_);

    layout->addWidget(modeGroup);

    // Simulation parameters
    simulationGroup_ = new QGroupBox("Simulation Parameters", this);
    auto* simLayout = new QGridLayout(simulationGroup_);

    simLayout->addWidget(new QLabel("Number of Cameras:"), 0, 0);
    cameraCountSpin_ = new QSpinBox(simulationGroup_);
    cameraCountSpin_->setRange(1, 16);
    cameraCountSpin_->setValue(cameraCount_);
    simLayout->addWidget(cameraCountSpin_, 0, 1);

    simLayout->addWidget(new QLabel("Frame Rate (FPS):"), 1, 0);
    frameRateSpin_ = new QSpinBox(simulationGroup_);
    frameRateSpin_->setRange(1, 120);
    frameRateSpin_->setValue(frameRate_);
    simLayout->addWidget(frameRateSpin_, 1, 1);

    synchronizedCheck_ = new QCheckBox("Synchronized Cameras", simulationGroup_);
    synchronizedCheck_->setChecked(synchronizedMode_);
    simLayout->addWidget(synchronizedCheck_, 2, 0, 1, 2);

    simLayout->addWidget(new QLabel("Simulated Jitter (ms):"), 3, 0);
    jitterSpin_ = new QSpinBox(simulationGroup_);
    jitterSpin_->setRange(0, 100);
    jitterSpin_->setValue(jitterMs_);
    simLayout->addWidget(jitterSpin_, 3, 1);

    layout->addWidget(simulationGroup_);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    okButton_ = new QPushButton("OK", this);
    cancelButton_ = new QPushButton("Cancel", this);
    buttonLayout->addWidget(okButton_);
    buttonLayout->addWidget(cancelButton_);
    layout->addLayout(buttonLayout);

    // Connect signals
    connect(buttonGroup, &QButtonGroup::idClicked,
            [this](int id) {
                selectedMode_ = static_cast<CameraMode>(id);
                updateControlState();
            });

    connect(okButton_, &QPushButton::clicked, this, &CameraModeSelector::onAccept);
    connect(cancelButton_, &QPushButton::clicked, this, &QDialog::reject);

    // Initialize state
    updateControlState();
}

void CameraModeSelector::updateControlState() {
    // Only enable simulation parameters for simulated mode
    simulationGroup_->setEnabled(selectedMode_ == Mode_Simulated);
}

void CameraModeSelector::onModeSelected() {
    updateControlState();
}

void CameraModeSelector::onAccept() {
    // Save parameters
    cameraCount_ = cameraCountSpin_->value();
    frameRate_ = frameRateSpin_->value();
    synchronizedMode_ = synchronizedCheck_->isChecked();
    jitterMs_ = jitterSpin_->value();

    accept();
}

} // namespace cam_matrix::ui
