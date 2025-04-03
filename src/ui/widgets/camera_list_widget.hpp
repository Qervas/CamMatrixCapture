#pragma once

#include <QWidget>
#include <QListWidget>
#include <vector>
#include <memory>

namespace cam_matrix::core {
class Camera;  // Forward declaration
}

namespace cam_matrix::ui {

class CameraListWidget : public QWidget {
    Q_OBJECT

public:
    explicit CameraListWidget(QWidget* parent = nullptr);
    ~CameraListWidget() override;

    void setCameras(const std::vector<std::shared_ptr<core::Camera>>& cameras);
    std::shared_ptr<core::Camera> selectedCamera() const;

signals:
    void cameraSelected(int index);
    void cameraStatusChanged(const QString& status);

private slots:
    void onItemSelectionChanged();
    void updateCameraStatus(int index, const QString& status);

private:
    void setupUi();
    void createConnections();

    QListWidget* listWidget_;
    std::vector<std::shared_ptr<core::Camera>> cameras_;
};

} // namespace cam_matrix::ui 