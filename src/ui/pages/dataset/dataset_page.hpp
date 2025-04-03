#pragma once

#include "ui/pages/page.hpp"
#include <QLineEdit>
#include <QTableWidget>
#include <QPushButton>
#include <memory>

namespace cam_matrix::ui {

class DatasetPage : public Page {
    Q_OBJECT

public:
    explicit DatasetPage(QWidget* parent = nullptr);
    ~DatasetPage() override;

    QString title() const override { return tr("Dataset"); }
    void initialize() override;
    void cleanup() override;

private slots:
    void onBrowseOutputDir();
    void onOutputDirChanged();
    void onExportDataset();

private:
    void setupUi() override;
    void createConnections() override;
    void loadSettings();
    void saveSettings();

    QLineEdit* outputDirEdit_;
    QPushButton* browseButton_;
    QTableWidget* imageList_;
    QPushButton* exportButton_;
};

} // namespace cam_matrix::ui 