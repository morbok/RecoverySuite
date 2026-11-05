#include "SourceDiscoveryWidget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QGroupBox>
#include <QTreeWidget>
#include <QHeaderView>
namespace recoverysuite::gui::widgets {
SourceDiscoveryWidget::SourceDiscoveryWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(tr("Recover lost data from a drive"), this);
    heading->setStyleSheet("font-size: 24px; font-weight: 600;"); layout->addWidget(heading);
    auto* help = new QLabel(tr("Deleted files, a lost partition, or a drive disconnected during a write? Select the affected drive, scan for recoverable files, then save your selection to another drive."), this);
    help->setWordWrap(true); layout->addWidget(help);
    drives_ = new QTreeWidget(this); drives_->setObjectName("connectedDrives");
    drives_->setHeaderLabels({tr("Drive"),tr("Capacity"),tr("Device path")});
    drives_->setRootIsDecorated(false); drives_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    drives_->header()->setStretchLastSection(true); layout->addWidget(drives_);
    auto* actions = new QHBoxLayout;
    auto* refresh = new QPushButton(tr("Refresh drives"), this); refresh->setObjectName("refreshDrives");
    auto* select = new QPushButton(tr("Use selected drive"), this); select->setObjectName("openPhysicalDrive");
    actions->addWidget(refresh); actions->addStretch(); actions->addWidget(select); layout->addLayout(actions);
    connect(refresh, &QPushButton::clicked, this, &SourceDiscoveryWidget::onRefreshClicked);
    connect(select, &QPushButton::clicked, this, [this] {
        const auto* row = drives_->currentItem();
        if (!row || !service_) { status_->setText(tr("Select a connected drive first.")); return; }
        try {
            const auto source = service_->openDevice(row->data(0, Qt::UserRole).toString().toStdString());
            status_->setText(tr("Drive opened read-only.")); emit sourceSelected(QString::fromStdString(source.devicePath));
        } catch (const std::exception& error) { status_->setText(QString::fromUtf8(error.what())); }
    });
    auto* access = new QLabel(tr("Source access is read-only. Unmount its volumes on Linux or take the source disk offline on Windows first; system drives cannot be scanned while in use. Device permissions may be required. RecoverySuite never changes this state automatically."), this);
    access->setWordWrap(true); layout->addWidget(access);
    auto* imageToggle = new QPushButton(tr("Open an existing disk image instead…"), this); imageToggle->setObjectName("imageSourceOptions"); layout->addWidget(imageToggle);
    auto* image = new QGroupBox(tr("Optional image source"), this); auto* imageLayout = new QVBoxLayout(image);
    auto* form = new QFormLayout;
    path_ = new QLineEdit(this); path_->setObjectName("imagePath"); form->addRow(tr("Image file"), path_);
    sectorSize_ = new QComboBox(this); sectorSize_->setObjectName("imageSectorSize");
    for (int size : {512,1024,2048,4096}) sectorSize_->addItem(QString::number(size), size);
    form->addRow(tr("Sector size"), sectorSize_); imageLayout->addLayout(form);
    auto* imageActions = new QHBoxLayout;
    auto* browse = new QPushButton(tr("Browse…"), this); auto* open = new QPushButton(tr("Open image"), this); open->setObjectName("openImage");
    imageActions->addWidget(browse); imageActions->addWidget(open); imageLayout->addLayout(imageActions);
    layout->addWidget(image); image->hide();
    connect(imageToggle, &QPushButton::clicked, image, [image] { image->setVisible(!image->isVisible()); });
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, tr("Choose disk image"), {}, tr("Disk images (*.img *.raw *.dd);;All files (*)"));
        if (!file.isEmpty()) path_->setText(file);
    });
    connect(open, &QPushButton::clicked, this, [this] { openImagePath(path_->text(), sectorSize_->currentData().toUInt()); });
    status_ = new QLabel(tr("Select the drive that contained your missing files"), this); status_->setObjectName("sourceStatus");
    status_->setWordWrap(true); status_->setTextFormat(Qt::PlainText); layout->addWidget(status_);
}
void SourceDiscoveryWidget::setRecoveryService(application::service::GUIRecoveryService* service) { service_ = service; onRefreshClicked(); }
void SourceDiscoveryWidget::onRefreshClicked() {
    drives_->clear(); if (!service_) return;
    try {
        for (const auto& drive : service_->enumerateStorageDevices()) {
            auto* row = new QTreeWidgetItem(drives_);
            row->setText(0, QString::fromStdString(drive.modelNumber.empty() ? "Unnamed drive" : drive.modelNumber));
            row->setText(1, tr("%1 GB").arg(double(drive.totalBytes)/1000000000.0,0,'f',1));
            row->setText(2, QString::fromStdString(drive.devicePath)); row->setData(0,Qt::UserRole,row->text(2));
        }
        status_->setText(drives_->topLevelItemCount() ? tr("Select a source drive. Recovered files must go to a different drive.") : tr("No drives reported. Refresh after reconnecting the drive, or open an existing image."));
    } catch (const std::exception& error) { status_->setText(QString::fromUtf8(error.what())); }
}
void SourceDiscoveryWidget::openImagePath(const QString& path, uint32_t sectorSize) {
    if (!service_) { status_->setText(tr("Recovery service is unavailable")); return; }
    try {
        const auto source = service_->openImage(path.toStdString(), sectorSize); path_->setText(QString::fromStdString(source.devicePath));
        status_->setText(tr("Opened read-only: %1 bytes").arg(source.totalBytes)); emit sourceSelected(QString::fromStdString(source.devicePath));
    } catch (const std::exception& failure) { status_->setText(QString::fromUtf8(failure.what())); }
}
}
