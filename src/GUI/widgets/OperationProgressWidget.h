#ifndef RECOVERYSUITE_GUI_WIDGETS_OPERATIONPROGRESSWIDGET_H
#define RECOVERYSUITE_GUI_WIDGETS_OPERATIONPROGRESSWIDGET_H

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>

namespace recoverysuite {
namespace gui {
namespace widgets {

/**
 * OperationProgressWidget
 *
 * Widget displaying progress and status of ongoing recovery operations.
 * Shows progress bar, operation name, and detailed status information.
 */
class OperationProgressWidget : public QWidget {
    Q_OBJECT

public:
    explicit OperationProgressWidget(QWidget *parent = nullptr);
    ~OperationProgressWidget() override;

public slots:
    /**
     * Update the progress display
     * @param operationName Name of the current operation
     * @param progress Percentage progress (0-100)
     * @param statusText Detailed status message
     */
    void updateProgress(const QString& operationName, int progress, const QString& statusText);

    /**
     * Reset the progress display to initial state
     */
    void resetProgress();

private:
    // UI Components
    QGroupBox* progressGroupBox_;
    QVBoxLayout* progressLayout_;

    QLabel* operationLabel_;
    QProgressBar* progressBar_;
    QLabel* statusLabel_;
    QLabel* detailsLabel_;
};

} // namespace widgets
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_WIDGETS_OPERATIONPROGRESSWIDGET_H