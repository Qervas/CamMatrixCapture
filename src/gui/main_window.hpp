#pragma once

#include <QMainWindow>
#include <QSharedPointer>

class QStatusBar;

namespace cam_matrix::ui {
class Page;
}

namespace cam_matrix {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onPageStatusChanged(const QString& message);
    void onPageError(const QString& message);
    void onAbout();

private:
    void setupUi();
    void createStatusBar();
    void connectPageSignals(ui::Page* page);

    QStatusBar* statusBar_;
};

} // namespace cam_matrix 