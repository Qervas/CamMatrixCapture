#pragma once
#include <string>
#include <memory>
#include <QObject>
#include <QImage>

namespace cam_matrix::core {

class Camera : public QObject {
    Q_OBJECT

public:
    explicit Camera(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~Camera() = default;

    // Minimal interface for our example
    virtual std::string getName() const = 0;
    virtual bool isConnected() const = 0;

    // Adding these to match MockCamera's implementation
    virtual bool connectCamera() = 0;
    virtual bool disconnectCamera() = 0;

signals:
    void newFrameAvailable(const QImage& frame);
    void statusChanged(const std::string& message);
    void error(const std::string& message);
};

} // namespace cam_matrix::core
