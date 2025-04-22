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
    QListWidget* cameraList_;
    CameraControlWidget* cameraControl_;
    QPushButton* refreshButton_;
    QPushButton* connectButton_;
    QPushButton* disconnectButton_;
    QPushButton* testSaperaButton_;
    QPushButton* directCameraButton_;
    
    // Multi-camera sync widgets
    QGroupBox* syncGroup_;
    QPushButton* clearSelectionButton_;
    QPushButton* toggleSelectButton_;
    QPushButton* connectSelectedButton_;
    QPushButton* disconnectSelectedButton_;
    QPushButton* captureSyncButton_;
    QProgressBar* syncProgressBar_;
    QLabel* syncStatusLabel_;
    
    // Video display
    VideoDisplayWidget* videoDisplay_;
    
    // Sapera status widget
    SaperaStatusWidget* saperaStatus_;
    
    // Debug console
    QPlainTextEdit* debugConsole_;
    QPushButton* clearConsoleButton_;
    
    int selectedCameraIndex_;
};

} // namespace cam_matrix::ui
