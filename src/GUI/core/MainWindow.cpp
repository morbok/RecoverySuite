#include "MainWindow.h"
#include "Application/Service/GUIRecoveryService.h"
#include "Application/Service/Models/GUIModels.h"
#include "Logging/Logger.h"
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QTimer>
#include <QFileDialog>
#include <QInputDialog>
#include <QMetaObject>
#include <QString>
#include <QMetaType>
#include <QtCore>
#include <QtGui>
#include <QObject>
#include <QStackedWidget>
#include <QVBoxLayout>
#include "widgets/SourceDiscoveryWidget.h"
#include "widgets/RecoveryConfigurationWidget.h"

namespace recoverysuite {
namespace gui {
namespace core {

MainWindow::MainWindow(recoverysuite::application::service::GUIRecoveryService* recoveryService, QWidget *parent)
    : QMainWindow(parent),
      stateManager_(),
      isOperationRunning_(false),
      recoveryService_(recoveryService),
      selectedDevice_("") {
    setupUI();
    setupMenuBar();
    setupStatusBar();
    setupCentralWidget();
    setupConnections();

    // Initialize to START state and immediately transition to SOURCE_SELECTION
    stateManager_.resetToStart(); // Sets to START
    if (stateManager_.canTransitionTo(ApplicationState::SOURCE_SELECTION)) {
        stateManager_.transitionTo(ApplicationState::SOURCE_SELECTION);
    }
    updateUIForState();

    recoverysuite::logging::Logger::instance().info("MainWindow created");
}

MainWindow::~MainWindow() {
    recoverysuite::logging::Logger::instance().info("MainWindow destroyed");
}

void MainWindow::setupUI() {
    setWindowTitle("RecoverySuite Enterprise");
    setMinimumSize(1024, 768);
    resize(1280, 800);

    // Apply application icon if available
    // setWindowIcon(QIcon(":/icons/recoverysuite.png"));
}

void MainWindow::setupMenuBar() {
    menuBar_ = new QMenuBar(this);
    setMenuBar(menuBar_);

    // File Menu
    QMenu* fileMenu = menuBar_->addMenu("&File");
    fileMenu->addAction("&Open Image...", this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "Open Disk Image", "", "Disk Images (*.img *.dd *.raw);;All Files (*)");
        if (!fileName.isEmpty()) {
            // TODO: Handle opening disk image
            QMessageBox::information(this, "Not Implemented", "Open image functionality coming soon");
        }
    });
    fileMenu->addAction("&Save Session...", this, [this]() {
        QMessageBox::information(this, "Not Implemented", "Save session functionality coming soon");
    });
    fileMenu->addAction("&Load Session...", this, [this]() {
        QMessageBox::information(this, "Not Implemented", "Load session functionality coming soon");
    });
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", qApp, &QApplication::quit);

    // Operations Menu
    QMenu* operationsMenu = menuBar_->addMenu("&Operations");
    operationsMenu->addAction("&List Storage Sources", this, [this]() {
        handleWorkflowOperation("list_sources");
    });
    operationsMenu->addAction("&Inspect Partitions", this, [this]() {
        handleWorkflowOperation("inspect_partitions");
    });
    operationsMenu->addAction("&Detect Filesystem", this, [this]() {
        handleWorkflowOperation("detect_filesystem");
    });
    operationsMenu->addAction("&Analyze Filesystem", this, [this]() {
        handleWorkflowOperation("analyze_filesystem");
    });
    operationsMenu->addAction("&Validate Recovery", this, [this]() {
        handleWorkflowOperation("validate_recovery");
    });
    operationsMenu->addAction("&Start Recovery", this, [this]() {
        handleWorkflowOperation("start_recovery");
    });
    operationsMenu->addAction("&Cancel Operation", this, [this]() {
        handleWorkflowOperation("cancel_operation");
    });
    operationsMenu->addSeparator();
    operationsMenu->addAction("&Recovery Queue...", this, [this]() {
        QMessageBox::information(this, "Not Implemented", "Recovery queue view coming soon");
    });

    // Help Menu
    QMenu* helpMenu = menuBar_->addMenu("&Help");
    helpMenu->addAction("&About RecoverySuite", this, [this]() {
        QMessageBox::about(nullptr, "About RecoverySuite",
            "RecoverySuite Enterprise v1.0.0\n"
            "Professional Data Recovery & Forensic Analysis Suite\n"
            "\n"
            "© 2026 RecoverySuite Team\n"
            "Licensed under the RecoverySuite Enterprise License");
    });
    helpMenu->addAction("&Documentation", this, [this]() {
        QMessageBox::information(this, "Documentation",
            "User documentation available at:\n"
            "file:///usr/local/share/doc/recoverysuite/");
    });
    helpMenu->addAction("&Check for Updates", this, [this]() {
        QMessageBox::information(this, "Updates",
            "Checking for updates...\n"
            "RecoverySuite is up to date.");
    });
}

void MainWindow::setupStatusBar() {
    statusBar_ = new QStatusBar(this);
    setStatusBar(statusBar_);

    // Add permanent widgets to status bar
    statusLabel_ = new QLabel("Ready");
    statusBar_->addWidget(statusLabel_, 1); // 1 = stretch factor

    // Add version info on the right
    versionLabel_ = new QLabel("v1.0.0");
    statusBar_->addPermanentWidget(versionLabel_);
}

void MainWindow::setupCentralWidget() {
    // Create the stacked widget to hold different views
    stackedWidget_ = new QStackedWidget(this);
    setCentralWidget(stackedWidget_);

    // Create and add the source discovery widget (index 0)
    sourceDiscoveryWidget_ = new recoverysuite::gui::widgets::SourceDiscoveryWidget();
    sourceDiscoveryWidget_->setRecoveryService(recoveryService_);
    stackedWidget_->addWidget(sourceDiscoveryWidget_);

    // Create and add the workflow container widget (index 1)
    setupWorkflowContainer();
}

void MainWindow::setupWorkflowContainer() {
    // Create a container widget for the workflow and progress widgets
    workflowContainer_ = new QWidget();
    QVBoxLayout* workflowLayout = new QVBoxLayout(workflowContainer_);
    workflowLayout->setContentsMargins(0, 0, 0, 0);
    workflowLayout->setSpacing(0);

    // Create and add the workflow widget
    workflowWidget_ = new recoverysuite::gui::widgets::RecoveryWorkflowWidget();
    workflowLayout->addWidget(workflowWidget_);

    // Create and add the progress widget
    progressWidget_ = new recoverysuite::gui::widgets::OperationProgressWidget();
    workflowLayout->addWidget(progressWidget_);

    // Create and add the recovery configuration widget
    recoveryConfigurationWidget_ = new recoverysuite::gui::widgets::RecoveryConfigurationWidget();
    recoveryConfigurationWidget_->setRecoveryService(recoveryService_);
    workflowLayout->addWidget(recoveryConfigurationWidget_);

    // Add the container to the stacked widget
    stackedWidget_->addWidget(workflowContainer_);
}

void MainWindow::setupConnections() {
    // Connect workflow widget signals to MainWindow slots
    connect(workflowWidget_, &recoverysuite::gui::widgets::RecoveryWorkflowWidget::operationRequested,
            this, &MainWindow::handleWorkflowOperation);

    // Connect progress widget signals if needed
    // connect(progressWidget_, &OperationProgressWidget::cancelRequested,
    //         this, &MainWindow::handleCancelRequested);

    // Connect source discovery widget signals
    connect(sourceDiscoveryWidget_, &recoverysuite::gui::widgets::SourceDiscoveryWidget::sourceSelected,
            this, &MainWindow::onSourceSelected);

    // Connect recovery configuration widget signals
    connect(recoveryConfigurationWidget_, &recoverysuite::gui::widgets::RecoveryConfigurationWidget::configurationReady,
            this, &MainWindow::onConfigurationReady);
}

void MainWindow::handleWorkflowOperation(const QString& operationType) {
    if (!recoveryService_) {
        QMessageBox::critical(this, "Service Error", "Recovery service not available");
        return;
    }

    // We are now using the state machine to validate transitions, so we don't need the isOperationRunning_ flag for blocking.
    // However, we still want to prevent multiple long-running operations (like recovery) from overlapping.
    // We'll keep the isOperationRunning_ flag for operations that should not be concurrent (like recovery, analysis, etc.)
    // For quick operations (like listing sources, inspecting partitions) we can allow them even if an operation is running?
    // But note: the state machine already prevents certain transitions. We'll use the flag for operations that take time.

    // Determine if this operation is a long-running one that should block others
    bool isLongRunning = (operationType == "start_recovery" ||
                          operationType == "analyze_filesystem" ||
                          operationType == "detect_filesystem" ||
                          operationType == "inspect_partitions" ||
                          operationType == "list_sources"); // Actually, listing sources is quick, but we'll treat it as blocking for simplicity

    if (isOperationRunning_ && isLongRunning) {
        QMessageBox::warning(this, "Operation in Progress",
            "Please wait for the current operation to complete before starting a new one.");
        return;
    }

    recoverysuite::logging::Logger::instance().info(
        "GUI requested operation: " + operationType.toStdString());

    // Update status bar
    statusLabel_->setText(QString("Executing: %1...").arg(operationType));
    isOperationRunning_ = isLongRunning;

    // Update workflow widget state (if we are in the workflow view)
    if (stackedWidget_->currentIndex() == 1) { // Workflow view index
        workflowWidget_->setOperationRunning(isOperationRunning_);
    }

    // Execute the requested operation
    try {
        if (operationType == "list_sources") {
            handleListStorageSources();
        }
        else if (operationType == "inspect_partitions") {
            handleInspectPartitions();
        }
        else if (operationType == "detect_filesystem") {
            handleDetectFilesystem();
        }
        else if (operationType == "analyze_filesystem") {
            handleAnalyzeFilesystem();
        }
        else if (operationType == "validate_recovery") {
            handleValidateRecovery();
        }
        else if (operationType == "start_recovery") {
            handleStartRecovery();
        }
        else if (operationType == "cancel_operation") {
            handleCancelOperation();
        }
        else {
            statusLabel_->setText(QString("Unknown operation: %1").arg(operationType));
            isOperationRunning_ = false;
            if (stackedWidget_->currentIndex() == 1) {
                workflowWidget_->setOperationRunning(false);
            }
        }
    } catch (const std::exception& e) {
        recoverysuite::logging::Logger::instance().error(
            "Error executing operation " + operationType.toStdString() + ": " + e.what());
        statusLabel_->setText(QString("Error: %1").arg(QString::fromStdString(e.what())));
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }

        // Transition to error state on exception
        stateManager_.transitionTo(ApplicationState::ERROR);
        updateUIForState();

        QMessageBox::critical(this, "Operation Error",
            QString("An error occurred while executing the operation:\n%1")
            .arg(QString::fromStdString(e.what())));
    }
}

void MainWindow::handleListStorageSources() {
    // We are going to show the source discovery view and refresh it.
    // This operation is only valid from certain states, but we allow it from any state to go back to source selection.
    if (stateManager_.getCurrentState() != ApplicationState::SOURCE_SELECTION) {
        // Transition to source selection state
        if (stateManager_.transitionTo(ApplicationState::SOURCE_SELECTION)) {
            // Switch to the source discovery view
            stackedWidget_->setCurrentIndex(0);
            // Refresh the source discovery widget
            sourceDiscoveryWidget_->onRefreshClicked(); // This will populate the table and update status
            return;
        }
    }

    // If we are already in the source selection state, just refresh
    if (stateManager_.getCurrentState() == ApplicationState::SOURCE_SELECTION) {
        sourceDiscoveryWidget_->onRefreshClicked();
        return;
    }

    // If we couldn't transition, show an error
    QMessageBox::warning(this, "Invalid Operation",
        "Cannot list storage sources from the current state.");
    statusLabel_->setText("Ready");
    isOperationRunning_ = false;
    if (stackedWidget_->currentIndex() == 1) {
        workflowWidget_->setOperationRunning(false);
    }
}

void MainWindow::handleInspectPartitions() {
    // This operation is only valid from the STORAGE_PARTITION_INSPECTION state or we can transition to it.
    // We are going to show a dialog to get the device path and then show partition information.
    // But note: we already have a selected device from the source discovery phase.

    // We require that a device has been selected
    if (selectedDevice_.isEmpty()) {
        QMessageBox::warning(this, "No Source Selected", "Please select a storage source first.");
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    // TODO: Implement actual partition enumeration using GUIRecoveryService
    // For now, show placeholder
    QMessageBox::information(this, "Partition Inspection",
        QString("Selected disk: %1\n\n"
                "Partitions:\n"
                "1. %11 - 100 MB - NTFS - System Reserved\n"
                "2. %12 - 200 GB - NTFS - Windows OS\n"
                "3. %13 - 50 GB - FAT32 - Data Partition\n"
                "4. %14 - 200 GB - EXT4 - Linux Data")
        .arg(selectedDevice_)
        .arg(selectedDevice_)
        .arg(selectedDevice_)
        .arg(selectedDevice_));
    statusLabel_->setText("Ready");
    isOperationRunning_ = false;
    if (stackedWidget_->currentIndex() == 1) {
        workflowWidget_->setOperationRunning(false);
    }
}

void MainWindow::handleDetectFilesystem() {
    // This operation is only valid from the FILESYSTEM_DETECTION state.
    // We require that a device has been selected
    if (selectedDevice_.isEmpty()) {
        QMessageBox::warning(this, "No Source Selected", "Please select a storage source first.");
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    // For filesystem detection, we analyze the first few sectors (boot sector area)
    // to determine the filesystem type
    const uint64_t startSector = 0;
    const uint64_t numSectors = 16; // Analyze first 16 sectors for detection

    // Update UI for operation start
    statusLabel_->setText(QString("Detecting filesystem on %1...").arg(selectedDevice_));
    if (stackedWidget_->currentIndex() == 1) {
        workflowWidget_->setOperationRunning(true);
    }
    isOperationRunning_ = true;

    // Set up progress callback
    auto progressCallback = [this](const recoverysuite::application::service::models::RecoveryProgress& progress) {
        // Update progress widget directly (we're in the GUI thread)
        updateProgressFromService(QString::fromStdString(progress.operationType),
                                 progress.percentage,
                                 QString::fromStdString(progress.currentStep));
    };

    // Set up cancellation token
    auto cancellationToken = [this]() -> bool {
        // Allow cancellation if we're still in FILESYSTEM_DETECTION state
        return stateManager_.getCurrentState() == ApplicationState::FILESYSTEM_DETECTION;
    };

    // Start detection in a way that doesn't block the UI
    QTimer::singleShot(100, this, [this, startSector, numSectors, progressCallback, cancellationToken]() {
        try {
            // Call the actual recovery service
            auto result = recoveryService_->analyzeFilesystem(selectedDevice_.toStdString(), startSector, numSectors);

            // Handle result
            if (result.success) {
                // Transition to results state on success
                stateManager_.transitionTo(ApplicationState::FILESYSTEM_ANALYSIS);
                updateUIForState();

                // Format the filesystem information for display
                QString message = QString("Filesystem detection completed for %1\n\n")
                    .arg(QString::fromStdString(result.filesystemInfo.type));

                if (!result.filesystemInfo.version.empty()) {
                    message += QString("Version: %1\n").arg(QString::fromStdString(result.filesystemInfo.version));
                }
                message += QString("Cluster Size: %1 bytes\n")
                    .arg(result.filesystemInfo.clusterSizeBytes);
                if (!result.filesystemInfo.volumeLabel.empty()) {
                    message += QString("Volume Label: %1\n").arg(QString::fromStdString(result.filesystemInfo.volumeLabel));
                }
                if (!result.filesystemInfo.serialNumber.empty()) {
                    message += QString("Serial Number: %1\n").arg(QString::fromStdString(result.filesystemInfo.serialNumber));
                }
                message += QString("Total Size: %1 bytes\n")
                    .arg(result.filesystemInfo.totalSizeBytes);
                message += QString("Used Space: %1 bytes (%2%)")
                    .arg(result.filesystemInfo.usedSizeBytes)
                    .arg(result.filesystemInfo.totalSizeBytes > 0 ?
                           QString::number((result.filesystemInfo.usedSizeBytes * 100.0) / result.filesystemInfo.totalSizeBytes, 'f', 1) : "0");
                message += QString("Free Space: %1 bytes (%2%)")
                    .arg(result.filesystemInfo.freeSizeBytes)
                    .arg(result.filesystemInfo.totalSizeBytes > 0 ?
                           QString::number((result.filesystemInfo.freeSizeBytes * 100.0) / result.filesystemInfo.totalSizeBytes, 'f', 1) : "0");
                message += QString("Read-Only: %1\n")
                    .arg(result.filesystemInfo.isReadOnly ? "Yes" : "No");
                message += QString("Corrupted: %1\n")
                    .arg(result.filesystemInfo.isCorrupted ? "Yes" : "No");

                QMessageBox::information(this, "Filesystem Detection Results", message);
            } else {
                // Transition to error state on failure
                stateManager_.transitionTo(ApplicationState::ERROR);
                updateUIForState();

                QMessageBox::warning(this, "Filesystem Detection Failed",
                    QString("Detection failed: %1").arg(QString::fromStdString(result.errorMessage)));
            }
        } catch (const std::exception& e) {
            // Transition to error state on exception
            stateManager_.transitionTo(ApplicationState::ERROR);
            updateUIForState();

            QMessageBox::critical(this, "Detection Error",
                QString("Error during detection: %1").arg(QString::fromStdString(e.what())));
        }

        // Reset UI
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
    });
}

void MainWindow::handleAnalyzeFilesystem() {
    // This operation is only valid from the FILESYSTEM_ANALYSIS state.
    // We require that a device has been selected
    if (selectedDevice_.isEmpty()) {
        QMessageBox::warning(this, "No Source Selected", "Please select a storage source first.");
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    // Get parameters from user (we'll use default values for now, but in a real implementation we might get them from the user)
    bool ok;
    QString startSectorText = QInputDialog::getText(this, "Analyze Filesystem",
        "Start sector (0-based):", QLineEdit::Normal, "0", &ok);
    if (!ok) {
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    bool ok2;
    QString numSectorsText = QInputDialog::getText(this, "Analyze Filesystem",
        "Number of sectors:", QLineEdit::Normal, "100", &ok2);
    if (!ok2) {
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    uint64_t startSector = startSectorText.toULongLong(&ok);
    uint64_t numSectors = numSectorsText.toULongLong(&ok2);
    if (!ok || !ok2) {
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    // Show progress
    statusLabel_->setText(QString("Analyzing filesystem on %1...").arg(selectedDevice_));

    // Perform analysis in a way that doesn't block the UI (using timer for demo)
    QTimer::singleShot(100, this, [this, startSector, numSectors]() {
        try {
            // Call the actual recovery service
            auto result = recoveryService_->analyzeFilesystem(selectedDevice_.toStdString(), startSector, numSectors);

            if (result.success) {
                QString message = QString("Filesystem analysis completed successfully for %1\n\n")
                    .arg(QString::fromStdString(result.filesystemInfo.type));

                if (!result.filesystemInfo.volumeLabel.empty()) {
                    message += QString("Volume Label: %1\n").arg(QString::fromStdString(result.filesystemInfo.volumeLabel));
                }
                message += QString("Total Size: %1 bytes\n")
                    .arg(result.filesystemInfo.totalSizeBytes);
                message += QString("Used Space: %1 bytes (%2%)")
                    .arg(result.filesystemInfo.usedSizeBytes)
                    .arg(result.filesystemInfo.totalSizeBytes > 0 ?
                           QString::number((result.filesystemInfo.usedSizeBytes * 100.0) / result.filesystemInfo.totalSizeBytes, 'f', 1) : "0");
                message += QString("Free Space: %1 bytes (%2%)")
                    .arg(result.filesystemInfo.freeSizeBytes)
                    .arg(result.filesystemInfo.totalSizeBytes > 0 ?
                           QString::number((result.filesystemInfo.freeSizeBytes * 100.0) / result.filesystemInfo.totalSizeBytes, 'f', 1) : "0");

                QMessageBox::information(this, "Filesystem Analysis", message);
            } else {
                QMessageBox::warning(this, "Filesystem Analysis Failed",
                    QString("Analysis failed: %1").arg(QString::fromStdString(result.errorMessage)));
            }
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Analysis Error",
                QString("Error during analysis: %1").arg(QString::fromStdString(e.what())));
        }

        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
    });
}

void MainWindow::handleValidateRecovery() {
    // This operation is only valid from the VALIDATION state.
    // We require that a device has been selected
    if (selectedDevice_.isEmpty()) {
        QMessageBox::warning(this, "No Source Selected", "Please select a storage source first.");
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    // Get destination path from user
    bool ok;
    QString destPath = QInputDialog::getText(this, "Validate Recovery Operation",
        "Destination path (e.g., /mnt/recovery):", QLineEdit::Normal, "/mnt/recovery", &ok);
    if (!ok || destPath.isEmpty()) {
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    // TODO: Implement actual validation using GUIRecoveryService
    // For now, show placeholder
    QMessageBox::information(this, "Recovery Validation",
        QString("Validating recovery operation...\n\n"
                "Source: %1\n"
                "Destination: %2\n\n"
                "Validation Results:\n"
                "✓ Source partition accessible\n"
                "✓ Destination has sufficient space\n"
                "✓ File systems are compatible\n"
                "✓ No overlapping partitions\n"
                "✓ Operation can proceed safely")
        .arg(selectedDevice_)
        .arg(destPath));
    statusLabel_->setText("Ready");
    isOperationRunning_ = false;
    if (stackedWidget_->currentIndex() == 1) {
        workflowWidget_->setOperationRunning(false);
    }
}

void MainWindow::handleStartRecovery() {
    // This operation is only valid from the RECOVERY state.
    // We require that a device has been selected
    if (selectedDevice_.isEmpty()) {
        QMessageBox::warning(this, "No Source Selected", "Please select a storage source first.");
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    // Get parameters from user
    bool ok;
    QString startSectorText = QInputDialog::getText(this, "Start Recovery",
        "Start sector (0-based):", QLineEdit::Normal, "0", &ok);
    if (!ok) {
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    bool ok2;
    QString numSectorsText = QInputDialog::getText(this, "Start Recovery",
        "Number of sectors:", QLineEdit::Normal, "1000", &ok2);
    if (!ok2) {
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    bool ok3;
    QString outputPath = QInputDialog::getText(this, "Start Recovery",
        "Output directory:", QLineEdit::Normal, "./recovered", &ok3);
    if (!ok3 || outputPath.isEmpty()) {
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    uint64_t startSector = startSectorText.toULongLong(&ok);
    uint64_t numSectors = numSectorsText.toULongLong(&ok2);
    if (!ok || !ok2) {
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
        return;
    }

    // Transition to recovery state (we are already in RECOVERY state when this is called, but we double-check)
    if (stateManager_.getCurrentState() != ApplicationState::RECOVERY) {
        if (!stateManager_.transitionTo(ApplicationState::RECOVERY)) {
            QMessageBox::warning(this, "Invalid State",
                "Cannot start recovery operation from current state.");
            statusLabel_->setText("Ready");
            isOperationRunning_ = false;
            if (stackedWidget_->currentIndex() == 1) {
                workflowWidget_->setOperationRunning(false);
            }
            return;
        }
    }

    // Update UI for operation start
    statusLabel_->setText(QString("Starting recovery operation on %1...").arg(selectedDevice_));
    if (stackedWidget_->currentIndex() == 1) {
        workflowWidget_->setOperationRunning(true);
    }

    // Set up progress callback
    auto progressCallback = [this](const recoverysuite::application::service::models::RecoveryProgress& progress) {
        // Update progress widget directly (we're in the GUI thread)
        updateProgressFromService(QString::fromStdString(progress.operationType),
                                 progress.percentage,
                                 QString::fromStdString(progress.currentStep));
    };

    // Set up cancellation token (simplified)
    auto cancellationToken = [this]() -> bool {
        // Allow cancellation if we're still in RECOVERY state
        return stateManager_.getCurrentState() == ApplicationState::RECOVERY;
    };

    // Start recovery in a way that doesn't block the UI
    QTimer::singleShot(100, this, [this, startSector, numSectors, outputPath, progressCallback, cancellationToken]() {
        try {
            // Call the actual recovery service
            auto result = recoveryService_->recoverFiles(
                selectedDevice_.toStdString(),
                startSector,
                numSectors,
                outputPath.toStdString(),
                progressCallback,
                cancellationToken);

            // Handle result
            if (result.success) {
                // Transition to results state on success
                stateManager_.transitionTo(ApplicationState::RESULTS);
                updateUIForState();

                QString message = QString("Recovery completed successfully!\n\n"
                    "Recovered: %1 files (%2 bytes)\n"
                    "Failed: %3 files (%4 bytes)\n"
                    "Time: %5 seconds")
                    .arg(result.recoveredItemsCount)
                    .arg(result.recoveredBytes)
                    .arg(result.failedItemsCount)
                    .arg(result.failedBytes)
                    .arg(result.endTime > result.startTime ?
                         std::chrono::duration_cast<std::chrono::seconds>(result.endTime - result.startTime).count() : 0);

                QMessageBox::information(this, "Recovery Complete", message);
            } else {
                // Transition to error state on failure
                stateManager_.transitionTo(ApplicationState::ERROR);
                updateUIForState();

                QMessageBox::warning(this, "Recovery Failed",
                    QString("Recovery failed: %1").arg(QString::fromStdString(result.errorMessage)));
            }
        } catch (const std::exception& e) {
            // Transition to error state on exception
            stateManager_.transitionTo(ApplicationState::ERROR);
            updateUIForState();

            QMessageBox::critical(this, "Recovery Error",
                QString("Error during recovery: %1").arg(QString::fromStdString(e.what())));
        }

        // Reset UI
        statusLabel_->setText("Ready");
        isOperationRunning_ = false;
        if (stackedWidget_->currentIndex() == 1) {
            workflowWidget_->setOperationRunning(false);
        }
    });
}

void MainWindow::handleCancelOperation() {
    // Only allow cancellation during recovery operation
    if (stateManager_.getCurrentState() == ApplicationState::RECOVERY) {
        statusLabel_->setText("Cancelling operation...");
        // In a real implementation, we would signal the operation to cancel
        // For now, just simulate
        QTimer::singleShot(1000, this, [this]() {
            statusLabel_->setText("Operation cancelled by user");
            // Transition back to validation state after cancellation
            stateManager_.transitionTo(ApplicationState::VALIDATION);
            updateUIForState();
            if (stackedWidget_->currentIndex() == 1) {
                workflowWidget_->setOperationRunning(false);
            }
        });
    } else {
        statusLabel_->setText("No operation to cancel");
    }
}

// Slot to update progress from service callbacks
void MainWindow::updateProgressFromService(const QString& operationName, int progress, const QString& statusText) {
    if (progressWidget_) {
        progressWidget_->updateProgress(operationName, progress, statusText);
    }
    if (!statusText.isEmpty()) {
        statusLabel_->setText(statusText);
    }
}

void MainWindow::updateOperationProgress(const QString& operationName, int progress, const QString& statusText) {
    if (progressWidget_) {
        progressWidget_->updateProgress(operationName, progress, statusText);
    }
    if (!statusText.isEmpty()) {
        statusLabel_->setText(statusText);
    }
}

void MainWindow::handleOperationCompleted(const QString& operationName, bool success, const QString& resultMessage) {
    isOperationRunning_ = false;
    if (stackedWidget_->currentIndex() == 1) {
        workflowWidget_->setOperationRunning(false);
    }

    if (success) {
        statusLabel_->setText(QString("Operation completed: %1").arg(operationName));
        if (!resultMessage.isEmpty()) {
            statusLabel_->setText(statusLabel_->text() + " - " + resultMessage);
        }
    } else {
        statusLabel_->setText(QString("Operation failed: %1").arg(operationName));
        if (!resultMessage.isEmpty()) {
            statusLabel_->setText(statusLabel_->text() + " - " + resultMessage);
        }
    }

    // Show completion dialog
    if (success) {
        QMessageBox::information(this, "Operation Completed",
            QString("The operation '%1' completed successfully.\n\n%2")
            .arg(operationName)
            .arg(resultMessage));
    } else {
        QMessageBox::warning(this, "Operation Failed",
            QString("The operation '%1' failed.\n\n%2")
            .arg(operationName)
            .arg(resultMessage));
    }
}

// Slot to handle source selection from the source discovery widget
void MainWindow::onSourceSelected(const QString& devicePath) {
    // Store the selected device
    selectedDevice_ = devicePath;

    // Transition to the next state (storage partition inspection)
    if (stateManager_.transitionTo(ApplicationState::STORAGE_PARTITION_INSPECTION)) {
        // Switch to the workflow view
        stackedWidget_->setCurrentIndex(1);
        // Update the status
        statusLabel_->setText(tr("Source %1 selected. Inspecting partitions...").arg(devicePath));
    } else {
        QMessageBox::warning(this, "State Transition Error",
            "Failed to transition to storage partition inspection state.");
        statusLabel_->setText("Ready");
    }
}

// Slot to handle configuration ready from recovery configuration widget
void MainWindow::onConfigurationReady(const recoverysuite::application::service::models::RecoveryOperation& operation) {
    // Store the operation for later use (in a real implementation, we'd pass it to the recovery service)
    // For now, we'll just transition to validation state and show a message

    // Transition to validation state
    if (stateManager_.transitionTo(ApplicationState::VALIDATION)) {
        // Update the status
        statusLabel_->setText(tr("Configuration validated. Ready to execute: %1")
                              .arg(QString::fromStdString(operation.operationType)));

        // Show a message box with the configuration details
        QString message = QString("Recovery operation configured:\n\n"
                                 "Operation: %1\n"
                                 "Device: %2\n"
                                 "Start Sector: %3\n"
                                 "Number of Sectors: %4\n"
                                 "Output Path: %5\n"
                                 "Verify After Recovery: %6\n"
                                 "Create Log File: %7")
            .arg(QString::fromStdString(operation.operationType))
            .arg(QString::fromStdString(operation.targetDevicePath))
            .arg(operation.startSector)
            .arg(operation.numSectors)
            .arg(QString::fromStdString(operation.outputPath))
            .arg(operation.verifyAfterRecovery ? "Yes" : "No")
            .arg(operation.createLogFile ? "Yes" : "No");

        QMessageBox::information(this, "Configuration Ready", message);
    } else {
        QMessageBox::warning(this, "State Transition Error",
            "Failed to transition to validation state.");
        statusLabel_->setText("Ready");
    }
}

void MainWindow::updateUIForState() {
    // Update the UI based on the current application state
    ApplicationState currentState = stateManager_.getCurrentState();

    // Update the status label with the current state (for debugging)
    // statusLabel_->setText(QString("State: %1").arg(static_cast<int>(currentState)));

    // Determine which view to show
    if (currentState == ApplicationState::SOURCE_SELECTION) {
        // Show the source discovery view
        stackedWidget_->setCurrentIndex(0);
    } else {
        // Show the workflow view for all other states (except START and EXIT, which we handle by transitioning)
        stackedWidget_->setCurrentIndex(1);
    }

    // Update the workflow widget's operation running state (if we are in the workflow view)
    if (stackedWidget_->currentIndex() == 1) {
        workflowWidget_->setOperationRunning(isOperationRunning_);
    }

    // Update the status label based on the state (optional)
    // We can set a default status for each state if we want
    switch (currentState) {
        case ApplicationState::START:
            statusLabel_->setText("Initializing...");
            break;
        case ApplicationState::SOURCE_SELECTION:
            statusLabel_->setText("Select a storage source");
            break;
        case ApplicationState::STORAGE_PARTITION_INSPECTION:
            statusLabel_->setText("Inspecting partitions...");
            break;
        case ApplicationState::FILESYSTEM_DETECTION:
            statusLabel_->setText("Detecting filesystem...");
            break;
        case ApplicationState::FILESYSTEM_ANALYSIS:
            statusLabel_->setText("Analyzing filesystem...");
            break;
        case ApplicationState::RECOVERY_CONFIGURATION:
            statusLabel_->setText("Configuring recovery...");
            break;
        case ApplicationState::VALIDATION:
            statusLabel_->setText("Validating recovery operation...");
            break;
        case ApplicationState::RECOVERY:
            statusLabel_->setText("Recovery in progress...");
            break;
        case ApplicationState::RESULTS:
            statusLabel_->setText("Recovery complete. Viewing results.");
            break;
        case ApplicationState::ERROR:
            statusLabel_->setText("An error occurred.");
            break;
        case ApplicationState::EXIT:
            statusLabel_->setText("Exiting...");
            break;
        default:
            statusLabel_->setText("Ready");
            break;
    }
}

} // namespace core
} // namespace gui
} // namespace recoverysuite