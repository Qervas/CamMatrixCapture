#pragma once
#include <QDialog>
#include <QRadioButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>
#include <QComboBox>

namespace cam_matrix::ui {

class CameraModeSelector : public QDialog {
    Q_OBJECT

public:
    enum CameraMode {
        Mode_Mock,
        Mode_Simulated,
        Mode_Sapera
    };

    explicit CameraModeSelector(QWidget* parent = nullptr);
    ~CameraModeSelector() = default;

    // Get the selected mode
    CameraMode selectedMode() const { return selectedMode_; }

    // Get simulation parameters
    int getCameraCount() const { return cameraCount_; }
    int getFrameRate() const { return frameRate_; }
    bool getSynchronizedMode() const { return synchronizedMode_; }
    int getJitterMs() const { return jitterMs_; }

private slots:
    void onModeSelected();
    void onAccept();

private:
    void setupUi();
    void updateControlState();

    // Mode selection
    CameraMode selectedMode_ = Mode_Mock;
    QRadioButton* mockRadio_ = nullptr;
    QRadioButton* simulatedRadio_ = nullptr;
    QRadioButton* saperaRadio_ = nullptr;

    // Simulation parameters
    QGroupBox* simulationGroup_ = nullptr;
    QSpinBox* cameraCountSpin_ = nullptr;
    QSpinBox* frameRateSpin_ = nullptr;
    QCheckBox* synchronizedCheck_ = nullptr;
    QSpinBox* jitterSpin_ = nullptr;

    int cameraCount_ = 4;
    int frameRate_ = 30;
    bool synchronizedMode_ = true;
    int jitterMs_ = 0;

    // Buttons
    QPushButton* okButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
};

} // namespace cam_matrix::ui
