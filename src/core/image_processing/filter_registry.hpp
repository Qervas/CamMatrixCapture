#pragma once

#include "filter.hpp"
#include <QMap>
#include <QVector>

namespace cam_matrix::core {

/**
 * @brief Registry for all image filters
 * 
 * This class manages the collection of available image filters and provides
 * methods to access them by name or category.
 */
class FilterRegistry {
public:
    /**
     * @brief Get the singleton instance of the filter registry
     * 
     * @return Reference to the filter registry
     */
    static FilterRegistry& instance();

    /**
     * @brief Register a new filter
     * 
     * @param filter The filter to register
     */
    void registerFilter(FilterPtr filter);

    /**
     * @brief Get a filter by name
     * 
     * @param name The name of the filter
     * @return The filter or nullptr if not found
     */
    FilterPtr getFilter(const QString& name) const;

    /**
     * @brief Get all filters
     * 
     * @return List of all registered filters
     */
    QVector<FilterPtr> getAllFilters() const;

    /**
     * @brief Get filters by category
     * 
     * @param category The filter category
     * @return List of filters in the specified category
     */
    QVector<FilterPtr> getFiltersByCategory(Filter::Category category) const;

    /**
     * @brief Get all filter categories
     * 
     * @return List of all filter categories
     */
    QVector<Filter::Category> getCategories() const;
    
    /**
     * @brief Get filter names by category
     * 
     * @param category The filter category
     * @return List of filter names in the specified category
     */
    QStringList getFilterNamesByCategory(Filter::Category category) const;

private:
    FilterRegistry(); // Private constructor for singleton
    FilterRegistry(const FilterRegistry&) = delete;
    FilterRegistry& operator=(const FilterRegistry&) = delete;

    void registerBuiltinFilters();

    QMap<QString, FilterPtr> filters_;
    QMap<Filter::Category, QVector<FilterPtr>> filtersByCategory_;
};

} // namespace cam_matrix::core 