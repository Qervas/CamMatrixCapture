#pragma once
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <memory>
#include "core/camera_manager.hpp"
#include "core/sapera_manager.hpp"
#include "ui/widgets/video_display_widget.hpp"

namespace cam_matrix::core {

class CameraTester : public QWidget {
    Q_OBJECT

public:
    explicit CameraTester(QWidget* parent = nullptr);
    ~CameraTester();

public slots:
    void refreshCameras();
    void onMockSelected(bool selected);
    void onRealSelected(bool selected);

private slots:
    void connectToSelectedCamera();
    void disconnectFromCamera();
    void onCameraSelected(int index);
    void onExposureChanged(int value);
    void onFrameReceived(const QImage& frame);

private:
    void setupUi();
    void createConnections();
    void updateCameraList();

    // Camera managers
    std::shared_ptr<CameraManager> mockManager_;
    std::shared_ptr<SaperaManager> saperaManager_;

    // UI elements
    QComboBox* cameraListCombo_;
    QPushButton* refreshButton_;
    QPushButton* connectButton_;
    QPushButton* disconnectButton_;
    QSlider* exposureSlider_;
    QLabel* exposureLabel_;
    ui::VideoDisplayWidget* videoDisplay_;
    QLabel* statusLabel_;

    // Status
    bool useMockCameras_;
    int selectedCameraIndex_;
    bool isConnected_;
};

} // namespace cam_matrix::core
