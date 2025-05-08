#include "filter_registry.hpp"

// Include all filter implementations
#include "filters/grayscale_filter.hpp"
#include "filters/invert_filter.hpp"
#include "filters/blur_filter.hpp"
#include "filters/brightness_contrast_filter.hpp"
#include "filters/sepia_filter.hpp"
#include "filters/saturation_filter.hpp"
#include "filters/sharpen_filter.hpp"
#include "filters/threshold_filter.hpp"
#include "filters/edge_detection_filter.hpp"
#include "filters/vignette_filter.hpp"

namespace cam_matrix::core {

FilterRegistry& FilterRegistry::instance() {
    static FilterRegistry instance;
    return instance;
}

FilterRegistry::FilterRegistry() {
    registerBuiltinFilters();
}

void FilterRegistry::registerFilter(FilterPtr filter) {
    if (!filter) return;
    
    // Add to filters map by name
    filters_[filter->name()] = filter;
    
    // Add to filters by category
    filtersByCategory_[filter->category()].append(filter);
}

FilterPtr FilterRegistry::getFilter(const QString& name) const {
    if (filters_.contains(name)) {
        return filters_[name];
    }
    return nullptr;
}

QVector<FilterPtr> FilterRegistry::getAllFilters() const {
    return filters_.values().toVector();
}

QVector<FilterPtr> FilterRegistry::getFiltersByCategory(Filter::Category category) const {
    if (filtersByCategory_.contains(category)) {
        return filtersByCategory_[category];
    }
    return QVector<FilterPtr>();
}

QVector<Filter::Category> FilterRegistry::getCategories() const {
    return filtersByCategory_.keys().toVector();
}

QStringList FilterRegistry::getFilterNamesByCategory(Filter::Category category) const {
    QStringList names;
    for (const auto& filter : getFiltersByCategory(category)) {
        names.append(filter->name());
    }
    return names;
}

void FilterRegistry::registerBuiltinFilters() {
    // Basic adjustments
    registerFilter(std::make_shared<BrightnessContrastFilter>());
    registerFilter(std::make_shared<SaturationFilter>());
    
    // Color effects
    registerFilter(std::make_shared<GrayscaleFilter>());
    registerFilter(std::make_shared<SepiaFilter>());
    registerFilter(std::make_shared<InvertFilter>());
    registerFilter(std::make_shared<ThresholdFilter>());
    
    // Artistic filters
    registerFilter(std::make_shared<BlurFilter>());
    registerFilter(std::make_shared<SharpenFilter>());
    registerFilter(std::make_shared<EdgeDetectionFilter>());
    registerFilter(std::make_shared<VignetteFilter>());
    
    // More filters can be added here...
}

} // namespace cam_matrix::core 