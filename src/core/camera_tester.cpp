#include "core/camera_tester.hpp"
#include <QGroupBox>
#include <QRadioButton>
#include <QMessageBox>
#include <QDebug>
#include <QString>
#include <QButtonGroup>

namespace cam_matrix::core {

CameraTester::CameraTester(QWidget* parent)
    : QWidget(parent)
    , mockManager_(std::make_shared<CameraManager>())
    , saperaManager_(std::make_shared<SaperaManager>())
    , useMockCameras_(true)
    , selectedCameraIndex_(-1)
    , isConnected_(false)
{
    setupUi();
    createConnections();
    updateCameraList();
}

CameraTester::~CameraTester() {
    disconnectFromCamera();
}

void CameraTester::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    // Camera type selection
    auto* typeGroup = new QGroupBox("Camera Type", this);
    auto* typeLayout = new QHBoxLayout(typeGroup);

    auto* mockRadio = new QRadioButton("Mock Cameras", typeGroup);
    auto* realRadio = new QRadioButton("Real Cameras (Sapera)", typeGroup);
    mockRadio->setChecked(true);

    auto* buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton(mockRadio, 0);
    buttonGroup->addButton(realRadio, 1);

    typeLayout->addWidget(mockRadio);
    typeLayout->addWidget(realRadio);

    mainLayout->addWidget(typeGroup);

    // Camera selection
    auto* selectionGroup = new QGroupBox("Camera Selection", this);
    auto* selectionLayout = new QHBoxLayout(selectionGroup);

    cameraListCombo_ = new QComboBox(selectionGroup);
    refreshButton_ = new QPushButton("Refresh", selectionGroup);

    selectionLayout->addWidget(cameraListCombo_);
    selectionLayout->addWidget(refreshButton_);

    mainLayout->addWidget(selectionGroup);

    // Camera controls
    auto* controlGroup = new QGroupBox("Camera Control", this);
    auto* controlLayout = new QVBoxLayout(controlGroup);

    auto* buttonLayout = new QHBoxLayout();
    connectButton_ = new QPushButton("Connect", controlGroup);
    disconnectButton_ = new QPushButton("Disconnect", controlGroup);
    disconnectButton_->setEnabled(false);

    buttonLayout->addWidget(connectButton_);
    buttonLayout->addWidget(disconnectButton_);
    controlLayout->addLayout(buttonLayout);

    // Exposure control
    exposureLabel_ = new QLabel("Exposure: 10.0 ms", controlGroup);
    exposureSlider_ = new QSlider(Qt::Horizontal, controlGroup);
    exposureSlider_->setRange(1, 100);
    exposureSlider_->setValue(10);
    exposureSlider_->setTracking(true);
    exposureSlider_->setEnabled(false);

    controlLayout->addWidget(exposureLabel_);
    controlLayout->addWidget(exposureSlider_);

    mainLayout->addWidget(controlGroup);

    // Video display
    videoDisplay_ = new ui::VideoDisplayWidget(this);
    mainLayout->addWidget(videoDisplay_, 1);

    // Status label
    statusLabel_ = new QLabel("Ready", this);
    mainLayout->addWidget(statusLabel_);

    // Set connections
    connect(buttonGroup, &QButtonGroup::idClicked,
        [this, mockRadio, realRadio](int id) {
            if (id == 0) {
                onMockSelected(true);
            } else {
                onRealSelected(true);
            }
        });
}

void CameraTester::createConnections() {
    connect(refreshButton_, &QPushButton::clicked,
            this, &CameraTester::refreshCameras);

    connect(connectButton_, &QPushButton::clicked,
            this, &CameraTester::connectToSelectedCamera);

    connect(disconnectButton_, &QPushButton::clicked,
            this, &CameraTester::disconnectFromCamera);

    connect(cameraListCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraTester::onCameraSelected);

    connect(exposureSlider_, &QSlider::valueChanged,
            this, &CameraTester::onExposureChanged);

    // Connect manager signals
    connect(mockManager_.get(), &CameraManager::camerasChanged,
            this, &CameraTester::updateCameraList);

    connect(saperaManager_.get(), &SaperaManager::camerasChanged,
            this, &CameraTester::updateCameraList);

    connect(saperaManager_.get(), &SaperaManager::statusChanged,
            [this](const std::string& message) {
                statusLabel_->setText(QString::fromStdString(message));
            });

    connect(saperaManager_.get(), &SaperaManager::error,
            [this](const std::string& message) {
                QMessageBox::warning(this, "Sapera Error",
                                    QString::fromStdString(message));
            });
}

void CameraTester::refreshCameras() {
    disconnectFromCamera();
    statusLabel_->setText("Refreshing camera list...");

    if (useMockCameras_) {
        mockManager_->scanForCameras();
    } else {
        saperaManager_->scanForCameras();
    }
}

void CameraTester::onMockSelected(bool selected) {
    if (selected && !useMockCameras_) {
        disconnectFromCamera();
        useMockCameras_ = true;
        updateCameraList();
        statusLabel_->setText("Switched to mock cameras");
    }
}

void CameraTester::onRealSelected(bool selected) {
    if (selected && useMockCameras_) {
        disconnectFromCamera();
        useMockCameras_ = false;
        updateCameraList();
        statusLabel_->setText("Switched to Sapera cameras");
    }
}

void CameraTester::updateCameraList() {
    cameraListCombo_->clear();

    std::vector<std::shared_ptr<Camera>> cameras;

    if (useMockCameras_) {
        cameras = mockManager_->getCameras();
    } else {
        cameras = saperaManager_->getCameras();
    }

    for (size_t i = 0; i < cameras.size(); ++i) {
        cameraListCombo_->addItem(
            QString("%1: %2")
                .arg(i)
                .arg(QString::fromStdString(cameras[i]->getName())),
            static_cast<int>(i)
        );
    }

    selectedCameraIndex_ = -1;
    if (cameras.empty()) {
        statusLabel_->setText("No cameras found");
    } else {
        statusLabel_->setText(QString("Found %1 cameras").arg(cameras.size()));
    }
}

void CameraTester::onCameraSelected(int index) {
    selectedCameraIndex_ = index;
    if (index >= 0) {
        connectButton_->setEnabled(true);
    } else {
        connectButton_->setEnabled(false);
    }
}

void CameraTester::connectToSelectedCamera() {
    if (selectedCameraIndex_ < 0) {
        return;
    }

    statusLabel_->setText("Connecting to camera...");
    bool success = false;

    if (useMockCameras_) {
        auto camera = mockManager_->getMockCameraByIndex(selectedCameraIndex_);
        if (camera) {
            // Connect to camera
            success = camera->connectCamera();

            // Connect signals/slots for frame updates
            connect(camera.get(), &MockCamera::newFrameAvailable,
                    this, &CameraTester::onFrameReceived);
        }
    } else {
        auto camera = saperaManager_->getSaperaCameraByIndex(selectedCameraIndex_);
        if (camera) {
            // Connect to camera
            success = camera->connectCamera();

            // Connect signals/slots for frame updates
            connect(camera.get(), &SaperaCamera::newFrameAvailable,
                    this, &CameraTester::onFrameReceived);
        }
    }

    if (success) {
        isConnected_ = true;
        connectButton_->setEnabled(false);
        disconnectButton_->setEnabled(true);
        exposureSlider_->setEnabled(true);
        statusLabel_->setText("Camera connected");
    } else {
        statusLabel_->setText("Failed to connect to camera");
    }
}

void CameraTester::disconnectFromCamera() {
    if (!isConnected_ || selectedCameraIndex_ < 0) {
        return;
    }

    statusLabel_->setText("Disconnecting from camera...");
    bool success = false;

    if (useMockCameras_) {
        auto camera = mockManager_->getMockCameraByIndex(selectedCameraIndex_);
        if (camera) {
            // Disconnect signals/slots for frame updates
            camera->disconnect(this);

            // Disconnect from camera
            success = camera->disconnectCamera();
        }
    } else {
        auto camera = saperaManager_->getSaperaCameraByIndex(selectedCameraIndex_);
        if (camera) {
            // Disconnect signals/slots for frame updates
            camera->disconnect(this);

            // Disconnect from camera
            success = camera->disconnectCamera();
        }
    }

    if (success) {
        isConnected_ = false;
        connectButton_->setEnabled(true);
        disconnectButton_->setEnabled(false);
        exposureSlider_->setEnabled(false);
        videoDisplay_->clearFrame();
        statusLabel_->setText("Camera disconnected");
    } else {
        statusLabel_->setText("Failed to disconnect from camera");
    }
}

void CameraTester::onExposureChanged(int value) {
    if (!isConnected_ || selectedCameraIndex_ < 0) {
        return;
    }

    // Convert slider value (1-100) to exposure time in ms
    double exposureMs = value * 0.1; // 0.1 to 10 ms range

    exposureLabel_->setText(QString("Exposure: %1 ms").arg(exposureMs, 0, 'f', 1));

    if (!useMockCameras_) {
        // Set exposure on Sapera camera (needs microseconds)
        auto camera = saperaManager_->getSaperaCameraByIndex(selectedCameraIndex_);
        if (camera) {
            camera->setExposureTime(exposureMs * 1000); // Convert ms to μs
        }
    }
}

void CameraTester::onFrameReceived(const QImage& frame) {
    videoDisplay_->updateFrame(frame);
}

} // namespace cam_matrix::core
