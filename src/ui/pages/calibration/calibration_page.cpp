#include "calibration_page.hpp"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QTimer>

namespace cam_matrix::ui {

CalibrationPage::CalibrationPage(QWidget* parent)
    : Page(parent)
    , patternView_(nullptr)
    , resultsTable_(nullptr)
    , captureButton_(nullptr)
{
}

CalibrationPage::~CalibrationPage() = default;

void CalibrationPage::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    patternView_ = new QGraphicsView(this);
    patternView_->setMinimumSize(640, 480);
    layout->addWidget(patternView_);

    captureButton_ = new QPushButton(tr("Capture Calibration Image"), this);
    layout->addWidget(captureButton_);

    resultsTable_ = new QTableWidget(0, 2, this);
    resultsTable_->setHorizontalHeaderLabels({tr("Parameter"), tr("Value")});
    resultsTable_->horizontalHeader()->setStretchLastSection(true);
    resultsTable_->verticalHeader()->setVisible(false);
    layout->addWidget(resultsTable_);
}

void CalibrationPage::createConnections()
{
    connect(captureButton_, &QPushButton::clicked,
            this, &CalibrationPage::onCaptureCalibration);
}

void CalibrationPage::onCaptureCalibration()
{
    emit statusChanged(tr("Capturing calibration image..."));
    // TODO: Implement actual calibration
    QTimer::singleShot(1000, this, &CalibrationPage::onCalibrationComplete);
}

void CalibrationPage::onCalibrationComplete()
{
    // Add dummy results
    resultsTable_->setRowCount(3);
    int row = 0;
    auto addRow = [this, &row](const QString& param, const QString& value) {
        resultsTable_->setItem(row, 0, new QTableWidgetItem(param));
        resultsTable_->setItem(row, 1, new QTableWidgetItem(value));
        row++;
    };

    addRow(tr("Focal Length"), "1024.5 px");
    addRow(tr("Principal Point"), "(512, 384)");
    addRow(tr("Distortion"), "k1=-0.1, k2=0.01");

    emit statusChanged(tr("Calibration complete"));
}

void CalibrationPage::initialize() {
    Page::initialize();
}

void CalibrationPage::cleanup() {
    Page::cleanup();
}

} // namespace cam_matrix::ui
