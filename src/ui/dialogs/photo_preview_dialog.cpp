#include "ui/dialogs/photo_preview_dialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QScrollArea>

namespace cam_matrix::ui {

PhotoPreviewDialog::PhotoPreviewDialog(const QImage& image, const QString& path, QWidget* parent)
    : QDialog(parent)
    , image_(image)
    , path_(path)
{
    setWindowTitle(tr("Photo Preview"));
    setMinimumSize(640, 480);
    
    setupUi();
    createConnections();
}

void PhotoPreviewDialog::setupUi()
{
    auto mainLayout = new QVBoxLayout(this);
    
    // Image scroll area
    auto scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    imageLabel_ = new QLabel();
    imageLabel_->setPixmap(QPixmap::fromImage(image_));
    imageLabel_->setAlignment(Qt::AlignCenter);
    
    scrollArea->setWidget(imageLabel_);
    mainLayout->addWidget(scrollArea, 1);
    
    // Path information
    pathLabel_ = new QLabel(path_);
    pathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel_->setWordWrap(true);
    mainLayout->addWidget(pathLabel_);
    
    // Buttons
    auto buttonLayout = new QHBoxLayout();
    
    saveAsButton_ = new QPushButton(tr("Save As..."));
    copyButton_ = new QPushButton(tr("Copy to Clipboard"));
    closeButton_ = new QPushButton(tr("Close"));
    closeButton_->setDefault(true);
    
    buttonLayout->addWidget(saveAsButton_);
    buttonLayout->addWidget(copyButton_);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton_);
    
    mainLayout->addLayout(buttonLayout);
}

void PhotoPreviewDialog::createConnections()
{
    connect(saveAsButton_, &QPushButton::clicked, this, &PhotoPreviewDialog::onSaveAsClicked);
    connect(copyButton_, &QPushButton::clicked, this, &PhotoPreviewDialog::onCopyToClipboardClicked);
    connect(closeButton_, &QPushButton::clicked, this, &QDialog::accept);
}

void PhotoPreviewDialog::onSaveAsClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Photo As"), path_, tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    
    if (!filePath.isEmpty()) {
        if (image_.save(filePath)) {
            QMessageBox::information(this, tr("Save Successful"), 
                tr("Photo saved successfully to:\n%1").arg(filePath));
        } else {
            QMessageBox::warning(this, tr("Save Failed"), 
                tr("Failed to save photo to:\n%1").arg(filePath));
        }
    }
}

void PhotoPreviewDialog::onCopyToClipboardClicked()
{
    QGuiApplication::clipboard()->setImage(image_);
    QMessageBox::information(this, tr("Copy Successful"), 
        tr("Photo copied to clipboard"));
}

} // namespace cam_matrix::ui 