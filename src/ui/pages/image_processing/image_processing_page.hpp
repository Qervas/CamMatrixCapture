#pragma once

#include "ui/pages/page.hpp"
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QGroupBox>
#include <QSplitter>
#include <QTabWidget>
#include <memory>

class QGraphicsView;
class QGraphicsScene;
class QStackedWidget;

namespace cam_matrix::ui {

class VideoDisplayWidget;

/**
 * @brief Image processing page for applying filters and effects to images
 */
class ImageProcessingPage : public Page {
    Q_OBJECT

public:
    explicit ImageProcessingPage(QWidget* parent = nullptr);
    ~ImageProcessingPage() override;

    QString title() const override { return tr("Image Processing"); }
    void initialize() override;
    void cleanup() override;

private slots:
    // Filter application
    void onApplyFilter();
    void onResetFilters();
    void onFilterCategoryChanged(int index);
    void onFilterSelected(int index);
    void onParameterChanged();
    void onSaveFilterProfile();
    void onLoadFilterProfile();
    
    // Image handling
    void onOpenImage();
    void onSaveImage();
    void onImageLoaded(const QImage& image);
    void onToggleBeforeAfter(bool showBefore);
    
    // Batch processing
    void onAddToBatch();
    void onRemoveFromBatch();
    void onStartBatchProcessing();
    void onBatchProcessingComplete();

    // Logging
    void logMessage(const QString& message, const QString& type = "INFO");

private:
    void setupUi() override;
    void createConnections() override;
    void loadSettings();
    void saveSettings();
    void updateFilterControls();
    void applyCurrentFilter();
    QImage applyFilters(const QImage& sourceImage);
    
    // UI Components
    QSplitter* mainSplitter_;
    
    // Left panel - Filter controls
    QGroupBox* filtersGroupBox_;
    QComboBox* filterCategoryCombo_;
    QListWidget* filtersListWidget_;
    QStackedWidget* filterControlsStack_;
    QPushButton* applyFilterButton_;
    QPushButton* resetFilterButton_;
    QPushButton* saveProfileButton_;
    QPushButton* loadProfileButton_;
    
    // Center panel - Image preview
    QGraphicsView* imageView_;
    QGraphicsScene* imageScene_;
    QCheckBox* beforeAfterToggle_;
    QSlider* zoomSlider_;
    QPushButton* openImageButton_;
    QPushButton* saveImageButton_;
    
    // Right panel - Batch processing
    QGroupBox* batchGroupBox_;
    QListWidget* batchListWidget_;
    QPushButton* addToBatchButton_;
    QPushButton* removeFromBatchButton_;
    QPushButton* processBatchButton_;
    QLabel* batchStatusLabel_;

    // Data
    QImage originalImage_;
    QImage processedImage_;
    QList<QPair<QString, QVariantMap>> appliedFilters_; // Filter name and parameters
    
    // Active UI state
    int currentFilterCategory_;
    int currentFilterIndex_;
};

} // namespace cam_matrix::ui 