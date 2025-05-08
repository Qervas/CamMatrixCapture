#pragma once

#include "../filter_base.hpp"

namespace cam_matrix::core {

/**
 * @brief Grayscale filter
 * 
 * Converts a color image to grayscale.
 */
class GrayscaleFilter : public FilterBase {
public:
    GrayscaleFilter();
    
    QImage apply(const QImage& image, const QVariantMap& parameters) const override;
    QWidget* createControlWidget(QWidget* parent) const override;
};

} // namespace cam_matrix::core 