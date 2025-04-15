#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QTimer>
#include "core/sapera_defs.hpp"

namespace cam_matrix::ui {

class SaperaStatusWidget : public QWidget {
    Q_OBJECT

public:
    explicit SaperaStatusWidget(QWidget* parent = nullptr);
    ~SaperaStatusWidget() override = default;

public slots:
    void refresh();
    void showStatusDetails();

signals:
    void statusChanged(const QString& status);

private:
    void setupUi();

    QLabel* titleLabel_;
    QLabel* saperaStatusLabel_;
    QLabel* saperaVersionLabel_;
    QLabel* gigeStatusLabel_;
    QLabel* gigeVersionLabel_;
    QLabel* cameraCountLabel_;
    QPushButton* refreshButton_;
    QPushButton* testConnectionButton_;
    QTimer* autoRefreshTimer_;
    bool isSaperaConnected_;
    bool isGigeConnected_;
};

} // namespace cam_matrix::ui 