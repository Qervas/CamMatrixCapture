#pragma once

#include "ui/pages/page.hpp"
#include "core/camera_manager.hpp"
#include <memory>
#include <QListWidget>
#include <QPushButton>
#include <QImage>

// Forward declarations
namespace cam_matrix::ui {
    class CameraControlWidget;
    class VideoDisplayWidget;
}

namespace cam_matrix::ui {

class CameraPage : public Page {
    Q_OBJECT

public:
    explicit CameraPage(QWidget* parent = nullptr);
    ~CameraPage() override;
    
    QString title() const override { return tr("Cameras"); }

protected:
    void setupUi() override;
    void createConnections() override;
    void initialize() override;
    void cleanup() override;

private slots:
    void onRefreshCameras();
    void onConnectCamera();
    void onDisconnectCamera();
    void onCameraSelected(int index);
    void onCameraStatusChanged(const QString& status);
    void onManagerStatusChanged(const std::string& status);
    void onNewFrame(const QImage& frame);
    void onTestSaperaCamera();

private:
    void loadSettings();
    void saveSettings();
    void updateCameraList();

    std::unique_ptr<core::CameraManager> cameraManager_;
    QListWidget* cameraList_;
    QPushButton* refreshButton_;
    QPushButton* connectButton_;
    QPushButton* disconnectButton_;
    QPushButton* testSaperaButton_;
    CameraControlWidget* cameraControl_;
    VideoDisplayWidget* videoDisplay_;
    int selectedCameraIndex_;
};

} // namespace cam_matrix::ui
