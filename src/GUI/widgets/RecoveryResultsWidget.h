#pragma once
#include <QWidget>
#include <QStringList>
class QLabel;
class QPlainTextEdit;
class QListWidget;
class QTreeWidget;
class QLineEdit;
class QPushButton;
namespace recoverysuite::application::service::models { struct RecoveryResult; }
namespace recoverysuite::gui::widgets {
/** Displays backend results without inferring exports or unreported metrics. */
class RecoveryResultsWidget : public QWidget {
    Q_OBJECT
public:
    explicit RecoveryResultsWidget(QWidget* parent = nullptr);
    void setRecoveryResult(const application::service::models::RecoveryResult& result);
    void clear();
signals:
    void recoverSelection(const QStringList& names, const QString& destination);
private:
    QTreeWidget* candidates_;
    QLineEdit* filter_;
    QLineEdit* output_;
    QPushButton* recover_;
    QWidget* reviewControls_;
    QLabel* statusLabel_;
    QLabel* operationLabel_;
    QLabel* processedBytesLabel_;
    QLabel* recoveredBytesLabel_;
    QLabel* failedBytesLabel_;
    QLabel* recoveredItemsLabel_;
    QLabel* failedItemsLabel_;
    QLabel* timeTakenLabel_;
    QLabel* destinationLabel_;
    QLabel* exportLabel_;
    QPlainTextEdit* detailsTextEdit_;
    QListWidget* recoveredItemsListWidget_;
};
}
