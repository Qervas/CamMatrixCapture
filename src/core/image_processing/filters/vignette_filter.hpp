#pragma once

#include "../filter_base.hpp"

namespace cam_matrix::core {

/**
 * @brief Vignette filter
 * 
 * Applies a vignette effect (darkened corners) to an image.
 */
class VignetteFilter : public FilterBase {
public:
    VignetteFilter();
    
    QImage apply(const QImage& image, const QVariantMap& parameters) const override;
    QWidget* createControlWidget(QWidget* parent) const override;
};

} // namespace cam_matrix::core 