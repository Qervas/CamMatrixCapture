#include "ui/dialogs/camera_mode_selector_dialog.hpp"
#include <QDialogButtonBox>

namespace cam_matrix::ui {

CameraModeSelectorDialog::CameraModeSelectorDialog(QWidget* parent)
    : QDialog(parent)
    , modeComboBox_(nullptr)
    , okButton_(nullptr)
    , cancelButton_(nullptr)
{
    setupUi();
    populateModeComboBox();
}

void CameraModeSelectorDialog::setupUi()
{
    setWindowTitle(tr("Select Camera Mode"));
    resize(400, 150);

    auto* mainLayout = new QVBoxLayout(this);
    
    // Add label
    auto* label = new QLabel(tr("Select the camera mode:"), this);
    mainLayout->addWidget(label);
    
    // Add combo box
    modeComboBox_ = new QComboBox(this);
    mainLayout->addWidget(modeComboBox_);
    
    // Add button box
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
    
    // Store button references
    okButton_ = buttonBox->button(QDialogButtonBox::Ok);
    cancelButton_ = buttonBox->button(QDialogButtonBox::Cancel);
    
    setLayout(mainLayout);
}

void CameraModeSelectorDialog::populateModeComboBox()
{
    // Add common camera modes
    modeComboBox_->addItem(tr("Continuous"), "continuous");
    modeComboBox_->addItem(tr("Single Shot"), "single_shot");
    modeComboBox_->addItem(tr("Triggered"), "triggered");
    modeComboBox_->addItem(tr("External Trigger"), "external_trigger");
    
    // Set default selection
    modeComboBox_->setCurrentIndex(0);
}

QString CameraModeSelectorDialog::getSelectedMode() const
{
    return modeComboBox_->currentData().toString();
}

} // namespace cam_matrix::ui 