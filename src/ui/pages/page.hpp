#pragma once

#include <QWidget>
#include <QString>

namespace cam_matrix::ui {

class Page : public QWidget {
    Q_OBJECT

public:
    explicit Page(QWidget* parent = nullptr);
    ~Page() override;

    // Title of the page
    virtual QString title() const = 0;
    
    // Initialize the page
    virtual void initialize();

public slots:
    // Public slot for refreshing cameras (override in derived classes if needed)
    virtual void refreshCameras() {}

signals:
    void statusChanged(const QString& status);
    void error(const QString& error);

protected:
    virtual void setupUi() = 0;
    virtual void createConnections() = 0;
    virtual void cleanup();
};

} // namespace cam_matrix::ui
