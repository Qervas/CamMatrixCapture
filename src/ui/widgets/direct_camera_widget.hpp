#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QTimer>
#include <QImage>
#include <memory>
#include <string>
#include <vector>

// Forward declare
namespace cam_matrix::core {
#if defined(SAPERA_FOUND)
    class SaperaDirectAccess;
#endif
}

namespace cam_matrix::ui {

class DirectCameraWidget : public QWidget {
    Q_OBJECT

public:
    explicit DirectCameraWidget(QWidget* parent = nullptr);
    ~DirectCameraWidget() override;

public slots:
    void refreshCameras();
    void onConnectClicked();
    void onDisconnectClicked();
    void onCameraSelected(int index);
    void onExposureChanged(int value);
    void onFormatChanged(int index);
    void onNewFrame(const QImage& frame);

signals:
    void statusChanged(const QString& status);
    void error(const QString& error);

private:
    void setupUi();
    void createConnections();
    void updateControls();

    // UI elements
    QLabel* cameraStatusLabel_;
    QComboBox* cameraComboBox_;
    QPushButton* refreshButton_;
    QPushButton* connectButton_;
    QPushButton* disconnectButton_;
    QLabel* videoFeedLabel_;
    QSlider* exposureSlider_;
    QLabel* exposureValueLabel_;
    QComboBox* formatComboBox_;
    QCheckBox* liveCheckbox_;

    // Camera access
#if defined(SAPERA_FOUND)
    std::unique_ptr<core::SaperaDirectAccess> camera_;
#endif
    bool isConnected_;
    bool isStreaming_;
    std::vector<std::string> cameraList_;
    std::vector<std::string> formatList_;
};

} // namespace cam_matrix::ui 