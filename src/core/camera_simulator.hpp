#pragma once
#include <QObject>
#include <QTimer>
#include <QImage>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <vector>
#include <memory>
#include <random>
#include <atomic>
#include "core/camera.hpp"

namespace cam_matrix::core {

// Forward declaration
class SimulatedCamera;

// Simulates a matrix of synchronized cameras
class CameraSimulator : public QObject {
    Q_OBJECT

public:
    explicit CameraSimulator(QObject* parent = nullptr);
    ~CameraSimulator();

    // Camera management
    std::vector<std::shared_ptr<SimulatedCamera>> createCameraMatrix(int rows, int cols);
    std::shared_ptr<SimulatedCamera> createCamera(int id);

    // Simulator control
    void setFrameRate(int fps);
    void setJitter(int maxMsJitter);
    void setSimulationMode(bool synchronizedMode);

    // Start/stop simulation
    bool startSimulation();
    void stopSimulation();
    bool isRunning() const { return running_; }

    // Get simulator info
    int getFrameRate() const { return frameRate_; }
    int getJitter() const { return maxJitter_; }
    bool isSynchronizedMode() const { return synchronizedMode_; }

signals:
    void frameReady(const QImage& frame, int cameraId);

private slots:
    void generateFrames();

private:
    // Simulation parameters
    int frameRate_ = 30;         // FPS
    int maxJitter_ = 0;          // Max jitter in ms
    bool synchronizedMode_ = true; // Whether cameras are in sync
    std::atomic<bool> running_{false};

    // Camera objects
    std::vector<std::shared_ptr<SimulatedCamera>> cameras_;

    // Frame generation
    QTimer frameTimer_;
    std::mt19937 rng_;
    QImage generateTestPattern(int cameraId, qint64 timestamp);

    // Synchronization
    QMutex mutex_;
    QWaitCondition condition_;
    QThread* simulationThread_ = nullptr;

    // Thread function
    void simulationLoop();
};

// SimulatedCamera class simulating a camera device
class SimulatedCamera : public QObject, public Camera {
    Q_OBJECT

public:
    SimulatedCamera(int id, CameraSimulator* parent);
    ~SimulatedCamera() override;

    // Camera interface
    std::string getName() const override;
    bool isConnected() const override;
    bool connectCamera() override;
    bool disconnectCamera() override;

    // Camera properties
    int getId() const { return id_; }
    void setDelay(int msDelay) { delay_ = msDelay; }
    QImage getLastFrame() const;

    // For simulator to provide frames
    void setFrame(const QImage& frame);

signals:
    void newFrameAvailable(QImage frame);
    void statusChanged(const std::string& message);
    void error(const std::string& message);

private:
    int id_;
    CameraSimulator* simulator_;
    std::atomic<bool> connected_{false};
    QImage lastFrame_;
    mutable QMutex frameMutex_;
    int delay_ = 0; // Simulated delay in ms

    // Properties
    double exposureTime_ = 10000.0;  // in microseconds
    double gain_ = 1.0;
    QString format_ = "1920x1080";
    bool autoExposure_ = false;
};

} // namespace cam_matrix::core
