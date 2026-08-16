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
     * Update the progress display with detailed information
     * @param operationName Name of the current operation
     * @param progress Percentage progress (0-100)
     * @param statusText Detailed status message
     * @param processedBytes Number of processed bytes
     * @param totalBytes Total number of bytes (0 if unknown)
     * @param elapsedSeconds Elapsed time in seconds
     * @param recoveredItemsCount Number of recovered items
     * @param failedItemsCount Number of failed items
     * @param isCancellable Whether the operation can be cancelled
     */
    void updateProgress(const QString& operationName,
                        int progress,
                        const QString& statusText,
                        uint64_t processedBytes,
                        uint64_t totalBytes,
                        uint64_t elapsedSeconds,
                        uint64_t recoveredItemsCount,
                        uint64_t failedItemsCount,
                        bool isCancellable);

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
    // Additional labels for detailed information
    QLabel* processedBytesLabel_;
    QLabel* totalBytesLabel_;
    QLabel* elapsedTimeLabel_;
    QLabel* recoveredItemsLabel_;
    QLabel* failedItemsLabel_;
    QLabel* cancellableLabel_;

    // Private methods
    void setupUI();
};

} // namespace widgets
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_WIDGETS_OPERATIONPROGRESSWIDGET_H