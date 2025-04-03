#include "dataset_page.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QDir>
#include <QTimer>

namespace cam_matrix::ui {

DatasetPage::DatasetPage(QWidget* parent)
    : Page(parent)
    , outputDirEdit_(nullptr)
    , browseButton_(nullptr)
    , imageList_(nullptr)
    , exportButton_(nullptr)
{
}

DatasetPage::~DatasetPage() = default;

void DatasetPage::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    // Output directory selection
    auto* dirLayout = new QHBoxLayout;
    outputDirEdit_ = new QLineEdit(QDir::currentPath(), this);
    browseButton_ = new QPushButton(tr("Browse..."), this);
    dirLayout->addWidget(outputDirEdit_);
    dirLayout->addWidget(browseButton_);
    layout->addLayout(dirLayout);

    // Image list
    imageList_ = new QTableWidget(0, 3, this);
    imageList_->setHorizontalHeaderLabels({
        tr("Camera"),
        tr("Timestamp"),
        tr("File Path")
    });
    imageList_->horizontalHeader()->setStretchLastSection(true);
    imageList_->verticalHeader()->setVisible(false);
    layout->addWidget(imageList_);

    // Export button
    exportButton_ = new QPushButton(tr("Export Dataset"), this);
    layout->addWidget(exportButton_);
}

void DatasetPage::createConnections()
{
    connect(browseButton_, &QPushButton::clicked,
            this, &DatasetPage::onBrowseOutputDir);
    connect(outputDirEdit_, &QLineEdit::textChanged,
            this, &DatasetPage::onOutputDirChanged);
    connect(exportButton_, &QPushButton::clicked,
            this, &DatasetPage::onExportDataset);
}

void DatasetPage::onBrowseOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        outputDirEdit_->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        outputDirEdit_->setText(dir);
    }
}

void DatasetPage::onOutputDirChanged()
{
    emit statusChanged(tr("Output directory changed to: %1")
                      .arg(outputDirEdit_->text()));
}

void DatasetPage::onExportDataset()
{
    emit statusChanged(tr("Exporting dataset..."));
    // TODO: Implement actual export
    QTimer::singleShot(1000, [this]() {
        emit statusChanged(tr("Dataset exported successfully"));
    });
}

void DatasetPage::initialize() {
    Page::initialize();
    loadSettings();
}

void DatasetPage::cleanup() {
    saveSettings();
    Page::cleanup();
}

void DatasetPage::loadSettings() {
    // Load settings (implement later)
}

void DatasetPage::saveSettings() {
    // Save settings (implement later)
}

} // namespace cam_matrix::ui
