#include "RecoveryResultsWidget.h"
#include "Application/Service/Models/GUIModels.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <chrono>
namespace recoverysuite::gui::widgets {
RecoveryResultsWidget::RecoveryResultsWidget(QWidget *parent)
    : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  heading_ = new QLabel(tr("Recovery results"), this);
  heading_->setStyleSheet("font-size: 24px; font-weight: 600;");
  layout->addWidget(heading_);
  scanBanner_ = new QLabel(this);
  scanBanner_->setTextFormat(Qt::PlainText);
  scanBanner_->setWordWrap(true);
  scanBanner_->setStyleSheet("font-size: 16px; padding: 6px 0;");
  layout->addWidget(scanBanner_);
  summaryPanel_ = new QWidget(this);
  auto *form = new QFormLayout(summaryPanel_);
  layout->addWidget(summaryPanel_);
  auto addField = [this, form](const QString &text, const char *name) {
    auto *label = new QLabel(this);
    label->setObjectName(name);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(text, label);
    return label;
  };
  statusLabel_ = addField(tr("Status"), "resultStatus");
  operationLabel_ = addField(tr("Operation"), "resultOperation");
  processedBytesLabel_ =
      addField(tr("Processed bytes"), "resultProcessedBytes");
  recoveredBytesLabel_ =
      addField(tr("Recovered bytes"), "resultRecoveredBytes");
  failedBytesLabel_ = addField(tr("Failed bytes"), "resultFailedBytes");
  recoveredItemsLabel_ =
      addField(tr("Recovered items"), "resultRecoveredItems");
  failedItemsLabel_ = addField(tr("Failed items"), "resultFailedItems");
  timeTakenLabel_ = addField(tr("Elapsed time"), "resultElapsed");
  destinationLabel_ =
      addField(tr("Requested destination"), "resultDestination");
  exportLabel_ = addField(tr("Output export"), "resultExport");
  detailsTextEdit_ = new QPlainTextEdit(this);
  detailsTextEdit_->setObjectName("resultDetails");
  detailsTextEdit_->setReadOnly(true);
  detailsToggle_ = new QPushButton(tr("Scan details and limitations"), this);
  detailsToggle_->setCheckable(true);
  layout->addWidget(detailsToggle_);
  connect(detailsToggle_, &QPushButton::toggled, detailsTextEdit_,
          &QWidget::setVisible);
  layout->addWidget(detailsTextEdit_);
  recoveredItemsListWidget_ = new QListWidget(this);
  recoveredItemsListWidget_->setObjectName("resultItems");
  layout->addWidget(recoveredItemsListWidget_);
  candidates_ = new QTreeWidget(this);
  candidates_->setObjectName("scanCandidates");
  candidates_->setHeaderLabels(
      {tr("Filename"), tr("Location"), tr("Bytes"), tr("Confidence")});
  candidates_->setRootIsDecorated(false);
  candidates_->header()->setStretchLastSection(true);
  candidates_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  candidates_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  layout->addWidget(candidates_);
  previewButton_ = new QPushButton(tr("Inspect highlighted file…"), this);
  previewButton_->setObjectName("previewCandidate");
  layout->addWidget(previewButton_);
  previewStatus_ = new QLabel(this);
  previewStatus_->setObjectName("previewStatus");
  previewStatus_->setTextFormat(Qt::PlainText);
  previewStatus_->setWordWrap(true);
  layout->addWidget(previewStatus_);
  previewText_ = new QPlainTextEdit(this);
  previewText_->setObjectName("candidatePreview");
  previewText_->setReadOnly(true);
  previewText_->setMaximumHeight(180);
  layout->addWidget(previewText_);
  connect(previewButton_, &QPushButton::clicked, this, [this] {
    if (const auto *row = candidates_->currentItem()) {
      previewStatus_->setText(tr("Reading a preview from the source…"));
      previewStatus_->show();
      previewText_->clear();
      emit previewRequested(row->data(0, Qt::UserRole).toString());
    } else {
      previewStatus_->setText(tr("Highlight a file to inspect its contents"));
      previewStatus_->show();
    }
  });
  connect(candidates_, &QTreeWidget::currentItemChanged, this, [this] {
    previewText_->clear();
    previewText_->hide();
    previewStatus_->hide();
  });
  reviewControls_ = new QWidget(this);
  auto *controls = new QVBoxLayout(reviewControls_);
  filter_ = new QLineEdit(this);
  filter_->setObjectName("candidateFilter");
  filter_->setPlaceholderText(
      tr("Filter by filename, folder or file extension"));
  layout->insertWidget(layout->indexOf(candidates_), filter_);
  auto *actions = new QHBoxLayout;
  auto *all = new QPushButton(tr("Select visible files"), this);
  all->setObjectName("selectVisibleCandidates");
  auto *none = new QPushButton(tr("Clear selection"), this);
  actions->addWidget(all);
  actions->addWidget(none);
  actions->addStretch();
  controls->addLayout(actions);
  auto *destination = new QHBoxLayout;
  output_ = new QLineEdit(this);
  output_->setObjectName("reviewOutput");
  output_->setPlaceholderText(tr("Choose a folder on a different drive"));
  auto *browse = new QPushButton(tr("Choose destination…"), this);
  destination->addWidget(output_);
  destination->addWidget(browse);
  controls->addLayout(destination);
  recover_ = new QPushButton(tr("Recover selected files"), this);
  recover_->setObjectName("recoverSelected");
  controls->addWidget(recover_);
  layout->addWidget(reviewControls_);
  connect(filter_, &QLineEdit::textChanged, this, [this](const QString &text) {
    for (int i = 0; i < candidates_->topLevelItemCount(); ++i) {
      auto *row = candidates_->topLevelItem(i);
      row->setHidden(!row->data(0, Qt::UserRole)
                          .toString()
                          .contains(text, Qt::CaseInsensitive));
    }
  });
  connect(all, &QPushButton::clicked, this, [this] {
    for (int i = 0; i < candidates_->topLevelItemCount(); ++i)
      if (!candidates_->topLevelItem(i)->isHidden())
        candidates_->topLevelItem(i)->setCheckState(0, Qt::Checked);
  });
  connect(none, &QPushButton::clicked, this, [this] {
    for (int i = 0; i < candidates_->topLevelItemCount(); ++i)
      candidates_->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
  });
  connect(browse, &QPushButton::clicked, this, [this] {
    const auto folder = QFileDialog::getExistingDirectory(
        this, tr("Recover to a different drive"));
    if (!folder.isEmpty())
      output_->setText(folder);
  });
  connect(recover_, &QPushButton::clicked, this, [this] {
    QStringList names;
    for (int i = 0; i < candidates_->topLevelItemCount(); ++i) {
      const auto *row = candidates_->topLevelItem(i);
      if (row->checkState(0) == Qt::Checked)
        names.push_back(row->data(0, Qt::UserRole).toString());
    }
    if (names.empty()) {
      statusLabel_->setText(tr("Choose files to recover first"));
      return;
    }
    if (output_->text().isEmpty()) {
      statusLabel_->setText(tr("Choose a destination on a different drive"));
      return;
    }
    emit recoverSelection(names, output_->text());
  });
  openDestination_ = new QPushButton(tr("Open recovery folder"), this);
  layout->addWidget(openDestination_);
  connect(openDestination_, &QPushButton::clicked, this, [this] {
    if (!destination_.isEmpty())
      QDesktopServices::openUrl(QUrl::fromLocalFile(destination_));
  });
  clear();
}
void RecoveryResultsWidget::clear() {
  for (auto *label :
       {statusLabel_, operationLabel_, processedBytesLabel_,
        recoveredBytesLabel_, failedBytesLabel_, recoveredItemsLabel_,
        failedItemsLabel_, timeTakenLabel_, destinationLabel_, exportLabel_})
    label->setText(tr("Not reported"));
  previewButton_->hide();
  previewStatus_->hide();
  previewText_->hide();
  previewText_->clear();
  heading_->setText(tr("Recovery results"));
  summaryPanel_->show();
  scanBanner_->hide();
  scanBanner_->clear();
  detailsToggle_->hide();
  detailsToggle_->setChecked(false);
  detailsTextEdit_->show();
  filter_->hide();
  openDestination_->hide();
  destination_.clear();
  statusLabel_->setText(tr("No operation completed"));
  detailsTextEdit_->clear();
  recoveredItemsListWidget_->clear();
  candidates_->clear();
  filter_->clear();
  candidates_->hide();
  reviewControls_->hide();
  recoveredItemsListWidget_->show();
}
void RecoveryResultsWidget::setRecoveryResult(
    const application::service::models::RecoveryResult &result) {
  clear();
  const bool hasData =
      result.recoveredItemsCount > 0 || result.recoveredBytes > 0;
  const bool hasFailures =
      result.failedItemsCount > 0 || result.failedBytes > 0 || !result.success;
  statusLabel_->setText(
      result.cancelled
          ? tr("Cancelled")
          : (hasData && hasFailures
                 ? tr("Partial recovery")
                 : (result.success ? tr("Completed") : tr("Failed"))));
  operationLabel_->setText(QString::fromStdString(result.operationType));
  if (result.processedBytesKnown)
    processedBytesLabel_->setText(QString::number(result.processedBytes));
  recoveredBytesLabel_->setText(QString::number(result.recoveredBytes));
  if (result.failedBytesKnown)
    failedBytesLabel_->setText(QString::number(result.failedBytes));
  recoveredItemsLabel_->setText(QString::number(result.recoveredItemsCount));
  if (result.failedItemsKnown || result.failedItemsCount > 0)
    failedItemsLabel_->setText(QString::number(result.failedItemsCount));
  if (result.startTime != std::chrono::system_clock::time_point{} &&
      result.endTime >= result.startTime) {
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                             result.endTime - result.startTime)
                             .count();
    timeTakenLabel_->setText(tr("%1 s").arg(seconds));
  }
  if (!result.destination.empty())
    destinationLabel_->setText(QString::fromStdString(result.destination));
  exportLabel_->setText(result.outputWritten ? tr("Written to destination")
                                             : tr("Not performed"));
  QString details = QString::fromStdString(result.errorMessage);
  for (const auto &[key, value] : result.details) {
    if (!details.isEmpty())
      details += '\n';
    details +=
        QString::fromStdString(key) + ": " + QString::fromStdString(value);
  }
  detailsTextEdit_->setPlainText(details);
  for (const auto &item : result.recoveredItems)
    recoveredItemsListWidget_->addItem(
        tr("%1 (%2 bytes)")
            .arg(QString::fromStdString(item.name))
            .arg(item.sizeBytes));
  if (result.recoveredItems.empty())
    recoveredItemsListWidget_->addItem(
        hasData ? tr("Item details were not provided by the backend.")
                : tr("No recovered items."));
  if (result.operationType == "disk_rescue" && result.outputWritten) {
    summaryPanel_->hide();
    detailsTextEdit_->hide();
    detailsToggle_->show();
    scanBanner_->show();
    heading_->setText(tr("Rescue image saved"));
    scanBanner_->setText(
        tr("Image created. Unreadable bytes replaced with zeros: %1. Review "
           "the rescue report, then open the image to scan for files.")
            .arg(result.failedBytes));
    if (!result.errorMessage.empty())
      scanBanner_->setText(scanBanner_->text() + "\n" +
                           QString::fromStdString(result.errorMessage));
    recoveredItemsListWidget_->clear();
    for (const auto &key : {"Rescue image", "Rescue report"})
      if (result.details.contains(key))
        recoveredItemsListWidget_->addItem(
            QString::fromStdString(result.details.at(key)));
    destination_ = QString::fromStdString(result.destination);
    openDestination_->show();
  }
  if (result.scanOnly || result.operationType == "disk_recovery") {
    summaryPanel_->hide();
    detailsTextEdit_->hide();
    detailsToggle_->show();
    scanBanner_->show();
    heading_->setText(result.scanOnly ? tr("Review found files")
                                      : tr("Recovery finished"));
    scanBanner_->setText(
        result.scanOnly
            ? tr("%1 candidates found. No files have been saved yet.")
                  .arg(result.recoveredItemsCount)
            : tr("Recovered files: %1. Destination: %2")
                  .arg(result.recoveredItemsCount)
                  .arg(QString::fromStdString(result.destination)));
    if (!result.errorMessage.empty())
      scanBanner_->setText(scanBanner_->text() + "\n" +
                           QString::fromStdString(result.errorMessage));
    if (!result.scanOnly && result.outputWritten) {
      destination_ = QString::fromStdString(result.destination);
      openDestination_->show();
    }
  }
  if (result.scanOnly) {
    statusLabel_->setText(
        result.cancelled ? tr("Scan cancelled")
        : result.success ? tr("Scan complete — review candidates")
                         : tr("Partial scan — review candidates and issues"));
    recoveredBytesLabel_->setText(
        tr("%1 candidate bytes; nothing saved").arg(result.recoveredBytes));
    exportLabel_->setText(tr("No files saved — select files below to recover"));
    recoveredItemsListWidget_->hide();
    candidates_->show();
    reviewControls_->show();
    filter_->show();
    QMap<QString, int> volumes;
    for (const auto &item : result.recoveredItems) {
      const auto path = QString::fromStdString(item.name);
      const auto first = path.section('/', 0, 0);
      const bool signature = path.startsWith("Signature matches/");
      if (!signature && !volumes.contains(first))
        volumes.insert(first, volumes.size() + 1);
      auto *row = new QTreeWidgetItem(candidates_);
      row->setData(0, Qt::UserRole, path);
      row->setText(0, path.section('/', -1));
      const auto folder = path.section('/', 1, -2);
      row->setText(
          1, signature
                 ? tr("Deep scan")
                 : tr("Volume %1 / %2").arg(volumes.value(first)).arg(folder));
      row->setText(2, QString::number(item.sizeBytes));
      row->setText(3, signature ? tr("Signature match — unverified")
                      : path.contains("/Deleted/")
                          ? tr("Deleted candidate — uncertain")
                          : tr("Filesystem records"));
      row->setToolTip(3, QString::fromStdString(item.confidence));
      row->setToolTip(0, path);
      row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
      row->setCheckState(0, Qt::Unchecked);
    }
    previewButton_->setVisible(!result.recoveredItems.empty());
    recover_->setEnabled(!result.recoveredItems.empty());
  }
}
} // namespace recoverysuite::gui::widgets

namespace recoverysuite::gui::widgets {
void RecoveryResultsWidget::setPreview(
    const application::service::models::RecoveryResult &result) {
  previewStatus_->show();
  previewText_->clear();
  if (!result.success) {
    previewStatus_->setText(QString::fromStdString(result.errorMessage));
    previewText_->hide();
    return;
  }
  const QByteArray bytes(
      reinterpret_cast<const char *>(result.previewBytes.data()),
      static_cast<qsizetype>(result.previewBytes.size()));
  bool text = true;
  for (const auto byte : bytes) {
    const auto c = static_cast<unsigned char>(byte);
    if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
      text = false;
      break;
    }
  }
  // Plain text only: never execute candidate contents or render active markup.
  previewText_->setPlainText(text ? QString::fromUtf8(bytes)
                                  : QString::fromLatin1(bytes.toHex(' ')));
  previewStatus_->setText(tr("First %1 bytes · %2 · Nothing saved. A readable "
                             "preview does not prove the whole file is intact.")
                              .arg(bytes.size())
                              .arg(text ? tr("text") : tr("hexadecimal")));
  previewText_->show();
}
} // namespace recoverysuite::gui::widgets
