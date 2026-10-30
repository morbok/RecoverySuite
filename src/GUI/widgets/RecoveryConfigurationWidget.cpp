#include "RecoveryConfigurationWidget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
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
    start_ = new QLineEdit("0", this); start_->setObjectName("recoveryStart");
    count_ = new QLineEdit(this); count_->setObjectName("recoveryCount");
    output_ = new QLineEdit(this); output_->setObjectName("recoveryOutput");
    form->addRow(tr("Start sector"), start_); form->addRow(tr("Number of sectors"), count_);
    form->addRow(tr("Existing output folder"), output_); layout->addLayout(form);
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
    operation.operationType = "carving"; operation.targetDevicePath = source_.toStdString();
    operation.startSector = parse(start_->text()); operation.numSectors = parse(count_->text());
    operation.outputPath = output_->text().toStdString(); operation.verifyAfterRecovery = verify_->isChecked();
    return operation;
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
