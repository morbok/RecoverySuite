#include "RecoveryResultsWidget.h"
#include "Application/Service/Models/GUIModels.h"
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <chrono>
#include <QTreeWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
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
    candidates_ = new QTreeWidget(this); candidates_->setObjectName("scanCandidates");
    candidates_->setHeaderLabels({tr("File"),tr("Bytes"),tr("Recovery confidence")});
    candidates_->setRootIsDecorated(false); candidates_->header()->setStretchLastSection(true);
    candidates_->header()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    layout->addWidget(candidates_);
    reviewControls_ = new QWidget(this); auto* controls = new QVBoxLayout(reviewControls_);
    filter_ = new QLineEdit(this); filter_->setObjectName("candidateFilter"); filter_->setPlaceholderText(tr("Filter by filename, folder or file extension")); controls->addWidget(filter_);
    auto* actions = new QHBoxLayout;
    auto* all = new QPushButton(tr("Select visible files"),this); all->setObjectName("selectVisibleCandidates");
    auto* none = new QPushButton(tr("Clear selection"),this); actions->addWidget(all); actions->addWidget(none); actions->addStretch(); controls->addLayout(actions);
    auto* destination = new QHBoxLayout;
    output_ = new QLineEdit(this); output_->setObjectName("reviewOutput"); output_->setPlaceholderText(tr("Choose a folder on a different drive"));
    auto* browse = new QPushButton(tr("Choose destination…"),this); destination->addWidget(output_); destination->addWidget(browse); controls->addLayout(destination);
    recover_ = new QPushButton(tr("Recover selected files"),this); recover_->setObjectName("recoverSelected"); controls->addWidget(recover_); layout->addWidget(reviewControls_);
    connect(filter_,&QLineEdit::textChanged,this,[this](const QString& text) {
        for(int i=0;i<candidates_->topLevelItemCount();++i) { auto* row=candidates_->topLevelItem(i); row->setHidden(!row->text(0).contains(text,Qt::CaseInsensitive)); }
    });
    connect(all,&QPushButton::clicked,this,[this] { for(int i=0;i<candidates_->topLevelItemCount();++i) if(!candidates_->topLevelItem(i)->isHidden()) candidates_->topLevelItem(i)->setCheckState(0,Qt::Checked); });
    connect(none,&QPushButton::clicked,this,[this] { for(int i=0;i<candidates_->topLevelItemCount();++i) candidates_->topLevelItem(i)->setCheckState(0,Qt::Unchecked); });
    connect(browse,&QPushButton::clicked,this,[this] { const auto folder=QFileDialog::getExistingDirectory(this,tr("Recover to a different drive")); if(!folder.isEmpty())output_->setText(folder); });
    connect(recover_,&QPushButton::clicked,this,[this] {
        QStringList names; for(int i=0;i<candidates_->topLevelItemCount();++i) { const auto* row=candidates_->topLevelItem(i); if(row->checkState(0)==Qt::Checked)names.push_back(row->text(0)); }
        if(names.empty()){statusLabel_->setText(tr("Choose files to recover first"));return;}
        if(output_->text().isEmpty()){statusLabel_->setText(tr("Choose a destination on a different drive"));return;}
        emit recoverSelection(names,output_->text());
    });
    clear();
}
void RecoveryResultsWidget::clear() {
    for (auto* label : {statusLabel_, operationLabel_, processedBytesLabel_, recoveredBytesLabel_,
                       failedBytesLabel_, recoveredItemsLabel_, failedItemsLabel_, timeTakenLabel_,
                       destinationLabel_, exportLabel_}) label->setText(tr("Not reported"));
    statusLabel_->setText(tr("No operation completed"));
    detailsTextEdit_->clear();
    recoveredItemsListWidget_->clear(); candidates_->clear(); filter_->clear(); candidates_->hide(); reviewControls_->hide(); recoveredItemsListWidget_->show();
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
    if(result.scanOnly) {
        statusLabel_->setText(result.cancelled ? tr("Scan cancelled") : result.success ? tr("Scan complete — review candidates") : tr("Partial scan — review candidates and issues"));
        recoveredBytesLabel_->setText(tr("%1 candidate bytes; nothing saved").arg(result.recoveredBytes));
        exportLabel_->setText(tr("No files saved — select files below to recover"));
        recoveredItemsListWidget_->hide(); candidates_->show(); reviewControls_->show();
        for(const auto& item:result.recoveredItems) {
            auto* row=new QTreeWidgetItem(candidates_); row->setText(0,QString::fromStdString(item.name));
            row->setText(1,QString::number(item.sizeBytes)); row->setText(2,QString::fromStdString(item.confidence));
            row->setFlags(row->flags()|Qt::ItemIsUserCheckable); row->setCheckState(0,Qt::Unchecked);
        }
        recover_->setEnabled(!result.recoveredItems.empty() && !result.cancelled);
    }

}
}
