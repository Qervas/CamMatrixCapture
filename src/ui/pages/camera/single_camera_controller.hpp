#pragma once

#include "core/camera_manager.hpp"
#include "ui/widgets/video_display_widget.hpp"
#include "ui/widgets/camera_control_widget.hpp"
#include <QObject>
#include <QPushButton>
#include <QListWidget>
#include <memory>

namespace cam_matrix::ui
{

    class SingleCameraController : public QObject
    {
        Q_OBJECT

    public:
        explicit SingleCameraController(QObject *parent = nullptr);
        ~SingleCameraController() override;

        void initialize(
            std::shared_ptr<core::CameraManager> cameraManager,
            VideoDisplayWidget *videoDisplay,
            CameraControlWidget *cameraControl,
            QListWidget *cameraList,
            QPushButton *connectButton,
            QPushButton *disconnectButton);

        void setVisible(bool visible);
        void onCameraSelected(int index);
        void refreshCameraList();

    public slots:
        void onConnectCamera();
        void onDisconnectCamera();
        void onPhotoCaptureRequested();

    signals:
        void logMessage(const QString &message, const QString &type = "INFO");
        void updateUI();

    private:
        std::shared_ptr<core::CameraManager> cameraManager_;
        VideoDisplayWidget *videoDisplay_ = nullptr;
        CameraControlWidget *cameraControl_ = nullptr;
        QListWidget *cameraList_ = nullptr;
        QPushButton *connectButton_ = nullptr;
        QPushButton *disconnectButton_ = nullptr;
        int selectedCameraIndex_ = -1;
    };

} // namespace cam_matrix::ui