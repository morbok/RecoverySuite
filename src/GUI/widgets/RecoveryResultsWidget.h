#pragma once
#include <QStringList>
#include <QWidget>
class QLabel;
class QPlainTextEdit;
class QListWidget;
class QTreeWidget;
class QLineEdit;
class QPushButton;
namespace recoverysuite::application::service::models {
struct RecoveryResult;
}
namespace recoverysuite::gui::widgets {
/** Displays backend results without inferring exports or unreported metrics. */
class RecoveryResultsWidget : public QWidget {
  Q_OBJECT
public:
  explicit RecoveryResultsWidget(QWidget *parent = nullptr);
  void
  setRecoveryResult(const application::service::models::RecoveryResult &result);
  void clear();
  void setPreview(const application::service::models::RecoveryResult &result);
signals:
  void continueScanRequested();
  void previewRequested(const QString &name);
  void recoverSelection(const QStringList &names, const QString &destination);

private:
  QPushButton *continueScan_;
  QPushButton *previewButton_;
  QLabel *previewStatus_;
  QLabel *previewImage_;
  QPlainTextEdit *previewText_;
  QWidget *summaryPanel_;
  QLabel *heading_;
  QLabel *scanBanner_;
  QPushButton *detailsToggle_;
  QPushButton *openDestination_;
  QString destination_;
  QTreeWidget *candidates_;
  QLineEdit *filter_;
  QLineEdit *output_;
  QPushButton *recover_;
  QWidget *reviewControls_;
  QLabel *statusLabel_;
  QLabel *operationLabel_;
  QLabel *processedBytesLabel_;
  QLabel *recoveredBytesLabel_;
  QLabel *failedBytesLabel_;
  QLabel *recoveredItemsLabel_;
  QLabel *failedItemsLabel_;
  QLabel *timeTakenLabel_;
  QLabel *destinationLabel_;
  QLabel *exportLabel_;
  QPlainTextEdit *detailsTextEdit_;
  QListWidget *recoveredItemsListWidget_;
};
} // namespace recoverysuite::gui::widgets
