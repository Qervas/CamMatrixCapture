#pragma once

#include <QString>
#include <QImage>
#include <QVariantMap>
#include <QWidget>
#include <memory>

namespace cam_matrix::core {

/**
 * @brief Base class for all image filters
 * 
 * This abstract class defines the interface that all image filters must implement.
 * Each filter provides a name, a category, and a method to apply the filter to an image.
 */
class Filter {
public:
    /**
     * Filter categories
     */
    enum class Category {
        BasicAdjustment,
        ColorEffect,
        ArtisticFilter,
        CorrectionTool
    };

    virtual ~Filter() = default;

    /**
     * @brief Apply the filter to an image
     * 
     * @param image The input image to process
     * @param parameters Parameters controlling the filter behavior
     * @return The processed image
     */
    virtual QImage apply(const QImage& image, const QVariantMap& parameters) const = 0;

    /**
     * @brief Get the name of the filter
     * 
     * @return The filter name
     */
    virtual QString name() const = 0;

    /**
     * @brief Get the category of the filter
     * 
     * @return The filter category
     */
    virtual Category category() const = 0;

    /**
     * @brief Create a control widget for adjusting filter parameters
     * 
     * @param parent The parent widget
     * @return A widget with UI controls for the filter
     */
    virtual QWidget* createControlWidget(QWidget* parent) const = 0;

    /**
     * @brief Get the default parameters for the filter
     * 
     * @return Default parameter values
     */
    virtual QVariantMap defaultParameters() const = 0;

    /**
     * @brief Translate a category enum to a readable string
     * 
     * @param category The filter category
     * @return String representation of the category
     */
    static QString categoryToString(Category category);
};

/**
 * @brief Smart pointer typedef for Filter objects
 */
using FilterPtr = std::shared_ptr<Filter>;

} // namespace cam_matrix::core 