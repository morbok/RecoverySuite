#include "RecoveryResultsWidget.h"
#include "Application/Service/Models/GUIModels.h"
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <chrono>
namespace recoverysuite::gui::widgets {
RecoveryResultsWidget::RecoveryResultsWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Recovery results"), this));
    auto* form = new QFormLayout;
    layout->addLayout(form);
    auto addField = [this, form](const QString& text, const char* name) {
        auto* label = new QLabel(this);
        label->setObjectName(name);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(text, label);
        return label;
    };
    statusLabel_ = addField(tr("Status"), "resultStatus");
    operationLabel_ = addField(tr("Operation"), "resultOperation");
    processedBytesLabel_ = addField(tr("Processed bytes"), "resultProcessedBytes");
    recoveredBytesLabel_ = addField(tr("Recovered bytes"), "resultRecoveredBytes");
    failedBytesLabel_ = addField(tr("Failed bytes"), "resultFailedBytes");
    recoveredItemsLabel_ = addField(tr("Recovered items"), "resultRecoveredItems");
    failedItemsLabel_ = addField(tr("Failed items"), "resultFailedItems");
    timeTakenLabel_ = addField(tr("Elapsed time"), "resultElapsed");
    destinationLabel_ = addField(tr("Requested destination"), "resultDestination");
    exportLabel_ = addField(tr("Output export"), "resultExport");
    detailsTextEdit_ = new QPlainTextEdit(this);
    detailsTextEdit_->setObjectName("resultDetails");
    detailsTextEdit_->setReadOnly(true);
    layout->addWidget(detailsTextEdit_);
    recoveredItemsListWidget_ = new QListWidget(this);
    recoveredItemsListWidget_->setObjectName("resultItems");
    layout->addWidget(recoveredItemsListWidget_);
    clear();
}
void RecoveryResultsWidget::clear() {
    for (auto* label : {statusLabel_, operationLabel_, processedBytesLabel_, recoveredBytesLabel_,
                       failedBytesLabel_, recoveredItemsLabel_, failedItemsLabel_, timeTakenLabel_,
                       destinationLabel_, exportLabel_}) label->setText(tr("Not reported"));
    statusLabel_->setText(tr("No operation completed"));
    detailsTextEdit_->clear();
    recoveredItemsListWidget_->clear();
}
void RecoveryResultsWidget::setRecoveryResult(const application::service::models::RecoveryResult& result) {
    clear();
    const bool hasData = result.recoveredItemsCount > 0 || result.recoveredBytes > 0;
    const bool hasFailures = result.failedItemsCount > 0 || result.failedBytes > 0 || !result.success;
    statusLabel_->setText(result.cancelled ? tr("Cancelled") :
        (hasData && hasFailures ? tr("Partial recovery") : (result.success ? tr("Completed") : tr("Failed"))));
    operationLabel_->setText(QString::fromStdString(result.operationType));
    if (result.processedBytesKnown) processedBytesLabel_->setText(QString::number(result.processedBytes));
    recoveredBytesLabel_->setText(QString::number(result.recoveredBytes));
    if (result.failedBytesKnown) failedBytesLabel_->setText(QString::number(result.failedBytes));
    recoveredItemsLabel_->setText(QString::number(result.recoveredItemsCount));
    if (result.failedItemsKnown || result.failedItemsCount > 0) failedItemsLabel_->setText(QString::number(result.failedItemsCount));
    if (result.startTime != std::chrono::system_clock::time_point{} && result.endTime >= result.startTime) {
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(result.endTime - result.startTime).count();
        timeTakenLabel_->setText(tr("%1 s").arg(seconds));
    }
    if (!result.destination.empty()) destinationLabel_->setText(QString::fromStdString(result.destination));
    exportLabel_->setText(result.outputWritten ? tr("Written to destination") : tr("Not performed"));
    QString details = QString::fromStdString(result.errorMessage);
    for (const auto& [key, value] : result.details) {
        if (!details.isEmpty()) details += '\n';
        details += QString::fromStdString(key) + ": " + QString::fromStdString(value);
    }
    detailsTextEdit_->setPlainText(details);
    for (const auto& item : result.recoveredItems)
        recoveredItemsListWidget_->addItem(tr("%1 (%2 bytes)").arg(QString::fromStdString(item.name)).arg(item.sizeBytes));
    if (result.recoveredItems.empty())
        recoveredItemsListWidget_->addItem(hasData ? tr("Item details were not provided by the backend.") : tr("No recovered items."));
}
}
