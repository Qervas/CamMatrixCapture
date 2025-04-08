#pragma once

#include "ui/pages/page.hpp"
#include "core/camera_manager.hpp"
#include "ui/widgets/video_display_widget.hpp"
#include <QListWidget>
#include <QPushButton>
#include <memory>

namespace cam_matrix::ui {

class CameraControlWidget;

class CameraPage : public Page {
    Q_OBJECT

public:
    explicit CameraPage(QWidget* parent = nullptr);
    ~CameraPage() override;

    QString title() const override { return tr("Cameras"); }
    void initialize() override;
    void cleanup() override;

private slots:
    void onRefreshCameras();
    void onCameraSelected(int index);
    void onCameraStatusChanged(const QString& status);
    void onConnectCamera();
    void onDisconnectCamera();
    void onNewFrame(const QImage& frame);
    void onTestSaperaCamera();


private:
    void setupUi() override;
    void createConnections() override;
    void loadSettings();
    void saveSettings();
    void updateCameraList();

    QListWidget* cameraList_;
    CameraControlWidget* cameraControl_;
    QPushButton* refreshButton_;
    QPushButton* connectButton_;
    QPushButton* disconnectButton_;
    VideoDisplayWidget* videoDisplay_;

    std::shared_ptr<core::CameraManager> cameraManager_;
    int selectedCameraIndex_;
    QPushButton* testSaperaButton_;

};

} // namespace cam_matrix::ui
