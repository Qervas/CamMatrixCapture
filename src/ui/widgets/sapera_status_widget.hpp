#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QTimer>
#include <QVBoxLayout>
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
    void setExpanded(bool expanded);
    bool isExpanded() const;

signals:
    void statusChanged(const QString& status);
    void expandedChanged(bool expanded);

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
    QPushButton* toggleButton_;
    QWidget* contentContainer_;
    QTimer* autoRefreshTimer_;
    bool isSaperaConnected_;
    bool isGigeConnected_;
    bool isExpanded_;
};

} // namespace cam_matrix::ui 