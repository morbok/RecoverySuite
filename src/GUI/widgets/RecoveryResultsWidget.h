#pragma once
#include <QWidget>
class QLabel;
class QPlainTextEdit;
class QListWidget;
namespace recoverysuite::application::service::models { struct RecoveryResult; }
namespace recoverysuite::gui::widgets {
/** Displays backend results without inferring exports or unreported metrics. */
class RecoveryResultsWidget : public QWidget {
    Q_OBJECT
public:
    explicit RecoveryResultsWidget(QWidget* parent = nullptr);
    void setRecoveryResult(const application::service::models::RecoveryResult& result);
    void clear();
private:
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
