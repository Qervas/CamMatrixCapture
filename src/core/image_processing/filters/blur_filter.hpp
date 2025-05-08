#pragma once

#include "../filter_base.hpp"

namespace cam_matrix::core {

/**
 * @brief Blur filter
 * 
 * Applies a gaussian blur to an image.
 */
class BlurFilter : public FilterBase {
public:
    BlurFilter();
    
    QImage apply(const QImage& image, const QVariantMap& parameters) const override;
    QWidget* createControlWidget(QWidget* parent) const override;
};

} // namespace cam_matrix::core 