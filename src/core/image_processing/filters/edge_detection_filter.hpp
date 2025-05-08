#pragma once

#include "../filter_base.hpp"

namespace cam_matrix::core {

/**
 * @brief Edge Detection filter
 * 
 * Detects and highlights edges in an image using a Sobel operator.
 */
class EdgeDetectionFilter : public FilterBase {
public:
    EdgeDetectionFilter();
    
    QImage apply(const QImage& image, const QVariantMap& parameters) const override;
    QWidget* createControlWidget(QWidget* parent) const override;
};

} // namespace cam_matrix::core 