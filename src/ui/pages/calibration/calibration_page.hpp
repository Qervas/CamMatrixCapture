#pragma once

#include "ui/pages/page.hpp"
#include <QGraphicsView>
#include <QTableWidget>
#include <QPushButton>
#include <memory>

namespace cam_matrix::ui {

class CalibrationPage : public Page {
    Q_OBJECT

public:
    explicit CalibrationPage(QWidget* parent = nullptr);
    ~CalibrationPage() override;

    QString title() const override { return tr("Calibration"); }
    void initialize() override;
    void cleanup() override;

private slots:
    void onCaptureCalibration();
    void onCalibrationComplete();

private:
    void setupUi() override;
    void createConnections() override;

    QGraphicsView* patternView_;
    QTableWidget* resultsTable_;
    QPushButton* captureButton_;
};

} // namespace cam_matrix::ui 