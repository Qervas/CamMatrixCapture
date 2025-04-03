#pragma once
#include "core/camera.hpp"
#include <string>
#include <vector>
#include <random>
#include <QImage>
#include <QTimer>
#include <QObject>

namespace cam_matrix::core {

class MockCamera : public QObject, public Camera {
    Q_OBJECT

public:
    MockCamera(int id, const std::string& name);
    ~MockCamera() override;

    std::string getName() const override { return name_; }
    bool isConnected() const override { return isConnected_; }

    // Renamed to avoid conflict with QObject::connect
    bool connectCamera();
    bool disconnectCamera();

    // Added to generate test images
    QImage getFrame();

signals:
    void newFrameAvailable(QImage frame);

private slots:
    void generateFrame();

private:
    int id_;
    std::string name_;
    bool isConnected_;
    QTimer frameTimer_;
    std::mt19937 rng_;
    std::vector<QColor> colors_;
    QImage currentFrame_;

    void createTestPattern();
};

} // namespace cam_matrix::core
