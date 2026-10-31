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
namespace recoverysuite::gui::widgets {
RecoveryConfigurationWidget::RecoveryConfigurationWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    sourceLabel_ = new QLabel(tr("No image selected"), this); sourceLabel_->setWordWrap(true);
    sourceLabel_->setTextFormat(Qt::PlainText); layout->addWidget(sourceLabel_);
    auto* help = new QLabel(tr("Signature carving: JPEG, PDF and ZIP candidates up to 16 MiB each. Candidates may still contain damaged data. Original filenames and fragmented files are not reconstructed."), this);
    help->setWordWrap(true); layout->addWidget(help);
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
    auto* metadataHelp = new QLabel(tr("FAT snapshot saves the boot sector and FAT copies, plus the fixed root directory on FAT12/16. Root-file mode follows allocated FAT12/16/32 chains using verified long names or 8.3 aliases; deleted entries and subdirectories are skipped. Choose files with subfolders for recursive recovery (up to 32 levels and 1023 subdirectories). Empty folders are not exported. Each region/file is limited to 16 MiB. FAT snapshot mode does not repair metadata or reconstruct FAT32 root chains. NTFS MFT snapshot reads up to 16 MiB of initialized records, without file reconstruction, mirror fallback or attribute-list extensions. NTFS root mode copies resident/nonresident files up to 16 MiB. NTFS subfolder mode resolves sequence-checked parent paths, up to 32 levels. Use deleted candidate modes for retained deleted metadata. Candidate bytes may be overwritten; FAT assumes contiguous free clusters and cannot restore lost filename characters."),this);
    metadataHelp->setWordWrap(true); layout->addWidget(metadataHelp);
    start_ = new QLineEdit("0", this); start_->setObjectName("recoveryStart");
    count_ = new QLineEdit(this); count_->setObjectName("recoveryCount");
    output_ = new QLineEdit(this); output_->setObjectName("recoveryOutput");
    form->addRow(tr("Start sector"), start_); form->addRow(tr("Number of sectors"), count_);
    form->addRow(tr("Existing output folder"), output_);
    session_=new QLineEdit(this);session_->setObjectName("recoverySession");session_->setPlaceholderText(tr("Optional new session file; use File → Resume session for an existing one"));form->addRow(tr("Session file"),session_); layout->addLayout(form);
    auto* browse = new QPushButton(tr("Choose output folder…"), this); layout->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto directory = QFileDialog::getExistingDirectory(this, tr("Output folder"));
        if (!directory.isEmpty()) output_->setText(directory);
    });
    verify_ = new QCheckBox(tr("Read saved files back and compare their bytes"), this);
    verify_->setObjectName("verifyOutput"); verify_->setChecked(true); layout->addWidget(verify_);
    auto* start = new QPushButton(tr("Validate and start recovery"), this); start->setObjectName("startImageRecovery");
    layout->addWidget(start); connect(start, &QPushButton::clicked, this, &RecoveryConfigurationWidget::validateConfiguration);
    status_ = new QLabel(this); status_->setObjectName("configurationStatus"); status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true); layout->addWidget(status_); layout->addStretch();
}
void RecoveryConfigurationWidget::setRecoveryService(application::service::GUIRecoveryService* service) { service_ = service; }
void RecoveryConfigurationWidget::setSelectedDevice(const QString& path) {
    resumeSession_=false;session_->clear();
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
    return operation;
}
void RecoveryConfigurationWidget::setOperation(const application::service::models::RecoveryOperation& operation) {
    source_=QString::fromStdString(operation.targetDevicePath);sourceLabel_->setText(source_);method_->setCurrentIndex(method_->findData(QString::fromStdString(operation.operationType)));start_->setText(QString::number(operation.startSector));count_->setText(QString::number(operation.numSectors));output_->setText(QString::fromStdString(operation.outputPath));verify_->setChecked(operation.verifyAfterRecovery);session_->setText(QString::fromStdString(operation.sessionPath));resumeSession_=operation.resumeSession;status_->setText(tr("Review the saved configuration. Resume rechecks the source and completed outputs, then replays discovery."));
}
void RecoveryConfigurationWidget::validateConfiguration() {
    if (!service_) { status_->setText(tr("Recovery service is unavailable")); return; }
    try {
        const auto operation = getRecoveryOperation();
        const auto error = service_->validateOperation(operation);
        if (!error.empty()) { status_->setText(QString::fromStdString(error)); return; }
        status_->setText(tr("Source, range and output folder validated. Existing files will not be replaced."));
        emit configurationReady(operation);
    } catch (const std::exception& failure) { status_->setText(QString::fromUtf8(failure.what())); }
}
}
