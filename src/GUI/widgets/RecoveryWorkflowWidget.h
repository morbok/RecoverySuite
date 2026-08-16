#ifndef RECOVERYSUITE_GUI_WIDGETS_RECOVERYWORKFLOWWIDGET_H
#define RECOVERYSUITE_GUI_WIDGETS_RECOVERYWORKFLOWWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

namespace recoverysuite {
namespace gui {
namespace widgets {

/**
 * RecoveryWorkflowWidget
 *
 * Widget providing workflow controls for common recovery operations.
 * Contains buttons for initiating various recovery workflow steps.
 */
class RecoveryWorkflowWidget : public QWidget {
    Q_OBJECT

public:
    explicit RecoveryWorkflowWidget(QWidget *parent = nullptr);
    ~RecoveryWorkflowWidget() override;

signals:
    /**
     * Emitted when an operation is requested from the UI
     * @param operationType Type of operation requested
     */
    void operationRequested(const QString& operationType);

private slots:
    void onListSourcesClicked();
    void onInspectPartitionsClicked();
    void onDetectFilesystemClicked();
    void onAnalyzeFilesystemClicked();
    void onValidateRecoveryClicked();
    void onStartRecoveryClicked();
    void onCancelOperationClicked();

private:
    // UI Components
    QGroupBox* workflowGroupBox_;
    QVBoxLayout* workflowLayout_;

    QPushButton* listSourcesButton_;
    QPushButton* inspectPartitionsButton_;
    QPushButton* detectFilesystemButton_;
    QPushButton* analyzeFilesystemButton_;
    QPushButton* validateRecoveryButton_;
    QPushButton* startRecoveryButton_;
    QPushButton* cancelOperationButton_;

    QLabel* statusLabel_;

    // Private methods
    void setupUI();
    void setupConnections();
    void setOperationCompleted(bool success, const QString& message);

public:
    /**
     * @brief Set the operation running state
     * @param running True if operation is running, false otherwise
     */
    void setOperationRunning(bool running);
};

} // namespace widgets
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_WIDGETS_RECOVERYWORKFLOWWIDGET_H