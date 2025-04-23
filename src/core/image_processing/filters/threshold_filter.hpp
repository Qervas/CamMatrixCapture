#pragma once

#include "../filter_base.hpp"

namespace cam_matrix::core {

/**
 * @brief Threshold filter
 * 
 * Converts an image to black and white based on a threshold value.
 */
class ThresholdFilter : public FilterBase {
public:
    ThresholdFilter();
    
    QImage apply(const QImage& image, const QVariantMap& parameters) const override;
    QWidget* createControlWidget(QWidget* parent) const override;
};

} // namespace cam_matrix::core 