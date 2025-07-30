#pragma once
#include <memory>
#include <vector>

namespace capture {
class AutoExposureController {
public:
    AutoExposureController(float targetBrightness = 0.5f, float minExposure = 100.0f, float maxExposure = 100000.0f);
    void setTargetBrightness(float target);
    float getTargetBrightness() const;
    void setExposureLimits(float minExp, float maxExp);
    void processImage(const std::vector<uint8_t>& imageData, int width, int height);
    void setCameraExposure(float exposureValue);
    float getCurrentExposure() const;
private:
    float targetBrightness;
    float minExposure;
    float maxExposure;
    float currentExposure;
    float computeAverageBrightness(const std::vector<uint8_t>& imageData, int width, int height) const;
    void adjustExposure(float averageBrightness);
};
} // namespace capture 