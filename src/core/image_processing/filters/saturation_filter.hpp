#pragma once

#include "../filter_base.hpp"

namespace cam_matrix::core {

/**
 * @brief Saturation filter
 * 
 * Adjusts the color saturation of an image.
 */
class SaturationFilter : public FilterBase {
public:
    SaturationFilter();
    
    QImage apply(const QImage& image, const QVariantMap& parameters) const override;
    QWidget* createControlWidget(QWidget* parent) const override;
};

} // namespace cam_matrix::core 