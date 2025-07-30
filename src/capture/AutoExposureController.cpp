#include "AutoExposureController.hpp"
#include <algorithm>

namespace capture {

AutoExposureController::AutoExposureController(float targetBrightness, float minExposure, float maxExposure)
    : targetBrightness(targetBrightness), minExposure(minExposure), maxExposure(maxExposure), currentExposure((minExposure+maxExposure)/2.0f) {}

void AutoExposureController::setTargetBrightness(float target) {
    targetBrightness = target;
}

float AutoExposureController::getTargetBrightness() const {
    return targetBrightness;
}

void AutoExposureController::setExposureLimits(float minExp, float maxExp) {
    minExposure = minExp;
    maxExposure = maxExp;
}

float AutoExposureController::getCurrentExposure() const {
    return currentExposure;
}

void AutoExposureController::processImage(const std::vector<uint8_t>& imageData, int width, int height) {
    float avg = computeAverageBrightness(imageData, width, height);
    adjustExposure(avg);
}

float AutoExposureController::computeAverageBrightness(const std::vector<uint8_t>& imageData, int width, int height) const {
    if (imageData.empty() || width <= 0 || height <= 0) return 0.0f;
    size_t pixelCount = width * height;
    uint64_t sum = 0;
    for (size_t i = 0; i < pixelCount; ++i) {
        sum += imageData[i];
    }
    return static_cast<float>(sum) / pixelCount / 255.0f;
}

void AutoExposureController::adjustExposure(float averageBrightness) {
    float error = targetBrightness - averageBrightness;
    float adjustment = error * 10000.0f; // Simple proportional control
    currentExposure = std::clamp(currentExposure + adjustment, minExposure, maxExposure);
    setCameraExposure(currentExposure);
}

void AutoExposureController::setCameraExposure(float exposureValue) {
    // TODO: Integrate with real camera API
    // For now, just store the value
    currentExposure = exposureValue;
}

} // namespace capture 