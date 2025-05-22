#pragma once

#include "core/camera_manager.hpp"
#include "ui/widgets/video_display_widget.hpp"
#include <QObject>
#include <QPushButton>
#include <QListWidget>
#include <QProgressBar>
#include <QLabel>
#include <memory>

namespace cam_matrix::ui
{

    class MultiCameraController : public QObject
    {
        Q_OBJECT

    public:
        explicit MultiCameraController(QObject *parent = nullptr);
        ~MultiCameraController() override;

        void initialize(
            std::shared_ptr<core::CameraManager> cameraManager,
            VideoDisplayWidget *videoDisplay,
            QListWidget *cameraList,
            QPushButton *toggleSelectButton,
            QPushButton *clearSelectionButton,
            QPushButton *connectSelectedButton,
            QPushButton *disconnectSelectedButton,
            QPushButton *captureSyncButton,
            QProgressBar *syncProgressBar,
            QLabel *syncStatusLabel);

        void setVisible(bool visible);
        void refreshCameraList();
        void updateUI();
        bool areAllCamerasSelected() const;

    public slots:
        void onToggleSelectAll();
        void onClearSelection();
        void onConnectSelectedCameras();
        void onDisconnectSelectedCameras();
        void onCaptureSync();
        void onCameraSelectionChanged(QListWidgetItem *item);
        void onSyncCaptureProgress(int current, int total);
        void onSyncCaptureComplete(int successCount, int total);

    signals:
        void logMessage(const QString &message, const QString &type = "INFO");

    private:
        std::shared_ptr<core::CameraManager> cameraManager_;
        VideoDisplayWidget *videoDisplay_ = nullptr;
        QListWidget *cameraList_ = nullptr;
        QPushButton *toggleSelectButton_ = nullptr;
        QPushButton *clearSelectionButton_ = nullptr;
        QPushButton *connectSelectedButton_ = nullptr;
        QPushButton *disconnectSelectedButton_ = nullptr;
        QPushButton *captureSyncButton_ = nullptr;
        QProgressBar *syncProgressBar_ = nullptr;
        QLabel *syncStatusLabel_ = nullptr;
    };

} // namespace cam_matrix::ui