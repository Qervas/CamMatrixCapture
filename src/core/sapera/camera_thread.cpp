#include "camera_thread.hpp"
#include <QDebug>
#include <QCoreApplication>
#include <stdexcept>

namespace cam_matrix::core::sapera {

CameraThread::CameraThread(QObject* parent)
    : QThread(parent)
{
    // Start the thread when the object is created
    start();
}

CameraThread::~CameraThread()
{
    // Stop the thread and wait for it to finish
    stop();
    wait();
}

void CameraThread::queueOperation(CameraOpType type, 
                                 const std::function<void()>& operation,
                                 const std::function<void(bool)>& callback,
                                 const std::string& description)
{
    if (!isRunning()) {
        qWarning() << "Cannot queue operation, thread is not running";
        return;
    }

    CameraOperation op;
    op.type = type;
    op.operation = operation;
    op.callback = callback;
    op.description = description;

    // Add the operation to the queue
    {
        QMutexLocker locker(&mutex_);
        operationQueue_.enqueue(op);
        qDebug() << "Queued camera operation:" << QString::fromStdString(description);
    }

    // Wake up the thread
    condition_.wakeOne();
}

void CameraThread::stop()
{
    // Signal the thread to stop
    running_ = false;
    
    // Wake up the thread if it's waiting
    condition_.wakeAll();
    
    qDebug() << "Camera thread stopping...";
}

void CameraThread::run()
{
    qDebug() << "Camera thread started";
    
    while (running_) {
        CameraOperation op;
        
        // Get the next operation from the queue
        {
            QMutexLocker locker(&mutex_);
            
            // Wait for operations if the queue is empty
            if (operationQueue_.isEmpty()) {
                condition_.wait(&mutex_);
                
                // Check if we were woken up to stop
                if (!running_) {
                    break;
                }
            }
            
            // Skip if still empty (could happen when stopping)
            if (operationQueue_.isEmpty()) {
                continue;
            }
            
            // Get the next operation
            op = operationQueue_.dequeue();
        }
        
        // Execute the operation
        bool success = executeOperation(op);
        
        // Call the callback if provided
        if (op.callback) {
            try {
                op.callback(success);
            } catch (const std::exception& e) {
                qWarning() << "Exception in operation callback:" << e.what();
            } catch (...) {
                qWarning() << "Unknown exception in operation callback";
            }
        }
        
        // Emit signal for operation completion
        emit operationCompleted(op.type, success);
    }
    
    qDebug() << "Camera thread finished";
}

bool CameraThread::executeOperation(const CameraOperation& op)
{
    qDebug() << "Executing camera operation:" << QString::fromStdString(op.description);
    
    try {
        // Execute the operation function
        op.operation();
        return true;
    }
    catch (const std::exception& e) {
        QString errorMsg = QString("Exception in camera operation '%1': %2")
                               .arg(QString::fromStdString(op.description))
                               .arg(e.what());
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }
    catch (...) {
        QString errorMsg = QString("Unknown exception in camera operation '%1'")
                               .arg(QString::fromStdString(op.description));
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }
}

} // namespace cam_matrix::core::sapera 