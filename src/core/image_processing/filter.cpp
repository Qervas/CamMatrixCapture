#include "filter.hpp"

namespace cam_matrix::core {

QString Filter::categoryToString(Category category) {
    switch (category) {
        case Category::BasicAdjustment:
            return QObject::tr("Basic Adjustments");
        case Category::ColorEffect:
            return QObject::tr("Color Effects");
        case Category::ArtisticFilter:
            return QObject::tr("Artistic Filters");
        case Category::CorrectionTool:
            return QObject::tr("Correction Tools");
        default:
            return QObject::tr("Unknown Category");
    }
}

} // namespace cam_matrix::core 