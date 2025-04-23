#pragma once

#include "../filter_base.hpp"

namespace cam_matrix::core {

/**
 * @brief Brightness and contrast filter
 * 
 * Adjusts the brightness and contrast of an image.
 */
class BrightnessContrastFilter : public FilterBase {
public:
    BrightnessContrastFilter();
    
    QImage apply(const QImage& image, const QVariantMap& parameters) const override;
    QWidget* createControlWidget(QWidget* parent) const override;
};

} // namespace cam_matrix::core 