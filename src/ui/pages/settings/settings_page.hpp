#pragma once

#include "ui/pages/page.hpp"
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <memory>

namespace cam_matrix::ui {

class SettingsPage : public Page {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);
    ~SettingsPage() override;

    QString title() const override { return tr("Settings"); }
    void initialize() override;
    void cleanup() override;

private slots:
    void onSettingsChanged();

private:
    void setupUi() override;
    void createConnections() override;
    void loadSettings();
    void saveSettings();

    // Capture settings
    QComboBox* resolutionCombo_;
    QSpinBox* exposureSpin_;
    QCheckBox* hardwareTrigger_;
};

} // namespace cam_matrix::ui 