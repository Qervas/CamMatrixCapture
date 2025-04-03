#pragma once
#include <string>
#include <memory>

namespace cam_matrix::core {

class Camera {
public:
    virtual ~Camera() = default;

    // Minimal interface for our example
    virtual std::string getName() const = 0;
    virtual bool isConnected() const = 0;
};

} // namespace cam_matrix::core
