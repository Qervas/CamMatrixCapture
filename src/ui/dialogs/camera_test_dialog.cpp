#include "ui/dialogs/camera_test_dialog.hpp"

namespace cam_matrix::ui {

CameraTestDialog::CameraTestDialog(QWidget* parent)
    : QDialog(parent)
    , tester_(nullptr)
    , closeButton_(nullptr)
{
    setupUi();
    setWindowTitle("Camera Tester");
    resize(800, 600);
}

void CameraTestDialog::setupUi() {
    auto* layout = new QVBoxLayout(this);

    tester_ = new core::CameraTester(this);
    layout->addWidget(tester_);

    closeButton_ = new QPushButton("Close", this);
    layout->addWidget(closeButton_);

    connect(closeButton_, &QPushButton::clicked, this, &QDialog::accept);
}

} // namespace cam_matrix::ui
