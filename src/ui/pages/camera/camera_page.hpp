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
    class SaperaStatusWidget;
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
    void onDirectCameraAccess();
    void onCapturePhotoRequested(int cameraIndex);
    void onPhotoCaptured(const QImage& image, const std::string& path);

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
    QPushButton* directCameraButton_;
    CameraControlWidget* cameraControl_;
    VideoDisplayWidget* videoDisplay_;
    SaperaStatusWidget* saperaStatus_;
    int selectedCameraIndex_;
};

} // namespace cam_matrix::ui
