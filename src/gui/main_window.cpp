#include "main_window.hpp"
#include "ui/pages/page.hpp"
#include "ui/pages/camera/camera_page.hpp"
#include "ui/pages/settings/settings_page.hpp"
#include "ui/pages/capture/capture_page.hpp"
#include "ui/pages/calibration/calibration_page.hpp"
#include "ui/pages/dataset/dataset_page.hpp"

#include <QTabWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QMenuBar>
#include <QMessageBox>

namespace cam_matrix {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , tabWidget_(nullptr)
    , statusBar_(nullptr)
    , mainToolBar_(nullptr)
{
    setupUi();
    createMenus();
    createToolBar();
    createStatusBar();
    registerPages();
    
    setWindowTitle(tr("Camera Matrix Capture"));
    resize(1024, 768);
}

void MainWindow::setupUi()
{
    tabWidget_ = new QTabWidget(this);
    setCentralWidget(tabWidget_);
}

void MainWindow::createMenus()
{
    fileMenu_ = menuBar()->addMenu(tr("&File"));
    fileMenu_->addAction(tr("&New Project"), QKeySequence::New, this, []() { /* TODO */ });
    fileMenu_->addAction(tr("&Save Settings"), QKeySequence::Save, this, []() { /* TODO */ });
    fileMenu_->addSeparator();
    fileMenu_->addAction(tr("E&xit"), QKeySequence::Quit, this, &QMainWindow::close);

    editMenu_ = menuBar()->addMenu(tr("&Edit"));
    editMenu_->addAction(tr("&Preferences"), this, []() { /* TODO */ });

    helpMenu_ = menuBar()->addMenu(tr("&Help"));
    helpMenu_->addAction(tr("&About"), this, &MainWindow::onAbout);
}

void MainWindow::createToolBar()
{
    mainToolBar_ = addToolBar(tr("Main Toolbar"));
    mainToolBar_->setMovable(false);
    // Toolbar actions will be added by individual pages
}

void MainWindow::createStatusBar()
{
    statusBar_ = new QStatusBar(this);
    setStatusBar(statusBar_);
    statusBar_->showMessage(tr("Ready"));
}

void MainWindow::registerPages()
{
    // Create and register all pages
    auto registerPage = [this](QSharedPointer<ui::Page> page) {
        if (!page) return;
        
        connectPageSignals(page.get());
        QString title = page->title();
        tabWidget_->addTab(page.get(), title);
        pages_[title] = page;
    };

    registerPage(QSharedPointer<ui::Page>(new ui::CameraPage()));
    registerPage(QSharedPointer<ui::Page>(new ui::SettingsPage()));
    registerPage(QSharedPointer<ui::Page>(new ui::CapturePage()));
    registerPage(QSharedPointer<ui::Page>(new ui::CalibrationPage()));
    registerPage(QSharedPointer<ui::Page>(new ui::DatasetPage()));

    // Initialize all pages
    QHashIterator<QString, QSharedPointer<ui::Page>> it(pages_);
    while (it.hasNext()) {
        it.next();
        it.value()->initialize();
    }
}

void MainWindow::connectPageSignals(ui::Page* page)
{
    if (!page) return;
    
    connect(page, &ui::Page::statusChanged,
            this, &MainWindow::onPageStatusChanged);
    connect(page, &ui::Page::error,
            this, &MainWindow::onPageError);
}

void MainWindow::onPageStatusChanged(const QString& message)
{
    statusBar_->showMessage(message);
}

void MainWindow::onPageError(const QString& message)
{
    QMessageBox::critical(this, tr("Error"), message);
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About Camera Matrix Capture"),
                      tr("Camera Matrix Capture v1.0.0\n"
                         "A professional tool for synchronized "
                         "multi-camera capture and calibration."));
}

} // namespace cam_matrix 