#pragma once

#include <QMainWindow>
#include <QSharedPointer>

class QStatusBar;
class QStackedWidget;
class QTabWidget;

namespace cam_matrix::ui {
class Page;
}

namespace cam_matrix {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

public slots:
    void refreshCameras();
    void showStatusMessage(const QString& message, int timeout = 3000);

private slots:
    void onPageStatusChanged(const QString& message);
    void onPageError(const QString& message);
    void onAbout();

private:
    void loadFonts();
    void setupUi();
    void createStatusBar();
    void addPage(ui::Page* page, const QString& title, const QString& iconName);
    void connectPageSignals(ui::Page* page);

    QStatusBar* statusBar_;
    QTabWidget* tabWidget_;
};

} // namespace cam_matrix 