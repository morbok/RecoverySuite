#include "OperationProgressWidget.h"
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QTimer>

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

    // For detailed operations, we might want to show more info in detailsLabel_
    // For now, we'll keep it simple
}

void OperationProgressWidget::resetProgress() {
    operationLabel_->setText("No operation running");
    progressBar_->setValue(0);
    statusLabel_->setText("Ready");
    detailsLabel_->clear();
}

} // namespace widgets
} // namespace gui
} // namespace recoverysuite