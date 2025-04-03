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

    // Adding these to match MockCamera's implementation
    virtual bool connectCamera() = 0;
    virtual bool disconnectCamera() = 0;
};

} // namespace cam_matrix::core
