#include "RecoveryConfigurationWidget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <charconv>
#include <QGroupBox>
namespace recoverysuite::gui::widgets {
RecoveryConfigurationWidget::RecoveryConfigurationWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    sourceLabel_ = new QLabel(tr("No drive selected"), this); sourceLabel_->setWordWrap(true);
    sourceLabel_->setTextFormat(Qt::PlainText); layout->addWidget(sourceLabel_);
    auto* help = new QLabel(tr("Start with an automatic scan. It searches the whole source for surviving FAT/NTFS volumes and deleted-file records, then JPEG/PDF/ZIP signatures. Review the found files before choosing what to recover. Damaged writes, overwritten bytes and SSD TRIM can make some data unrecoverable. Other filesystems and encrypted data are not supported yet."), this);
    help->setWordWrap(true); layout->addWidget(help);
    mode_ = new QComboBox(this); mode_->setObjectName("scanMode");
    mode_->addItem(tr("Automatic scan — find lost data")); mode_->addItem(tr("Advanced — choose a specific method")); layout->addWidget(mode_);
    advanced_ = new QGroupBox(tr("Advanced recovery settings"), this); auto* advancedLayout = new QVBoxLayout(advanced_);
    auto* form = new QFormLayout;
    method_ = new QComboBox(this); method_->setObjectName("recoveryMethod");
    method_->addItem(tr("Signature carving"),"carving");
    method_->addItem(tr("FAT metadata snapshot"),"metadata_recovery");
    method_->addItem(tr("FAT12/16/32 allocated root files (long names / 8.3)"),"file_recovery");
    method_->addItem(tr("FAT12/16/32 files with subfolders"),"file_tree_recovery");
    method_->addItem(tr("NTFS boot and initialized MFT snapshot"),"ntfs_mft_snapshot");
    method_->addItem(tr("NTFS root files (resident / nonresident)"),"ntfs_resident_recovery");
    method_->addItem(tr("NTFS files with subfolders"),"ntfs_tree_recovery");
    method_->addItem(tr("NTFS files with allocation and directory checks"),"ntfs_checked_recovery");
    method_->addItem(tr("NTFS deleted file candidates"),"ntfs_deleted_recovery");
    method_->addItem(tr("FAT deleted file candidates (contiguous assumption)"),"fat_deleted_recovery");
    form->addRow(tr("Recovery method"),method_);
    start_ = new QLineEdit("0", this); start_->setObjectName("recoveryStart");
    count_ = new QLineEdit(this); count_->setObjectName("recoveryCount");
    output_ = new QLineEdit(this); output_->setObjectName("recoveryOutput");
    form->addRow(tr("Start sector"), start_); form->addRow(tr("Number of sectors"), count_);
    form->addRow(tr("Existing output folder"), output_);
    session_=new QLineEdit(this);session_->setObjectName("recoverySession");session_->setPlaceholderText(tr("Optional new session file; use File → Resume session for an existing one"));form->addRow(tr("Session file"),session_); advancedLayout->addLayout(form);
    auto* browse = new QPushButton(tr("Choose output folder…"), this); advancedLayout->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto directory = QFileDialog::getExistingDirectory(this, tr("Output folder"));
        if (!directory.isEmpty()) output_->setText(directory);
    });
    verify_ = new QCheckBox(tr("Read saved files back and compare their bytes"), this);
    verify_->setObjectName("verifyOutput"); verify_->setChecked(true); advancedLayout->addWidget(verify_);
    layout->addWidget(advanced_); advanced_->hide();
    auto* start = new QPushButton(tr("Search for lost data"), this); start->setObjectName("startImageRecovery");
    layout->addWidget(start); connect(start, &QPushButton::clicked, this, &RecoveryConfigurationWidget::validateConfiguration);
    connect(mode_, &QComboBox::currentIndexChanged, this, [this,start](int index) {
        advanced_->setVisible(index == 1); start->setText(index == 0 ? tr("Search for lost data") : tr("Validate and recover"));
    });
    connect(method_, &QComboBox::currentIndexChanged, this, [this] { mode_->setCurrentIndex(1); });
    status_ = new QLabel(this); status_->setObjectName("configurationStatus"); status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true); layout->addWidget(status_); layout->addStretch();
}
void RecoveryConfigurationWidget::setRecoveryService(application::service::GUIRecoveryService* service) { service_ = service; }
void RecoveryConfigurationWidget::setSelectedDevice(const QString& path) {
    resumeSession_=false;session_->clear();mode_->setCurrentIndex(0);
    source_ = path; sourceLabel_->setText(path); start_->setText("0"); status_->clear();
    if (service_) count_->setText(QString::number(service_->getDiskInfo(path.toStdString()).totalSectors));
}
application::service::models::RecoveryOperation RecoveryConfigurationWidget::getRecoveryOperation() const {
    application::service::models::RecoveryOperation operation{};
    const auto parse = [](const QString& text) {
        const auto string = text.toStdString(); uint64_t value{};
        const auto [end, error] = std::from_chars(string.data(), string.data() + string.size(), value);
        if (error != std::errc{} || end != string.data() + string.size()) throw std::invalid_argument("Enter whole, nonnegative sector numbers");
        return value;
    };
    operation.operationType = method_->currentData().toString().toStdString(); operation.targetDevicePath = source_.toStdString();
    operation.startSector = parse(start_->text()); operation.numSectors = parse(count_->text());
    operation.sessionPath=session_->text().toStdString();operation.resumeSession=resumeSession_;
    operation.outputPath = output_->text().toStdString(); operation.verifyAfterRecovery = verify_->isChecked();
    if (mode_->currentIndex() == 0) { operation.operationType="disk_scan"; operation.outputPath.clear(); operation.sessionPath.clear(); operation.resumeSession=false; operation.startSector=0; if(service_) operation.numSectors=service_->getDiskInfo(operation.targetDevicePath).totalSectors; }
    return operation;
}
void RecoveryConfigurationWidget::setOperation(const application::service::models::RecoveryOperation& operation) {
    mode_->setCurrentIndex(1);source_=QString::fromStdString(operation.targetDevicePath);sourceLabel_->setText(source_);method_->setCurrentIndex(method_->findData(QString::fromStdString(operation.operationType)));start_->setText(QString::number(operation.startSector));count_->setText(QString::number(operation.numSectors));output_->setText(QString::fromStdString(operation.outputPath));verify_->setChecked(operation.verifyAfterRecovery);session_->setText(QString::fromStdString(operation.sessionPath));resumeSession_=operation.resumeSession;status_->setText(tr("Review the saved configuration. Resume rechecks the source and completed outputs, then replays discovery."));
}
void RecoveryConfigurationWidget::validateConfiguration() {
    if (!service_) { status_->setText(tr("Recovery service is unavailable")); return; }
    try {
        const auto operation = getRecoveryOperation();
        const auto error = service_->validateOperation(operation);
        if (!error.empty()) { status_->setText(QString::fromStdString(error)); return; }
        status_->setText(operation.operationType=="disk_scan" ? tr("Scanning finds candidates for review; it does not save any files.") : tr("Source and destination validated. Existing files will not be replaced."));
        emit configurationReady(operation);
    } catch (const std::exception& failure) { status_->setText(QString::fromUtf8(failure.what())); }
}
}
