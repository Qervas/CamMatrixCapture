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
    try {
        // Stop the thread and wait for it to finish
        stop();
        
        // Wait with timeout to prevent deadlocks
        if (!wait(3000)) {
            qWarning() << "Camera thread did not terminate in time, forcing termination";
            terminate();
            wait(1000);
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in CameraThread destructor:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception in CameraThread destructor";
    }
}

void CameraThread::queueOperation(CameraOpType type, 
                                 const std::function<void()>& operation,
                                 const std::function<void(bool)>& callback,
                                 const std::string& description)
{
    try {
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
    } catch (const std::exception& e) {
        qWarning() << "Exception in queueOperation:" << e.what();
        if (callback) {
            try {
                callback(false);
            } catch (...) {
                qWarning() << "Exception in callback during exception handling";
            }
        }
    } catch (...) {
        qWarning() << "Unknown exception in queueOperation";
        if (callback) {
            try {
                callback(false);
            } catch (...) {
                qWarning() << "Exception in callback during exception handling";
            }
        }
    }
}

void CameraThread::stop()
{
    try {
        // Signal the thread to stop
        running_ = false;
        
        // Clear any pending operations to prevent deadlocks
        {
            QMutexLocker locker(&mutex_);
            while (!operationQueue_.isEmpty()) {
                CameraOperation op = operationQueue_.dequeue();
                qDebug() << "Discarding queued operation during shutdown:" << QString::fromStdString(op.description);
                // Call callbacks with failure
                if (op.callback) {
                    try {
                        op.callback(false);
                    } catch (...) {
                        qWarning() << "Exception in callback during thread stop";
                    }
                }
            }
        }
        
        // Wake up the thread if it's waiting
        condition_.wakeAll();
        
        qDebug() << "Camera thread stopping...";
    } catch (const std::exception& e) {
        qWarning() << "Exception in stop:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception in stop";
    }
}

void CameraThread::run()
{
    try {
        qDebug() << "Camera thread started";
        
        while (running_) {
            CameraOperation op;
            bool hasOperation = false;
            
            // Get the next operation from the queue
            {
                QMutexLocker locker(&mutex_);
                
                // Wait for operations if the queue is empty
                if (operationQueue_.isEmpty() && running_) {
                    // Use timed wait to prevent permanent blocking
                    condition_.wait(&mutex_, 1000); // 1 second timeout
                }
                
                // Check if we were woken up to stop
                if (!running_) {
                    break;
                }
                
                // Skip if still empty (could happen when stopping or after timeout)
                if (!operationQueue_.isEmpty()) {
                    op = operationQueue_.dequeue();
                    hasOperation = true;
                }
            }
            
            // Execute the operation if we have one
            if (hasOperation) {
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
                try {
                    emit operationCompleted(op.type, success);
                } catch (const std::exception& e) {
                    qWarning() << "Exception when emitting operationCompleted:" << e.what();
                } catch (...) {
                    qWarning() << "Unknown exception when emitting operationCompleted";
                }
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "Exception in camera thread run loop:" << e.what();
    } catch (...) {
        qWarning() << "Unknown exception in camera thread run loop";
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
        try {
            emit errorOccurred(errorMsg);
        } catch (...) {
            qWarning() << "Exception when emitting errorOccurred";
        }
        return false;
    }
    catch (...) {
        QString errorMsg = QString("Unknown exception in camera operation '%1'")
                               .arg(QString::fromStdString(op.description));
        qWarning() << errorMsg;
        try {
            emit errorOccurred(errorMsg);
        } catch (...) {
            qWarning() << "Exception when emitting errorOccurred";
        }
        return false;
    }
}

} // namespace cam_matrix::core::sapera 