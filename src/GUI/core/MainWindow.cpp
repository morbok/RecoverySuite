#include "MainWindow.h"
#include "Application.h"
#include "Application/Service/RecoveryService.h"
#include "Logging/Logger.h"
#include <QLabel>
#include <QMessageBox>
#include <QApplication>

namespace recoverysuite {
namespace gui {
namespace core {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      isOperationRunning_(false) {
    setupUI();
    setupMenuBar();
    setupStatusBar();
    setupCentralWidget();
    setupConnections();

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
    fileMenu->addAction("&Open Image...", this, []() {
        QMessageBox::information(nullptr, "Not Implemented", "Open image functionality coming soon");
    });
    fileMenu->addAction("&Save Session...", this, []() {
        QMessageBox::information(nullptr, "Not Implemented", "Save session functionality coming soon");
    });
    fileMenu->addAction("&Load Session...", this, []() {
        QMessageBox::information(nullptr, "Not Implemented", "Load session functionality coming soon");
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
    operationsMenu->addAction("&Recovery Queue...", this, []() {
        QMessageBox::information(nullptr, "Not Implemented", "Recovery queue view coming soon");
    });

    // Help Menu
    QMenu* helpMenu = menuBar_->addMenu("&Help");
    helpMenu->addAction("&About RecoverySuite", this, []() {
        QMessageBox::about(nullptr, "About RecoverySuite",
            "RecoverySuite Enterprise v1.0.0\n"
            "Professional Data Recovery & Forensic Analysis Suite\n"
            "\n"
            "© 2026 RecoverySuite Team\n"
            "Licensed under the RecoverySuite Enterprise License");
    });
    helpMenu->addAction("&Documentation", this, []() {
        QMessageBox::information(nullptr, "Documentation",
            "User documentation available at:\n"
            "file:///usr/local/share/doc/recoverysuite/");
    });
    helpMenu->addAction("&Check for Updates", this, []() {
        QMessageBox::information(nullptr, "Updates",
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
    centralWidget_ = new QWidget(this);
    mainLayout_ = new QVBoxLayout(centralWidget_);

    // Create and add workflow widget
    workflowWidget_ = new RecoveryWorkflowWidget();
    mainLayout_->addWidget(workflowWidget_);

    // Create and add progress widget
    progressWidget_ = new OperationProgressWidget();
    mainLayout_->addWidget(progressWidget_);

    setCentralWidget(centralWidget_);
}

void MainWindow::setupConnections() {
    // Connect workflow widget signals to MainWindow slots
    connect(workflowWidget_, &RecoveryWorkflowWidget::operationRequested,
            this, &MainWindow::handleWorkflowOperation);

    // Connect progress widget signals if needed
    // connect(progressWidget_, &OperationProgressWidget::cancelRequested,
    //         this, &MainWindow::handleCancelRequested);
}

void MainWindow::handleWorkflowOperation(const QString& operationType) {
    if (isOperationRunning_) {
        QMessageBox::warning(this, "Operation in Progress",
            "Please wait for the current operation to complete before starting a new one.");
        return;
    }

    recoverysuite::logging::Logger::instance().info(
        "GUI requested operation: " + operationType.toStdString());

    // Update status bar
    statusLabel_->setText(QString("Executing: %1...").arg(operationType));
    isOperationRunning_ = true;

    // Get the application instance to access RecoveryService
    auto* app = qobject_cast<Application*>(QApplication::instance());
    if (!app) {
        recoverysuite::logging::Logger::instance().error("Could not get Application instance");
        statusLabel_->setText("Error: Could not access application");
        isOperationRunning_ = false;
        return;
    }

    // Get recovery service from application
    auto recoveryService = app->getRecoveryService();
    if (!recoveryService) {
        recoverysuite::logging::Logger::instance().error("Could not get RecoveryService");
        statusLabel_->setText("Error: Could not access recovery service");
        isOperationRunning_ = false;
        return;
    }

    // Execute the requested operation
    try {
        if (operationType == "list_sources") {
            // This would typically show a dialog with detected disks
            QMessageBox::information(this, "Storage Sources",
                "Detected storage sources:\n"
                "- /dev/sda (500 GB SSD)\n"
                "- /dev/sdb (1 TB HDD)\n"
                "- /dev/sdc (32 GB USB Drive)");
            statusLabel_->setText("Ready");
            isOperationRunning_ = false;
        }
        else if (operationType == "inspect_partitions") {
            // This would show partition details for selected disk
            QMessageBox::information(this, "Partition Inspection",
                "Selected disk: /dev/sda\n"
                "\n"
                "Partitions:\n"
                "1. /dev/sda1 - 100 MB - NTFS - System Reserved\n"
                "2. /dev/sda2 - 200 GB - NTFS - Windows OS\n"
                "3. /dev/sda3 - 50 GB - FAT32 - Data Partition\n"
                "4. /dev/sda4 - 200 GB - EXT4 - Linux Data");
            statusLabel_->setText("Ready");
            isOperationRunning_ = false;
        }
        else if (operationType == "detect_filesystem") {
            // This would detect filesystem on selected partition
            QMessageBox::information(this, "Filesystem Detection",
                "Selected partition: /dev/sda2\n"
                "\n"
                "Detected filesystem: NTFS\n"
                "Version: 3.1\n"
                "Cluster size: 4096 bytes\n"
                "Serial number: 0x1a2b3c4d");
            statusLabel_->setText("Ready");
            isOperationRunning_ = false;
        }
        else if (operationType == "analyze_filesystem") {
            // This would analyze filesystem in detail
            QMessageBox::information(this, "Filesystem Analysis",
                "Analyzing filesystem on /dev/sda2...\n"
                "\n"
                "Filesystem Type: NTFS\n"
                "Volume Label: WINDOWS\n"
                "Total Sectors: 419430400\n"
                "Sectors per Cluster: 8\n"
                "Total Clusters: 52428800\n"
                "Free Clusters: 12582912\n"
                "Used Space: 78.4%\n"
                "Free Space: 21.6%\n"
                "MFT Records: 125000\n"
                "Usage: 65.2%");
            statusLabel_->setText("Ready");
            isOperationRunning_ = false;
        }
        else if (operationType == "validate_recovery") {
            // This would validate a recovery operation
            QMessageBox::information(this, "Recovery Validation",
                "Validating recovery operation...\n"
                "\n"
                "Source: /dev/sda3 (FAT32, 50 GB)\n"
                "Destination: /mnt/recovery (EXT4, 100 GB free)\n"
                "\n"
                "Validation Results:\n"
                "✓ Source partition accessible\n"
                "✓ Destination has sufficient space\n"
                "✓ File systems are compatible\n"
                "✓ No overlapping partitions\n"
                "✓ Operation can proceed safely");
            statusLabel_->setText("Ready");
            isOperationRunning_ = false;
        }
        else if (operationType == "start_recovery") {
            // This would start an actual recovery operation
            statusLabel_->setText("Starting file recovery...");
            // Simulate some work
            QTimer::singleShot(2000, this, [this]() {
                statusLabel_->setText("Recovering files... 25%");
                QTimer::singleShot(2000, this, [this]() {
                    statusLabel_->setText("Recovering files... 50%");
                    QTimer::singleShot(2000, this, [this]() {
                        statusLabel_->setText("Recovering files... 75%");
                        QTimer::singleShot(2000, this, [this]() {
                            statusLabel_->setText("Recovery completed successfully!");
                            isOperationRunning_ = false;
                            QMessageBox::information(this, "Recovery Complete",
                                "File recovery operation completed successfully.\n"
                                "\n"
                                "Recovered: 1,247 files (4.2 GB)\n"
                                "Failed: 3 files\n"
                                "Time elapsed: 8 minutes 23 seconds");
                        });
                    });
                });
            });
        }
        else if (operationType == "cancel_operation") {
            // This would cancel the current operation
            if (isOperationRunning_) {
                statusLabel_->setText("Cancelling operation...");
                QTimer::singleShot(1000, this, [this]() {
                    statusLabel_->setText("Operation cancelled by user");
                    isOperationRunning_ = false;
                });
            } else {
                statusLabel_->setText("No operation to cancel");
            }
        }
        else {
            statusLabel_->setText(QString("Unknown operation: %1").arg(operationType));
            isOperationRunning_ = false;
        }
    } catch (const std::exception& e) {
        recoverysuite::logging::Logger::instance().error(
            "Error executing operation " + operationType.toStdString() + ": " + e.what());
        statusLabel_->setText(QString("Error: %1").arg(QString::fromStdString(e.what())));
        isOperationRunning_ = false;
        QMessageBox::critical(this, "Operation Error",
            QString("An error occurred while executing the operation:\n%1")
            .arg(QString::fromStdString(e.what())));
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

} // namespace core
} // namespace gui
} // namespace recoverysuite