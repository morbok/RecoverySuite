#include "RecoveryWorkflowWidget.h"
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QTimer>

namespace recoverysuite {
namespace gui {
namespace widgets {

RecoveryWorkflowWidget::RecoveryWorkflowWidget(QWidget *parent)
    : QWidget(parent) {
    setupUI();
    setupConnections();
}

RecoveryWorkflowWidget::~RecoveryWorkflowWidget() {
    // Destructor
}

void RecoveryWorkflowWidget::setupUI() {
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Workflow group box
    workflowGroupBox_ = new QGroupBox("Recovery Workflow", this);
    workflowLayout_ = new QVBoxLayout(workflowGroupBox_);
    workflowLayout_->setContentsMargins(15, 20, 15, 15);
    workflowLayout_->setSpacing(10);

    // Create buttons
    listSourcesButton_ = new QPushButton("&List Storage Sources", this);
    listSourcesButton_->setToolTip("Detect and list available storage devices");
    workflowLayout_->addWidget(listSourcesButton_);

    inspectPartitionsButton_ = new QPushButton("&Inspect Partitions", this);
    inspectPartitionsButton_->setToolTip("Examine partition table of selected disk");
    workflowLayout_->addWidget(inspectPartitionsButton_);

    detectFilesystemButton_ = new QPushButton("&Detect Filesystem", this);
    detectFilesystemButton_->setToolTip("Identify filesystem type on selected partition");
    workflowLayout_->addWidget(detectFilesystemButton_);

    analyzeFilesystemButton_ = new QPushButton("&Analyze Filesystem", this);
    analyzeFilesystemButton_->setToolTip("Perform detailed analysis of filesystem structure");
    workflowLayout_->addWidget(analyzeFilesystemButton_);

    validateRecoveryButton_ = new QPushButton("&Validate Recovery Operation", this);
    validateRecoveryButton_->setToolTip("Check if recovery operation can proceed safely");
    workflowLayout_->addWidget(validateRecoveryButton_);

    startRecoveryButton_ = new QPushButton("&Start Recovery", this);
    startRecoveryButton_->setToolTip("Begin file recovery process");
    startRecoveryButton_->setStyleSheet(
        "QPushButton { "
        "   background-color: #4CAF50; "
        "   color: white; "
        "   border: none; "
        "   padding: 8px 16px; "
        "   text-align: center; "
        "   text-decoration: none; "
        "   display: inline-block; "
        "   font-size: 14px; "
        "   margin: 4px 2px; "
        "   border-radius: 4px; "
        "}"
        "QPushButton:hover { "
        "   background-color: #45a049; "
        "}"
        "QPushButton:pressed { "
        "   background-color: #3d8b40; "
        "}");
    workflowLayout_->addWidget(startRecoveryButton_);

    cancelOperationButton_ = new QPushButton("&Cancel Operation", this);
    cancelOperationButton_->setToolTip("Cancel currently running operation");
    cancelOperationButton_->setEnabled(false); // Initially disabled
    workflowLayout_->addWidget(cancelOperationButton_);

    // Status label
    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setStyleSheet(
        "QLabel { "
        "   padding: 8px; "
        "   background-color: #f0f0f0; "
        "   border: 1px solid #ccc; "
        "   border-radius: 4px; "
        "}");
    workflowLayout_->addWidget(statusLabel_);

    // Add group box to main layout
    mainLayout->addWidget(workflowGroupBox_);

    // Add stretch to push everything to top
    mainLayout->addStretch();

    setLayout(mainLayout);
}

void RecoveryWorkflowWidget::setupConnections() {
    connect(listSourcesButton_, &QPushButton::clicked,
            this, &RecoveryWorkflowWidget::onListSourcesClicked);
    connect(inspectPartitionsButton_, &QPushButton::clicked,
            this, &RecoveryWorkflowWidget::onInspectPartitionsClicked);
    connect(detectFilesystemButton_, &QPushButton::clicked,
            this, &RecoveryWorkflowWidget::onDetectFilesystemClicked);
    connect(analyzeFilesystemButton_, &QPushButton::clicked,
            this, &RecoveryWorkflowWidget::onAnalyzeFilesystemClicked);
    connect(validateRecoveryButton_, &QPushButton::clicked,
            this, &RecoveryWorkflowWidget::onValidateRecoveryClicked);
    connect(startRecoveryButton_, &QPushButton::clicked,
            this, &RecoveryWorkflowWidget::onStartRecoveryClicked);
    connect(cancelOperationButton_, &QPushButton::clicked,
            this, &RecoveryWorkflowWidget::onCancelOperationClicked);
}

void RecoveryWorkflowWidget::onListSourcesClicked() {
    emit operationRequested("list_sources");
    statusLabel_->setText("Listing storage sources...");
}

void RecoveryWorkflowWidget::onInspectPartitionsClicked() {
    emit operationRequested("inspect_partitions");
    statusLabel_->setText("Inspecting partitions...");
}

void RecoveryWorkflowWidget::onDetectFilesystemClicked() {
    emit operationRequested("detect_filesystem");
    statusLabel_->setText("Detecting filesystem...");
}

void RecoveryWorkflowWidget::onAnalyzeFilesystemClicked() {
    emit operationRequested("analyze_filesystem");
    statusLabel_->setText("Analyzing filesystem...");
}

void RecoveryWorkflowWidget::onValidateRecoveryClicked() {
    emit operationRequested("validate_recovery");
    statusLabel_->setText("Validating recovery operation...");
}

void RecoveryWorkflowWidget::onStartRecoveryClicked() {
    emit operationRequested("start_recovery");
    statusLabel_->setText("Starting recovery operation...");
    // Disable start button and enable cancel button during operation
    startRecoveryButton_->setEnabled(false);
    cancelOperationButton_->setEnabled(true);
}

void RecoveryWorkflowWidget::onCancelOperationClicked() {
    emit operationRequested("cancel_operation");
    statusLabel_->setText("Cancelling operation...");
    // Re-enable start button and disable cancel button
    startRecoveryButton_->setEnabled(true);
    cancelOperationButton_->setEnabled(false);
}

// Public methods to update UI state from MainWindow
void RecoveryWorkflowWidget::setOperationRunning(bool running) {
    startRecoveryButton_->setEnabled(!running);
    cancelOperationButton_->setEnabled(running);

    if (running) {
        statusLabel_->setText("Operation in progress...");
    } else {
        statusLabel_->setText("Ready");
    }
}

void RecoveryWorkflowWidget::setOperationCompleted(bool success, const QString& message) {
    startRecoveryButton_->setEnabled(true);
    cancelOperationButton_->setEnabled(false);

    if (success) {
        statusLabel_->setText(QString("Operation completed: %1").arg(message));
    } else {
        statusLabel_->setText(QString("Operation failed: %1").arg(message));
    }
}

} // namespace widgets
} // namespace gui
} // namespace recoverysuite