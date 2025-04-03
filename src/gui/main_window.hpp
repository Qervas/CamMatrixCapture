#pragma once

#include "ui/pages/page.hpp"
#include <QMainWindow>
#include <QHash>
#include <QSharedPointer>
#include <QString>

class QTabWidget;
class QStatusBar;
class QToolBar;
class QMenu;

namespace cam_matrix {

namespace ui {
class Page;  // Forward declaration
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onPageStatusChanged(const QString& message);
    void onPageError(const QString& message);
    void onAbout();

private:
    void setupUi();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void registerPages();
    void connectPageSignals(ui::Page* page);

    // Core UI elements
    QTabWidget* tabWidget_;
    QStatusBar* statusBar_;
    QToolBar* mainToolBar_;
    
    // Menus
    QMenu* fileMenu_;
    QMenu* editMenu_;
    QMenu* helpMenu_;

    // Page management
    QHash<QString, QSharedPointer<ui::Page>> pages_;
};

} // namespace cam_matrix 