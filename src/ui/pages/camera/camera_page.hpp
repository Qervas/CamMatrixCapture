#pragma once

#include "ui/pages/page.hpp"
#include "core/camera_manager.hpp"
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <memory>

namespace cam_matrix::ui {

class VideoDisplayWidget;
class CameraControlWidget;
class SaperaStatusWidget;

class CameraPage : public Page {
    Q_OBJECT

public:
    explicit CameraPage(QWidget* parent = nullptr);
    ~CameraPage() override;
    
    QString title() const override { return tr("Cameras"); }
    void initialize() override;

public slots:
    void refreshCameras() override;

protected:
    void setupUi() override;
    void createConnections() override;
    void cleanup() override;
    bool eventFilter(QObject* watched, QEvent* event) override;

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
    
    // Multi-camera synchronization slots
    void onCameraSelectionChanged(QListWidgetItem* item);
    void onToggleSelectAll();
    void onClearSelection();
    void onConnectSelectedCameras();
    void onDisconnectSelectedCameras();
    void onCaptureSync();
    void onSyncCaptureProgress(int current, int total);
    void onSyncCaptureComplete(int successCount, int total);
    
    // Debug console slots
    void logDebugMessage(const QString& message, const QString& type = "INFO");
    void clearDebugConsole();

private:
    void loadSettings();
    void saveSettings();
    void updateCameraList();
    void updateSyncUI();
    bool areAllCamerasSelected() const;

    std::unique_ptr<core::CameraManager> cameraManager_;
    int selectedCameraIndex_;
    
    // UI Components
    QListWidget* cameraList_{nullptr};
    QWidget* loadingOverlay_{nullptr};
    VideoDisplayWidget* videoDisplay_{nullptr};
    CameraControlWidget* cameraControl_{nullptr};
    SaperaStatusWidget* saperaStatus_{nullptr};
    QWidget* syncGroup_{nullptr};
    QProgressBar* syncProgressBar_{nullptr};
    QLabel* syncStatusLabel_{nullptr};
    QPlainTextEdit* debugConsole_{nullptr};
    
    // Buttons
    QPushButton* refreshButton_{nullptr};
    QPushButton* connectButton_{nullptr};
    QPushButton* disconnectButton_{nullptr};
    QPushButton* toggleSelectButton_{nullptr};
    QPushButton* clearSelectionButton_{nullptr};
    QPushButton* connectSelectedButton_{nullptr};
    QPushButton* disconnectSelectedButton_{nullptr};
    QPushButton* captureSyncButton_{nullptr};
    QPushButton* clearConsoleButton_{nullptr};
    QPushButton* testSaperaButton_{nullptr};
    QPushButton* directCameraButton_{nullptr};
};

} // namespace cam_matrix::ui
