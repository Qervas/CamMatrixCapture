#include "image_processing_page.hpp"
#include "core/settings.hpp"
#include "core/image_processing/filter_registry.hpp"
#include "core/image_processing/filter.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QStackedWidget>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QApplication>
#include <QPainter>
#include <QPushButton>
#include <QDir>
#include <QInputDialog>
#include <QLineEdit>
#include <QMetaMethod>

namespace cam_matrix::ui {

ImageProcessingPage::ImageProcessingPage(QWidget* parent)
    : Page(parent)
    , currentFilterCategory_(0)
    , currentFilterIndex_(-1)
{
    setupUi();
    createConnections();
}

ImageProcessingPage::~ImageProcessingPage() {
    cleanup();
}

void ImageProcessingPage::initialize() {
    // Populate filter categories from the filter registry
    filterCategoryCombo_->clear();
    for (auto category : cam_matrix::core::FilterRegistry::instance().getCategories()) {
        filterCategoryCombo_->addItem(cam_matrix::core::Filter::categoryToString(category), 
                                     QVariant::fromValue(static_cast<int>(category)));
    }
    
    // Load settings
    loadSettings();
    
    // Initial state
    updateFilterControls();
    logMessage(tr("Image Processing page initialized"));
}

void ImageProcessingPage::cleanup() {
    saveSettings();
    logMessage(tr("Image Processing page cleanup completed"));
}

void ImageProcessingPage::setupUi() {
    // Get system palette for theme-aware colors
    QPalette systemPalette = QApplication::palette();
    bool isDarkTheme = systemPalette.color(QPalette::Window).lightness() < 128;
    
    // Theme-aware colors
    QString borderColor = isDarkTheme ? "#555555" : "#cccccc";
    QString bgLight = isDarkTheme ? "#333333" : "#f0f0f0";
    QString bgLighter = isDarkTheme ? "#3c3c3c" : "#ffffff";
    QString textColor = isDarkTheme ? "#e0e0e0" : "#202020";
    
    // Main layout
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // Main splitter for the three panels
    mainSplitter_ = new QSplitter(Qt::Horizontal);
    mainSplitter_->setChildrenCollapsible(false);
    
    // ---- Left Panel: Filter Controls ----
    auto leftWidget = new QWidget();
    auto leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);
    
    // Filter category and list
    filtersGroupBox_ = new QGroupBox(tr("Image Filters"));
    filtersGroupBox_->setStyleSheet(QString(
        "QGroupBox { font-weight: bold; border: 1px solid %1; border-radius: 5px; margin-top: 10px; padding-top: 10px; color: %2; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
    ).arg(borderColor, textColor));
    
    auto filtersLayout = new QVBoxLayout(filtersGroupBox_);
    filtersLayout->setContentsMargins(10, 15, 10, 10);
    
    auto categoryLabel = new QLabel(tr("Filter Category:"));
    filterCategoryCombo_ = new QComboBox();
    
    filtersListWidget_ = new QListWidget();
    QString listStyle = QString(
        "QListWidget { background: %2; border: 1px solid %1; border-radius: 3px; color: %3; } "
        "QListWidget::item { padding: 5px; border-bottom: 1px solid %1; } "
        "QListWidget::item:selected { background: rgba(0, 120, 215, 0.6); color: white; }"
    ).arg(borderColor, bgLighter, textColor);
    filtersListWidget_->setStyleSheet(listStyle);
    
    filtersLayout->addWidget(categoryLabel);
    filtersLayout->addWidget(filterCategoryCombo_);
    filtersLayout->addWidget(new QLabel(tr("Available Filters:")));
    filtersLayout->addWidget(filtersListWidget_);
    
    // Filter parameters (stacked widget for different filters)
    auto parametersLabel = new QLabel(tr("Filter Parameters:"));
    filterControlsStack_ = new QStackedWidget();
    filterControlsStack_->setMinimumHeight(200);
    filterControlsStack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Default "no filter selected" page
    auto noFilterPage = new QWidget();
    auto noFilterLayout = new QVBoxLayout(noFilterPage);
    noFilterLayout->addWidget(new QLabel(tr("Select a filter to adjust parameters")));
    noFilterLayout->addStretch();
    filterControlsStack_->addWidget(noFilterPage);
    
    filtersLayout->addWidget(parametersLabel);
    filtersLayout->addWidget(filterControlsStack_);
    
    // Filter action buttons
    auto filterButtonsLayout = new QHBoxLayout();
    applyFilterButton_ = new QPushButton(tr("Apply Filter"));
    resetFilterButton_ = new QPushButton(tr("Reset"));
    
    filterButtonsLayout->addWidget(applyFilterButton_);
    filterButtonsLayout->addWidget(resetFilterButton_);
    filtersLayout->addLayout(filterButtonsLayout);
    
    // Filter profiles
    auto profilesLayout = new QHBoxLayout();
    saveProfileButton_ = new QPushButton(tr("Save Profile"));
    loadProfileButton_ = new QPushButton(tr("Load Profile"));
    
    profilesLayout->addWidget(saveProfileButton_);
    profilesLayout->addWidget(loadProfileButton_);
    filtersLayout->addLayout(profilesLayout);
    
    leftLayout->addWidget(filtersGroupBox_);
    
    // ---- Center Panel: Image Preview ----
    auto centerWidget = new QWidget();
    auto centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(10);
    
    // Preview title
    auto previewLabel = new QLabel(tr("Image Preview"));
    previewLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = previewLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    previewLabel->setFont(titleFont);
    
    // Image view
    imageScene_ = new QGraphicsScene(this);
    imageView_ = new QGraphicsView(imageScene_);
    imageView_->setRenderHint(QPainter::Antialiasing, true);
    imageView_->setRenderHint(QPainter::SmoothPixmapTransform, true);
    imageView_->setDragMode(QGraphicsView::ScrollHandDrag);
    imageView_->setBackgroundBrush(QBrush(isDarkTheme ? QColor(30, 30, 30) : QColor(240, 240, 240)));
    imageView_->setFrameShape(QFrame::NoFrame);
    imageView_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Controls for the image preview
    auto previewControlsLayout = new QHBoxLayout();
    
    openImageButton_ = new QPushButton(tr("Open Image"));
    saveImageButton_ = new QPushButton(tr("Save Image"));
    saveImageButton_->setEnabled(false);
    
    beforeAfterToggle_ = new QCheckBox(tr("Show Original"));
    
    auto zoomLabel = new QLabel(tr("Zoom:"));
    zoomSlider_ = new QSlider(Qt::Horizontal);
    zoomSlider_->setRange(-50, 50);
    zoomSlider_->setValue(0);
    zoomSlider_->setTickPosition(QSlider::TicksBelow);
    
    previewControlsLayout->addWidget(openImageButton_);
    previewControlsLayout->addWidget(saveImageButton_);
    previewControlsLayout->addStretch();
    previewControlsLayout->addWidget(beforeAfterToggle_);
    previewControlsLayout->addStretch();
    previewControlsLayout->addWidget(zoomLabel);
    previewControlsLayout->addWidget(zoomSlider_);
    
    centerLayout->addWidget(previewLabel);
    centerLayout->addWidget(imageView_);
    centerLayout->addLayout(previewControlsLayout);
    
    // ---- Right Panel: Batch Processing ----
    auto rightWidget = new QWidget();
    auto rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);
    
    batchGroupBox_ = new QGroupBox(tr("Batch Processing"));
    batchGroupBox_->setStyleSheet(QString(
        "QGroupBox { font-weight: bold; border: 1px solid %1; border-radius: 5px; margin-top: 10px; padding-top: 10px; color: %2; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
    ).arg(borderColor, textColor));
    
    auto batchLayout = new QVBoxLayout(batchGroupBox_);
    batchLayout->setContentsMargins(10, 15, 10, 10);
    
    batchListWidget_ = new QListWidget();
    batchListWidget_->setStyleSheet(listStyle);
    
    // Batch control buttons
    auto batchButtonsLayout = new QHBoxLayout();
    addToBatchButton_ = new QPushButton(tr("Add Image"));
    removeFromBatchButton_ = new QPushButton(tr("Remove"));
    
    batchButtonsLayout->addWidget(addToBatchButton_);
    batchButtonsLayout->addWidget(removeFromBatchButton_);
    
    processBatchButton_ = new QPushButton(tr("Process Batch"));
    processBatchButton_->setEnabled(false);
    
    batchStatusLabel_ = new QLabel(tr("No images in batch queue"));
    batchStatusLabel_->setAlignment(Qt::AlignCenter);
    
    batchLayout->addWidget(new QLabel(tr("Batch Queue:")));
    batchLayout->addWidget(batchListWidget_);
    batchLayout->addLayout(batchButtonsLayout);
    batchLayout->addWidget(processBatchButton_);
    batchLayout->addWidget(batchStatusLabel_);
    
    rightLayout->addWidget(batchGroupBox_);
    rightLayout->addStretch();
    
    // Add all panels to the splitter
    mainSplitter_->addWidget(leftWidget);
    mainSplitter_->addWidget(centerWidget);
    mainSplitter_->addWidget(rightWidget);
    
    // Set initial sizes (25% left, 50% center, 25% right)
    mainSplitter_->setSizes({250, 500, 250});
    
    mainLayout->addWidget(mainSplitter_);
    
    // Set initial button states
    resetFilterButton_->setEnabled(false);
    applyFilterButton_->setEnabled(false);
    
    // Button styling - use the same style as existing pages
    QString buttonBgColor = isDarkTheme ? "#444444" : "#f0f0f0";
    QString buttonBgHoverColor = isDarkTheme ? "#555555" : "#e0e0e0";
    QString buttonBgPressedColor = isDarkTheme ? "#333333" : "#d0d0d0";
    QString buttonDisabledBgColor = isDarkTheme ? "#383838" : "#f8f8f8";
    QString buttonDisabledTextColor = isDarkTheme ? "#777777" : "#bbbbbb";
    
    QString buttonStyle = QString(
        "QPushButton { background-color: %1; border: 1px solid %5; border-radius: 4px; padding: 6px 12px; color: %6; } "
        "QPushButton:hover { background-color: %2; } "
        "QPushButton:pressed { background-color: %3; } "
        "QPushButton:disabled { background-color: %4; color: %7; }"
    ).arg(buttonBgColor, buttonBgHoverColor, buttonBgPressedColor, buttonDisabledBgColor, 
          borderColor, textColor, buttonDisabledTextColor);
    
    QList<QPushButton*> buttons = findChildren<QPushButton*>();
    for (auto button : buttons) {
        button->setStyleSheet(buttonStyle);
        button->setCursor(Qt::PointingHandCursor);
    }
}

void ImageProcessingPage::createConnections() {
    // Filter controls
    connect(filterCategoryCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageProcessingPage::onFilterCategoryChanged);
    connect(filtersListWidget_, &QListWidget::currentRowChanged,
            this, &ImageProcessingPage::onFilterSelected);
    connect(applyFilterButton_, &QPushButton::clicked,
            this, &ImageProcessingPage::onApplyFilter);
    connect(resetFilterButton_, &QPushButton::clicked,
            this, &ImageProcessingPage::onResetFilters);
    connect(saveProfileButton_, &QPushButton::clicked,
            this, &ImageProcessingPage::onSaveFilterProfile);
    connect(loadProfileButton_, &QPushButton::clicked,
            this, &ImageProcessingPage::onLoadFilterProfile);
    
    // Image controls
    connect(openImageButton_, &QPushButton::clicked,
            this, &ImageProcessingPage::onOpenImage);
    connect(saveImageButton_, &QPushButton::clicked,
            this, &ImageProcessingPage::onSaveImage);
    connect(beforeAfterToggle_, &QCheckBox::toggled,
            this, &ImageProcessingPage::onToggleBeforeAfter);
    connect(zoomSlider_, &QSlider::valueChanged, [this](int value) {
        double scaleFactor = std::pow(1.01, value);
        QTransform transform;
        transform.scale(scaleFactor, scaleFactor);
        imageView_->setTransform(transform);
    });
    
    // Batch processing
    connect(addToBatchButton_, &QPushButton::clicked,
            this, &ImageProcessingPage::onAddToBatch);
    connect(removeFromBatchButton_, &QPushButton::clicked,
            this, &ImageProcessingPage::onRemoveFromBatch);
    connect(processBatchButton_, &QPushButton::clicked,
            this, &ImageProcessingPage::onStartBatchProcessing);
}

void ImageProcessingPage::loadSettings() {
    QSettings settings;
    currentFilterCategory_ = settings.value("imageProcessing/filterCategory", 0).toInt();
    filterCategoryCombo_->setCurrentIndex(currentFilterCategory_);
}

void ImageProcessingPage::saveSettings() {
    QSettings settings;
    settings.setValue("imageProcessing/filterCategory", filterCategoryCombo_->currentIndex());
}

void ImageProcessingPage::updateFilterControls() {
    // Clear and populate the filter list based on the selected category
    filtersListWidget_->clear();
    currentFilterIndex_ = -1;
    
    // Get the selected category
    int index = filterCategoryCombo_->currentIndex();
    if (index < 0) return;
    
    core::Filter::Category category = static_cast<core::Filter::Category>(
        filterCategoryCombo_->itemData(index).toInt());
    
    // Get filters for this category
    QStringList filterNames = core::FilterRegistry::instance().getFilterNamesByCategory(category);
    filtersListWidget_->addItems(filterNames);
    
    // Disable apply/reset buttons until a filter is selected
    applyFilterButton_->setEnabled(false);
    resetFilterButton_->setEnabled(!appliedFilters_.isEmpty());
}

void ImageProcessingPage::onFilterCategoryChanged(int index) {
    if (index < 0) return;
    
    currentFilterCategory_ = index;
    updateFilterControls();
}

void ImageProcessingPage::onFilterSelected(int index) {
    currentFilterIndex_ = index;
    
    // Get the selected filter
    if (index < 0) {
        filterControlsStack_->setCurrentIndex(0); // Show "no filter selected" page
        applyFilterButton_->setEnabled(false);
        return;
    }
    
    QString filterName = filtersListWidget_->item(index)->text();
    core::FilterPtr filter = core::FilterRegistry::instance().getFilter(filterName);
    
    if (!filter) {
        filterControlsStack_->setCurrentIndex(0);
        applyFilterButton_->setEnabled(false);
        return;
    }
    
    // Create or use existing control widget for this filter
    QWidget* controlWidget = nullptr;
    
    // Check if we already have a control widget for this filter
    for (int i = 1; i < filterControlsStack_->count(); i++) {
        QWidget* widget = filterControlsStack_->widget(i);
        if (widget->property("filterName").toString() == filterName) {
            controlWidget = widget;
            filterControlsStack_->setCurrentWidget(controlWidget);
            break;
        }
    }
    
    // Create a new control widget if needed
    if (!controlWidget) {
        controlWidget = filter->createControlWidget(filterControlsStack_);
        controlWidget->setProperty("filterName", filterName);
        filterControlsStack_->addWidget(controlWidget);
        filterControlsStack_->setCurrentWidget(controlWidget);
    }
    
    // Enable apply button if we have an image
    bool hasImage = !originalImage_.isNull();
    applyFilterButton_->setEnabled(hasImage);
}

void ImageProcessingPage::onApplyFilter() {
    if (originalImage_.isNull() || currentFilterIndex_ < 0) {
        return;
    }
    
    // Get the selected filter
    QString filterName = filtersListWidget_->item(currentFilterIndex_)->text();
    core::FilterPtr filter = core::FilterRegistry::instance().getFilter(filterName);
    
    if (!filter) {
        return;
    }
    
    // Get the parameters from the control widget
    QVariantMap parameters;
    QWidget* controlWidget = filterControlsStack_->currentWidget();
    
    if (controlWidget) {
        // Get the parameters using the function stored in the widget property
        QVariant getParamsVariant = controlWidget->property("getParameters");
        if (getParamsVariant.isValid()) {
            // Call the lambda function to get parameters
            std::function<QVariantMap()> getParams = getParamsVariant.value<std::function<QVariantMap()>>();
            if (getParams) {
                parameters = getParams();
            }
        }
    }
    
    // If no custom parameters, use defaults
    if (parameters.isEmpty()) {
        parameters = filter->defaultParameters();
    }
    
    // Add the filter to the applied filters list
    appliedFilters_.append(qMakePair(filterName, parameters));
    
    // Apply the filter pipeline to the image
    processedImage_ = applyFilters(originalImage_);
    
    // Update the preview
    imageScene_->clear();
    imageScene_->addPixmap(QPixmap::fromImage(processedImage_));
    imageView_->setSceneRect(imageScene_->itemsBoundingRect());
    imageView_->fitInView(imageScene_->itemsBoundingRect(), Qt::KeepAspectRatio);
    
    // Enable save and reset buttons
    saveImageButton_->setEnabled(true);
    resetFilterButton_->setEnabled(true);
    
    // Log the action
    logMessage(tr("Applied filter: %1").arg(filterName));
}

void ImageProcessingPage::onResetFilters() {
    if (originalImage_.isNull()) {
        return;
    }
    
    // Clear the filter list
    appliedFilters_.clear();
    
    // Reset to original image
    processedImage_ = originalImage_;
    
    // Update the preview
    imageScene_->clear();
    imageScene_->addPixmap(QPixmap::fromImage(originalImage_));
    imageView_->setSceneRect(imageScene_->itemsBoundingRect());
    imageView_->fitInView(imageScene_->itemsBoundingRect(), Qt::KeepAspectRatio);
    
    // Update button states
    resetFilterButton_->setEnabled(false);
    
    // Log the action
    logMessage(tr("Reset all filters"));
}

void ImageProcessingPage::onOpenImage() {
    QString lastDir = QSettings().value("imageProcessing/lastDir", QDir::homePath()).toString();
    
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open Image"), lastDir,
        tr("Image Files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
    
    if (filePath.isEmpty()) {
        return;
    }
    
    // Save the directory for next time
    QFileInfo fileInfo(filePath);
    QSettings().setValue("imageProcessing/lastDir", fileInfo.absolutePath());
    
    // Load the image
    QImage newImage(filePath);
    if (newImage.isNull()) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load image: %1").arg(filePath));
        return;
    }
    
    // Store the image and update the UI
    originalImage_ = newImage;
    processedImage_ = newImage;
    appliedFilters_.clear();
    
    // Update the image view
    imageScene_->clear();
    imageScene_->addPixmap(QPixmap::fromImage(newImage));
    imageView_->setSceneRect(imageScene_->itemsBoundingRect());
    imageView_->fitInView(imageScene_->itemsBoundingRect(), Qt::KeepAspectRatio);
    
    // Update UI states
    saveImageButton_->setEnabled(false);
    resetFilterButton_->setEnabled(false);
    applyFilterButton_->setEnabled(currentFilterIndex_ >= 0);
    
    // Log the action
    logMessage(tr("Opened image: %1").arg(fileInfo.fileName()));
    
    // Handle as if we had received this image from another source
    onImageLoaded(newImage);
}

void ImageProcessingPage::onSaveImage() {
    if (processedImage_.isNull()) {
        return;
    }
    
    QString lastDir = QSettings().value("imageProcessing/lastDir", QDir::homePath()).toString();
    QString suggestedName = lastDir + "/processed_image.png";
    
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Processed Image"), suggestedName,
        tr("PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp)"));
    
    if (filePath.isEmpty()) {
        return;
    }
    
    // Save the image
    if (!processedImage_.save(filePath)) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to save image to: %1").arg(filePath));
        return;
    }
    
    // Log the action
    QFileInfo fileInfo(filePath);
    logMessage(tr("Saved processed image to: %1").arg(fileInfo.fileName()));
}

void ImageProcessingPage::onToggleBeforeAfter(bool showBefore) {
    if (originalImage_.isNull()) {
        return;
    }
    
    // Switch between showing the original and processed image
    imageScene_->clear();
    if (showBefore) {
        imageScene_->addPixmap(QPixmap::fromImage(originalImage_));
    } else {
        imageScene_->addPixmap(QPixmap::fromImage(processedImage_));
    }
    
    // Maintain the current view
    imageView_->setSceneRect(imageScene_->itemsBoundingRect());
}

void ImageProcessingPage::onAddToBatch() {
    QString lastDir = QSettings().value("imageProcessing/lastDir", QDir::homePath()).toString();
    
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this, tr("Add Images to Batch"), lastDir,
        tr("Image Files (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
    
    if (filePaths.isEmpty()) {
        return;
    }
    
    // Save the directory for next time
    QFileInfo fileInfo(filePaths.first());
    QSettings().setValue("imageProcessing/lastDir", fileInfo.absolutePath());
    
    // Add the files to the batch list
    for (const auto& path : filePaths) {
        QFileInfo info(path);
        QListWidgetItem* item = new QListWidgetItem(info.fileName());
        item->setData(Qt::UserRole, path);
        batchListWidget_->addItem(item);
    }
    
    // Update status
    int count = batchListWidget_->count();
    batchStatusLabel_->setText(tr("%1 image(s) in queue").arg(count));
    processBatchButton_->setEnabled(count > 0 && !appliedFilters_.isEmpty());
    
    // Log the action
    logMessage(tr("Added %1 image(s) to batch queue").arg(filePaths.size()));
}

void ImageProcessingPage::onRemoveFromBatch() {
    QList<QListWidgetItem*> selectedItems = batchListWidget_->selectedItems();
    
    if (selectedItems.isEmpty()) {
        return;
    }
    
    // Remove selected items
    for (auto item : selectedItems) {
        delete batchListWidget_->takeItem(batchListWidget_->row(item));
    }
    
    // Update status
    int count = batchListWidget_->count();
    batchStatusLabel_->setText(count > 0 ? tr("%1 image(s) in queue").arg(count) : tr("No images in batch queue"));
    processBatchButton_->setEnabled(count > 0 && !appliedFilters_.isEmpty());
    
    // Log the action
    logMessage(tr("Removed %1 image(s) from batch queue").arg(selectedItems.size()));
}

void ImageProcessingPage::onStartBatchProcessing() {
    int count = batchListWidget_->count();
    
    if (count == 0 || appliedFilters_.isEmpty()) {
        return;
    }
    
    // Ask for output directory
    QString lastDir = QSettings().value("imageProcessing/lastDir", QDir::homePath()).toString();
    QString outputDir = QFileDialog::getExistingDirectory(
        this, tr("Select Output Directory"), lastDir);
    
    if (outputDir.isEmpty()) {
        return;
    }
    
    // Process each image
    int successCount = 0;
    for (int i = 0; i < count; ++i) {
        QString imagePath = batchListWidget_->item(i)->data(Qt::UserRole).toString();
        QFileInfo fileInfo(imagePath);
        
        // Load the image
        QImage sourceImage(imagePath);
        if (sourceImage.isNull()) {
            logMessage(tr("Failed to load image: %1").arg(fileInfo.fileName()), "ERROR");
            continue;
        }
        
        // Apply filters
        QImage processedImage = applyFilters(sourceImage);
        
        // Save to output directory
        QString outputPath = outputDir + "/processed_" + fileInfo.fileName();
        if (processedImage.save(outputPath)) {
            successCount++;
            logMessage(tr("Processed: %1").arg(fileInfo.fileName()));
        } else {
            logMessage(tr("Failed to save processed image: %1").arg(fileInfo.fileName()), "ERROR");
        }
    }
    
    // Log completion
    logMessage(tr("Batch processing completed: %1 of %2 images processed successfully").arg(successCount).arg(count));
    batchStatusLabel_->setText(tr("Batch processing complete"));
    
    // Notify about completion
    onBatchProcessingComplete();
}

void ImageProcessingPage::onBatchProcessingComplete() {
    QMessageBox::information(this, tr("Batch Processing Complete"),
                           tr("Processed %1 of %2 images successfully.")
                           .arg(batchListWidget_->count())
                           .arg(batchListWidget_->count()));
}

void ImageProcessingPage::onImageLoaded(const QImage& image) {
    // This method would be called when an image is loaded from another source
    // such as a camera capture. For now, it's just the same as manually opening an image.
    originalImage_ = image;
    processedImage_ = image;
    appliedFilters_.clear();
    
    // Update the image view
    imageScene_->clear();
    imageScene_->addPixmap(QPixmap::fromImage(image));
    imageView_->setSceneRect(imageScene_->itemsBoundingRect());
    imageView_->fitInView(imageScene_->itemsBoundingRect(), Qt::KeepAspectRatio);
    
    // Update UI states
    saveImageButton_->setEnabled(false);
    resetFilterButton_->setEnabled(false);
    applyFilterButton_->setEnabled(currentFilterIndex_ >= 0);
}

void ImageProcessingPage::onSaveFilterProfile() {
    if (appliedFilters_.isEmpty()) {
        QMessageBox::information(this, tr("No Filters"), tr("No filters have been applied to save as a profile."));
        return;
    }
    
    QString profileName = QInputDialog::getText(this, tr("Save Filter Profile"),
                                           tr("Profile Name:"), QLineEdit::Normal);
    
    if (profileName.isEmpty()) {
        return;
    }
    
    // In a real implementation, you would serialize the filter list to settings or a file
    QSettings settings;
    settings.beginGroup("imageProcessing/profiles");
    settings.beginGroup(profileName);
    
    settings.setValue("filterCount", appliedFilters_.size());
    for (int i = 0; i < appliedFilters_.size(); ++i) {
        settings.setValue(QString("filter%1/name").arg(i), appliedFilters_[i].first);
        settings.setValue(QString("filter%1/parameters").arg(i), appliedFilters_[i].second);
    }
    
    settings.endGroup();
    settings.endGroup();
    
    logMessage(tr("Saved filter profile: %1").arg(profileName));
}

void ImageProcessingPage::onLoadFilterProfile() {
    QSettings settings;
    settings.beginGroup("imageProcessing/profiles");
    QStringList profiles = settings.childGroups();
    settings.endGroup();
    
    if (profiles.isEmpty()) {
        QMessageBox::information(this, tr("No Profiles"), tr("No saved filter profiles found."));
        return;
    }
    
    QString profileName = QInputDialog::getItem(this, tr("Load Filter Profile"),
                                           tr("Select Profile:"), profiles, 0, false);
    
    if (profileName.isEmpty()) {
        return;
    }
    
    // Clear current filters
    appliedFilters_.clear();
    
    // Load the filter profile
    settings.beginGroup("imageProcessing/profiles");
    settings.beginGroup(profileName);
    
    int filterCount = settings.value("filterCount", 0).toInt();
    for (int i = 0; i < filterCount; ++i) {
        QString name = settings.value(QString("filter%1/name").arg(i)).toString();
        QVariantMap parameters = settings.value(QString("filter%1/parameters").arg(i)).toMap();
        
        appliedFilters_.append(qMakePair(name, parameters));
    }
    
    settings.endGroup();
    settings.endGroup();
    
    // Apply the filters if we have an image
    if (!originalImage_.isNull()) {
        processedImage_ = applyFilters(originalImage_);
        
        // Update the preview
        imageScene_->clear();
        imageScene_->addPixmap(QPixmap::fromImage(processedImage_));
        imageView_->setSceneRect(imageScene_->itemsBoundingRect());
        
        // Enable save and reset buttons
        saveImageButton_->setEnabled(true);
        resetFilterButton_->setEnabled(true);
    }
    
    logMessage(tr("Loaded filter profile: %1 with %2 filters").arg(profileName).arg(filterCount));
}

QImage ImageProcessingPage::applyFilters(const QImage& sourceImage) {
    if (appliedFilters_.isEmpty()) {
        return sourceImage;
    }
    
    // Start with the source image
    QImage result = sourceImage;
    
    // Apply each filter in sequence
    for (const auto& filterPair : appliedFilters_) {
        QString filterName = filterPair.first;
        QVariantMap parameters = filterPair.second;
        
        // Get the filter from the registry
        core::FilterPtr filter = core::FilterRegistry::instance().getFilter(filterName);
        if (filter) {
            // Apply the filter with the stored parameters
            result = filter->apply(result, parameters);
        }
    }
    
    return result;
}

void ImageProcessingPage::onParameterChanged() {
    // This would be called when any filter parameter is changed
    // For now, we'll just enable the apply button if a filter is selected
    applyFilterButton_->setEnabled(currentFilterIndex_ >= 0 && !originalImage_.isNull());
}

void ImageProcessingPage::logMessage(const QString& message, const QString& type) {
    // Emit the status signal for the main window to display
    emit statusChanged(message);
    
    // In a real implementation, you might also log to a text widget or file
}

} // namespace cam_matrix::ui 