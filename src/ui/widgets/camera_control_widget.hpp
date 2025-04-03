#pragma once

#include <QWidget>
#include <QGroupBox>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>

namespace cam_matrix::ui {

class CameraControlWidget : public QWidget {
    Q_OBJECT

public:
    explicit CameraControlWidget(QWidget* parent = nullptr);
    ~CameraControlWidget() = default;

    void setCameraIndex(int index);

signals:
    void controlValueChanged(const QString& controlName, int value);
    void statusChanged(const QString& message);

private slots:
    void onExposureChanged(int value);
    void onGainChanged(int value);
    void onFormatChanged(int index);

private:
    void setupUi();
    void createConnections();

    QGroupBox* exposureGroup_;
    QSlider* exposureSlider_;

    QGroupBox* gainGroup_;
    QSlider* gainSlider_;

    QGroupBox* formatGroup_;
    QComboBox* formatCombo_;

    QCheckBox* autoExposureCheck_;

    int currentCameraIndex_;
};

} // namespace cam_matrix::ui
