#include "core/camera_tester.hpp"
#include <QDebug>
#include <QVBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>

namespace cam_matrix::core {

CameraTester::CameraTester(QWidget* parent)
    : QWidget(parent)
    , cameraManager_(std::make_unique<CameraManager>(this))
{
    setupUi();
}

CameraTester::~CameraTester() = default;

void CameraTester::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    
    auto* label = new QLabel("Camera Tester Widget", this);
    layout->addWidget(label);
    
    auto* refreshButton = new QPushButton("Refresh Cameras", this);
    layout->addWidget(refreshButton);
    
    connect(refreshButton, &QPushButton::clicked, this, &CameraTester::scanForCameras);
}

void CameraTester::initialize()
{
    scanForCameras();
}

void CameraTester::cleanup()
{
    disconnectAllCameras();
}

void CameraTester::scanForCameras()
{
    cameraManager_->scanForCameras();
    emit statusChanged(tr("Cameras scanned"));
}

void CameraTester::connectCamera(size_t index)
{
    if (cameraManager_->connectCamera(index)) {
        emit statusChanged(tr("Camera connected"));
        emit cameraConnected(index);
    } else {
        emit error(tr("Failed to connect camera"));
    }
}

void CameraTester::disconnectCamera(size_t index)
{
    if (cameraManager_->disconnectCamera(index)) {
        emit statusChanged(tr("Camera disconnected"));
        emit cameraDisconnected(index);
    } else {
        emit error(tr("Failed to disconnect camera"));
    }
}

void CameraTester::disconnectAllCameras()
{
    if (cameraManager_->disconnectAllCameras()) {
        emit statusChanged(tr("All cameras disconnected"));
    } else {
        emit error(tr("Failed to disconnect all cameras"));
    }
}

std::vector<Camera*> CameraTester::getCameras() const
{
    return cameraManager_->getCameras();
}

Camera* CameraTester::getCameraByIndex(size_t index) const
{
    return cameraManager_->getCameraByIndex(index);
}

SaperaCamera* CameraTester::getSaperaCameraByIndex(size_t index) const
{
    return cameraManager_->getSaperaCameraByIndex(index);
}

} // namespace cam_matrix::core 