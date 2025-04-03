#pragma once

#include "ui/pages/page.hpp"
#include <QListWidget>
#include <QPushButton>
#include <memory>

namespace cam_matrix::ui {

// Forward declarations
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
    void onRefreshCameras();  // We'll implement this instead of onRefresh()
    void onCameraSelected(int index);
    void onCameraStatusChanged(const QString& status);

private:
    void setupUi() override;
    void createConnections() override;
    void loadSettings();
    void saveSettings();

    QListWidget* cameraList_;
    CameraControlWidget* cameraControl_;  // Raw pointer, not unique_ptr
    QPushButton* refreshButton_;
};

} // namespace cam_matrix::ui
