#include "core/camera_simulator.hpp"
#include <QPainter>
#include <QDateTime>
#include <QDebug>
#include <QRandomGenerator>
#include <chrono>
#include <thread>

namespace cam_matrix::core {

CameraSimulator::CameraSimulator(QObject* parent)
    : QObject(parent)
    , rng_(std::random_device{}())
{
    connect(&frameTimer_, &QTimer::timeout, this, &CameraSimulator::generateFrames);
}

CameraSimulator::~CameraSimulator() {
    stopSimulation();
}

std::vector<std::shared_ptr<SimulatedCamera>> CameraSimulator::createCameraMatrix(int rows, int cols) {
    std::vector<std::shared_ptr<SimulatedCamera>> newCameras;
    int startId = cameras_.size();

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int id = startId + r * cols + c;
            auto camera = std::make_shared<SimulatedCamera>(id, this);
            newCameras.push_back(camera);
            cameras_.push_back(camera);
        }
    }

    return newCameras;
}

std::shared_ptr<SimulatedCamera> CameraSimulator::createCamera(int id) {
    auto camera = std::make_shared<SimulatedCamera>(id, this);
    cameras_.push_back(camera);
    return camera;
}

void CameraSimulator::setFrameRate(int fps) {
    frameRate_ = qBound(1, fps, 120);  // Limit FPS range

    if (running_) {
        // Update timer
        frameTimer_.setInterval(1000 / frameRate_);
    }
}

void CameraSimulator::setJitter(int maxMsJitter) {
    maxJitter_ = qBound(0, maxMsJitter, 100);  // Limit jitter range
}

void CameraSimulator::setSimulationMode(bool synchronizedMode) {
    synchronizedMode_ = synchronizedMode;
}

bool CameraSimulator::startSimulation() {
    if (running_ || cameras_.empty()) {
        return false;
    }

    // Start in either threaded or simple mode
    if (synchronizedMode_) {
        // Start simulation thread for synchronous simulation
        running_ = true;
        simulationThread_ = QThread::create([this]{ simulationLoop(); });
        simulationThread_->start();
    } else {
        // Simple timer for asynchronous simulation
        frameTimer_.setInterval(1000 / frameRate_);
        frameTimer_.start();
        running_ = true;
    }

    qDebug() << "Camera simulator started with" << cameras_.size() << "cameras";
    return true;
}

void CameraSimulator::stopSimulation() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Stop the timer if used
    frameTimer_.stop();

    // Stop the thread if used
    if (simulationThread_) {
        // Wake up the condition
        condition_.wakeAll();

        // Wait for thread to finish
        simulationThread_->quit();
        simulationThread_->wait();
        delete simulationThread_;
        simulationThread_ = nullptr;
    }

    qDebug() << "Camera simulator stopped";
}

void CameraSimulator::generateFrames() {
    if (!running_) return;

    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();

    for (const auto& camera : cameras_) {
        if (camera->isConnected()) {
            // Generate frame with possible jitter
            int jitter = maxJitter_ > 0 ? QRandomGenerator::global()->bounded(maxJitter_) : 0;
            QThread::msleep(jitter);

            QImage frame = generateTestPattern(camera->getId(), timestamp);
            camera->setFrame(frame);
            emit frameReady(frame, camera->getId());
        }
    }
}

void CameraSimulator::simulationLoop() {
    const int frameTimeMs = 1000 / frameRate_;

    while (running_) {
        auto startTime = std::chrono::steady_clock::now();

        // Generate synchronized frames for all cameras
        generateFrames();

        // Calculate time to next frame
        auto elapsedTime = std::chrono::steady_clock::now() - startTime;
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime).count();
        int sleepTime = frameTimeMs - elapsedMs;

        if (sleepTime > 0) {
            // Use wait condition so it can be interrupted when stopping
            QMutexLocker locker(&mutex_);
            condition_.wait(&mutex_, sleepTime);
        }
    }
}

QImage CameraSimulator::generateTestPattern(int cameraId, qint64 timestamp) {
    QImage frame(1280, 720, QImage::Format_RGB32);
    frame.fill(Qt::black);

    QPainter painter(&frame);

    // Generate colors based on camera ID
    QColor color = QColor::fromHsv((cameraId * 60) % 360, 255, 255);

    // Draw moving pattern
    static int patternPos = 0;
    patternPos = (patternPos + 5) % frame.width();

    // Draw circles in a grid
    for (int y = 50; y < frame.height() - 50; y += 100) {
        for (int x = 50; x < frame.width() - 50; x += 100) {
            int radius = 20 + (cameraId % 3) * 5;

            // Move circles based on time
            int offsetX = (patternPos + x) % frame.width();

            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawEllipse(offsetX - radius, y - radius, radius * 2, radius * 2);
        }
    }

    // Draw camera ID and timestamp
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 20));

    QString cameraText = QString("Camera %1").arg(cameraId);
    painter.drawText(20, 40, cameraText);

    QString timestampText = QDateTime::fromMSecsSinceEpoch(timestamp).toString("yyyy-MM-dd hh:mm:ss.zzz");
    painter.drawText(20, frame.height() - 20, timestampText);

    // Draw frame number
    static QHash<int, int> frameCounters;
    int frameNum = frameCounters.value(cameraId, 0) + 1;
    frameCounters[cameraId] = frameNum;

    QString frameText = QString("Frame #%1").arg(frameNum);
    painter.drawText(frame.width() - 200, 40, frameText);

    return frame;
}

// ==== SimulatedCamera Implementation ====

SimulatedCamera::SimulatedCamera(int id, CameraSimulator* parent)
    : QObject(parent)
    , id_(id)
    , simulator_(parent)
{
    lastFrame_ = QImage(640, 480, QImage::Format_RGB32);
    lastFrame_.fill(Qt::black);
}

SimulatedCamera::~SimulatedCamera() {
    disconnectCamera();
}

std::string SimulatedCamera::getName() const {
    return "Simulated Camera " + std::to_string(id_);
}

bool SimulatedCamera::isConnected() const {
    return connected_;
}

bool SimulatedCamera::connectCamera() {
    if (connected_) {
        return true;
    }

    connected_ = true;
    emit statusChanged("Connected to simulated camera " + std::to_string(id_));
    return true;
}

bool SimulatedCamera::disconnectCamera() {
    if (!connected_) {
        return true;
    }

    connected_ = false;
    emit statusChanged("Disconnected from simulated camera " + std::to_string(id_));
    return true;
}

QImage SimulatedCamera::getLastFrame() const {
    QMutexLocker locker(&frameMutex_);
    return lastFrame_.copy();
}

void SimulatedCamera::setFrame(const QImage& frame) {
    if (!connected_) {
        return;
    }

    // Add simulated processing delay if configured
    if (delay_ > 0) {
        QThread::msleep(delay_);
    }

    // Update the frame
    {
        QMutexLocker locker(&frameMutex_);
        lastFrame_ = frame;
    }

    // Emit the frame
    emit newFrameAvailable(frame);
}

} // namespace cam_matrix::core
