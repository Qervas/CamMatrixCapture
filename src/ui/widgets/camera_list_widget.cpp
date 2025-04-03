#include "camera_list_widget.hpp"
#include <QVBoxLayout>

namespace cam_matrix::ui {

CameraListWidget::CameraListWidget(QWidget* parent)
    : QWidget(parent)
    , listWidget_(nullptr)
{
    setupUi();
    createConnections();
}

CameraListWidget::~CameraListWidget() = default;

void CameraListWidget::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    listWidget_ = new QListWidget(this);
    layout->addWidget(listWidget_);
}

void CameraListWidget::createConnections()
{
    connect(listWidget_, &QListWidget::currentRowChanged,
            this, &CameraListWidget::cameraSelected);

    connect(listWidget_, &QListWidget::itemSelectionChanged,
            this, &CameraListWidget::onItemSelectionChanged);
}

void CameraListWidget::setCameras(const std::vector<std::shared_ptr<core::Camera>>& cameras)
{
    cameras_ = cameras;
    listWidget_->clear();

    int index = 0;
    for (const auto& camera : cameras) {
        // In a real implementation, we'd use camera properties
        listWidget_->addItem(QString("Camera %1").arg(index++));
    }
}

std::shared_ptr<core::Camera> CameraListWidget::selectedCamera() const
{
    int row = listWidget_->currentRow();
    if (row >= 0 && row < static_cast<int>(cameras_.size()))
        return cameras_[row];

    return nullptr;
}

void CameraListWidget::onItemSelectionChanged()
{
    int row = listWidget_->currentRow();
    if (row >= 0)
        emit cameraStatusChanged(QString("Camera %1 selected").arg(row));
}

void CameraListWidget::updateCameraStatus(int index, const QString& status)
{
    if (index >= 0 && index < listWidget_->count()) {
        QListWidgetItem* item = listWidget_->item(index);
        if (item) {
            item->setToolTip(status);
            // You might want to update the item's text or icon as well
        }

        emit cameraStatusChanged(status);
    }
}

} // namespace cam_matrix::ui
