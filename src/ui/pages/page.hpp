#pragma once

#include <QWidget>
#include <QString>
#include <memory>

namespace cam_matrix::ui {

class Page : public QWidget {
    Q_OBJECT

public:
    explicit Page(QWidget* parent = nullptr);
    virtual ~Page() = default;

    virtual QString title() const = 0;
    virtual void initialize();
    virtual void cleanup();

protected:
    virtual void setupUi() = 0;
    virtual void createConnections() = 0;

signals:
    void statusChanged(const QString& message);
    void error(const QString& message);
};

} // namespace cam_matrix::ui
