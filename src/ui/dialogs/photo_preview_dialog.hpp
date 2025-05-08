#pragma once

#include <QDialog>
#include <QImage>
#include <QString>

class QLabel;
class QPushButton;

namespace cam_matrix::ui {

/**
 * Dialog for previewing captured photos
 */
class PhotoPreviewDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * Constructor
     * @param image The image to preview
     * @param path The path where the image was saved
     * @param parent The parent widget
     */
    explicit PhotoPreviewDialog(const QImage& image, const QString& path, QWidget* parent = nullptr);
    ~PhotoPreviewDialog() override = default;

private slots:
    void onSaveAsClicked();
    void onCopyToClipboardClicked();

private:
    void setupUi();
    void createConnections();
    
    QImage image_;
    QString path_;
    QLabel* imageLabel_;
    QLabel* pathLabel_;
    QPushButton* saveAsButton_;
    QPushButton* copyButton_;
    QPushButton* closeButton_;
};

} // namespace cam_matrix::ui 