#pragma once

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QImage>
#include <atomic>
#include <functional>
#include <memory>

namespace cam_matrix::core::sapera {

// Forward declarations
class SaperaCamera;

// Camera operation types
enum class CameraOpType {
    Connect,
    Disconnect,
    CapturePhoto,
    GetFrame,
    SetExposure,
    Custom
};

// Structure to represent a camera operation
struct CameraOperation {
    CameraOpType type;
    std::function<void()> operation;
    std::function<void(bool)> callback;
    std::string description;
};

// Thread class for camera operations
class CameraThread : public QThread {
    Q_OBJECT

public:
    explicit CameraThread(QObject* parent = nullptr);
    ~CameraThread() override;

    // Queue a camera operation to be executed in the thread
    void queueOperation(CameraOpType type, 
                       const std::function<void()>& operation,
                       const std::function<void(bool)>& callback = nullptr,
                       const std::string& description = "");

    // Stop the thread
    void stop();

signals:
    void operationCompleted(CameraOpType type, bool success);
    void frameReady(const QImage& frame);
    void errorOccurred(const QString& errorMessage);

protected:
    void run() override;

private:
    QMutex mutex_;
    QWaitCondition condition_;
    QQueue<CameraOperation> operationQueue_;
    std::atomic<bool> running_{true};
    
    // Execute a camera operation safely
    bool executeOperation(const CameraOperation& op);
};

} // namespace cam_matrix::core::sapera 