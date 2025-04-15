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

signals:
    void statusChanged(const QString& status);

private:
    void setupUi();
    void updateStatus();

    QLabel* titleLabel_;
    QLabel* statusLabel_;
    QLabel* versionLabel_;
    QLabel* cameraCountLabel_;
    QPushButton* refreshButton_;
    QPushButton* testConnectionButton_;
    QTimer* autoRefreshTimer_;
    bool isConnected_;
};

} // namespace cam_matrix::ui 