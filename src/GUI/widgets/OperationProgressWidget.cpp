#include "OperationProgressWidget.h"
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QTimer>
#include <QString>

namespace recoverysuite {
namespace gui {
namespace widgets {

OperationProgressWidget::OperationProgressWidget(QWidget *parent)
    : QWidget(parent) {
    setupUI();
}

OperationProgressWidget::~OperationProgressWidget() {
    // Destructor
}

void OperationProgressWidget::setupUI() {
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Progress group box
    progressGroupBox_ = new QGroupBox("Operation Progress", this);
    progressLayout_ = new QVBoxLayout(progressGroupBox_);
    progressLayout_->setContentsMargins(15, 20, 15, 15);
    progressLayout_->setSpacing(10);

    // Operation name label
    operationLabel_ = new QLabel("No operation running", this);
    operationLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    operationLabel_->setStyleSheet(
        "QLabel { "
        "   font-weight: bold; "
        "   font-size: 14px; "
        "   padding: 4px; "
        "}");
    progressLayout_->addWidget(operationLabel_);

    // Progress bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setMinimum(0);
    progressBar_->setMaximum(100);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(true);
    progressBar_->setStyleSheet(
        "QProgressBar { "
        "   border: 2px solid grey; "
        "   border-radius: 5px; "
        "   text-align: center; "
        "   height: 25px; "
        "}"
        "QProgressBar::chunk { "
        "   background-color: #4CAF50; "
        "   width: 10px; "
        "}");
    progressLayout_->addWidget(progressBar_);

    // Status label
    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusLabel_->setStyleSheet(
        "QLabel { "
        "   padding: 4px; "
        "   background-color: #f9f9f9; "
        "   border: 1px solid #ddd; "
        "   border-radius: 3px; "
        "}");
    progressLayout_->addWidget(statusLabel_);

    // Details label
    detailsLabel_ = new QLabel("", this);
    detailsLabel_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    detailsLabel_->setWordWrap(true);
    detailsLabel_->setStyleSheet(
        "QLabel { "
        "   padding: 4px; "
        "   color: #666; "
        "   font-size: 12px; "
        "}");
    progressLayout_->addWidget(detailsLabel_);

    // Additional labels for detailed information
    processedBytesLabel_ = new QLabel("Processed: 0 bytes", this);
    processedBytesLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    processedBytesLabel_->setStyleSheet("QLabel { padding: 2px; }");
    progressLayout_->addWidget(processedBytesLabel_);

    totalBytesLabel_ = new QLabel("Total: unknown", this);
    totalBytesLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    totalBytesLabel_->setStyleSheet("QLabel { padding: 2px; }");
    progressLayout_->addWidget(totalBytesLabel_);

    elapsedTimeLabel_ = new QLabel("Elapsed: 0s", this);
    elapsedTimeLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    elapsedTimeLabel_->setStyleSheet("QLabel { padding: 2px; }");
    progressLayout_->addWidget(elapsedTimeLabel_);

    recoveredItemsLabel_ = new QLabel("Recovered: 0 items", this);
    recoveredItemsLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    recoveredItemsLabel_->setStyleSheet("QLabel { padding: 2px; }");
    progressLayout_->addWidget(recoveredItemsLabel_);

    failedItemsLabel_ = new QLabel("Failed: 0 items", this);
    failedItemsLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    failedItemsLabel_->setStyleSheet("QLabel { padding: 2px; }");
    progressLayout_->addWidget(failedItemsLabel_);

    cancellableLabel_ = new QLabel("Cancellable: No", this);
    cancellableLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    cancellableLabel_->setStyleSheet("QLabel { padding: 2px; }");
    progressLayout_->addWidget(cancellableLabel_);

    // Add group box to main layout
    mainLayout->addWidget(progressGroupBox_);

    // Add stretch to push everything to top
    mainLayout->addStretch();

    setLayout(mainLayout);
}

void OperationProgressWidget::updateProgress(const QString& operationName, int progress, const QString& statusText) {
    // Update operation name
    operationLabel_->setText(operationName);

    // Update progress bar
    progressBar_->setValue(progress);

    // Update status label
    statusLabel_->setText(statusText);

    // Reset detailed labels to default values
    processedBytesLabel_->setText("Processed: 0 bytes");
    totalBytesLabel_->setText("Total: unknown");
    elapsedTimeLabel_->setText("Elapsed: 0s");
    recoveredItemsLabel_->setText("Recovered: 0 items");
    failedItemsLabel_->setText("Failed: 0 items");
    cancellableLabel_->setText("Cancellable: No");
}

void OperationProgressWidget::updateProgress(const QString& operationName,
                                            int progress,
                                            const QString& statusText,
                                            uint64_t processedBytes,
                                            uint64_t totalBytes,
                                            uint64_t elapsedSeconds,
                                            uint64_t recoveredItemsCount,
                                            uint64_t failedItemsCount,
                                            bool isCancellable) {
    // Update operation name
    operationLabel_->setText(operationName);

    // Update progress bar
    progressBar_->setValue(progress);

    // Update status label
    statusLabel_->setText(statusText);

    // Update detailed labels
    processedBytesLabel_->setText(QString("Processed: %1 bytes").arg(processedBytes));
    if (totalBytes > 0) {
        totalBytesLabel_->setText(QString("Total: %1 bytes").arg(totalBytes));
    } else {
        totalBytesLabel_->setText("Total: unknown");
    }
    elapsedTimeLabel_->setText(QString("Elapsed: %1s").arg(elapsedSeconds));
    recoveredItemsLabel_->setText(QString("Recovered: %1 items").arg(recoveredItemsCount));
    failedItemsLabel_->setText(QString("Failed: %1 items").arg(failedItemsCount));
    cancellableLabel_->setText(QString("Cancellable: %1").arg(isCancellable ? "Yes" : "No"));
}

void OperationProgressWidget::resetProgress() {
    operationLabel_->setText("No operation running");
    progressBar_->setValue(0);
    statusLabel_->setText("Ready");
    detailsLabel_->clear();
    // Reset detailed labels
    processedBytesLabel_->setText("Processed: 0 bytes");
    totalBytesLabel_->setText("Total: unknown");
    elapsedTimeLabel_->setText("Elapsed: 0s");
    recoveredItemsLabel_->setText("Recovered: 0 items");
    failedItemsLabel_->setText("Failed: 0 items");
    cancellableLabel_->setText("Cancellable: No");
}

} // namespace widgets
} // namespace gui
} // namespace recoverysuite