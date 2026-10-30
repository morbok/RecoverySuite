#include "SourceDiscoveryWidget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
namespace recoverysuite::gui::widgets {
SourceDiscoveryWidget::SourceDiscoveryWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(tr("Open a disk image"), this);
    layout->addWidget(heading);
    auto* help = new QLabel(tr("Choose a raw image file. The source is opened read-only. Physical devices and compressed images are not supported in this workflow."), this);
    help->setWordWrap(true); layout->addWidget(help);
    auto* form = new QFormLayout;
    path_ = new QLineEdit(this); path_->setObjectName("imagePath");
    form->addRow(tr("Image file"), path_);
    sectorSize_ = new QComboBox(this); sectorSize_->setObjectName("imageSectorSize");
    for (int size : {512,1024,2048,4096}) sectorSize_->addItem(QString::number(size), size);
    form->addRow(tr("Logical sector size (bytes)"), sectorSize_);
    layout->addLayout(form);
    auto* actions = new QHBoxLayout;
    auto* browse = new QPushButton(tr("Browse…"), this);
    auto* open = new QPushButton(tr("Open image"), this); open->setObjectName("openImage");
    actions->addWidget(browse); actions->addWidget(open); layout->addLayout(actions);
    status_ = new QLabel(tr("No image opened"), this); status_->setObjectName("sourceStatus");
    status_->setWordWrap(true); status_->setTextFormat(Qt::PlainText); layout->addWidget(status_); layout->addStretch();
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, tr("Choose disk image"), {}, tr("Disk images (*.img *.raw *.dd);;All files (*)"));
        if (!file.isEmpty()) path_->setText(file);
    });
    connect(open, &QPushButton::clicked, this, [this] { openImagePath(path_->text(), sectorSize_->currentData().toUInt()); });
}
void SourceDiscoveryWidget::setRecoveryService(application::service::GUIRecoveryService* service) { service_ = service; }
void SourceDiscoveryWidget::onRefreshClicked() { status_->setText(tr("Choose an image file to open it read-only.")); }
void SourceDiscoveryWidget::openImagePath(const QString& path, uint32_t sectorSize) {
    if (!service_) { status_->setText(tr("Recovery service is unavailable")); return; }
    try {
        const auto source = service_->openImage(path.toStdString(), sectorSize);
        path_->setText(QString::fromStdString(source.devicePath));
        status_->setText(tr("Opened read-only: %1 bytes, %2 bytes per sector").arg(source.totalBytes).arg(source.bytesPerSector));
        emit sourceSelected(QString::fromStdString(source.devicePath));
    } catch (const std::exception& failure) { status_->setText(QString::fromUtf8(failure.what())); }
}
}
