#include "settings_page.hpp"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

namespace cam_matrix::ui {

SettingsPage::SettingsPage(QWidget* parent)
    : Page(parent)
    , resolutionCombo_(nullptr)
    , exposureSpin_(nullptr)
    , hardwareTrigger_(nullptr)
{
}

SettingsPage::~SettingsPage() = default;

void SettingsPage::setupUi()
{
    auto* layout = new QVBoxLayout(this);

    // Capture settings group
    auto* captureGroup = new QGroupBox(tr("Capture Settings"), this);
    auto* captureLayout = new QFormLayout(captureGroup);

    resolutionCombo_ = new QComboBox(this);
    resolutionCombo_->addItems({
        "1920x1080",
        "1280x720",
        "640x480"
    });
    captureLayout->addRow(tr("Resolution:"), resolutionCombo_);

    exposureSpin_ = new QSpinBox(this);
    exposureSpin_->setRange(1, 1000);
    exposureSpin_->setValue(100);
    exposureSpin_->setSuffix(tr(" ms"));
    captureLayout->addRow(tr("Exposure:"), exposureSpin_);

    layout->addWidget(captureGroup);

    // Sync settings group
    auto* syncGroup = new QGroupBox(tr("Synchronization"), this);
    auto* syncLayout = new QVBoxLayout(syncGroup);

    hardwareTrigger_ = new QCheckBox(tr("Use Hardware Trigger"), this);
    syncLayout->addWidget(hardwareTrigger_);

    layout->addWidget(syncGroup);
    layout->addStretch();
}

void SettingsPage::createConnections()
{
    connect(resolutionCombo_, &QComboBox::currentTextChanged,
            this, &SettingsPage::onSettingsChanged);
    connect(exposureSpin_, &QSpinBox::valueChanged,
            this, &SettingsPage::onSettingsChanged);
    connect(hardwareTrigger_, &QCheckBox::toggled,
            this, &SettingsPage::onSettingsChanged);
}

void SettingsPage::onSettingsChanged()
{
    emit statusChanged(tr("Settings changed"));
    // TODO: Apply settings to cameras
}

void SettingsPage::initialize() {
    Page::initialize();
    loadSettings();
}

void SettingsPage::cleanup() {
    saveSettings();
    Page::cleanup();
}

void SettingsPage::loadSettings() {
    // Load settings (implement later)
}

void SettingsPage::saveSettings() {
    // Save settings (implement later)
}

} // namespace cam_matrix::ui
