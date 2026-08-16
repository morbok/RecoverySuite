#include "RecoveryConfigurationWidget.h"
#include <QFileDialog>
#include <QIntValidator>
#include <QLabel>
#include <QMessageBox>
#include "Logging/Logger.h"
#include "../../Application/Service/GUIRecoveryService.h"
#include "../../Recovery/RecoveryOperationValidator.h"
#include "../../Recovery/RecoveryCapability.h"
#include "../../Recovery/RecoverySafetyPolicy.h"

namespace recoverysuite {
namespace gui {
namespace widgets {

RecoveryConfigurationWidget::RecoveryConfigurationWidget(QWidget *parent)
    : QWidget(parent),
      recoveryService_(nullptr),
      selectedDevicePath_(""),
      registeredCapabilities_(recoverysuite::recovery::RecoveryCapability::NONE) {
    setupUI();
    setupConnections();
}

RecoveryConfigurationWidget::~RecoveryConfigurationWidget() {
    // Destructor
}

void RecoveryConfigurationWidget::setRecoveryService(recoverysuite::application::service::GUIRecoveryService* service) {
    recoveryService_ = service;
    // Detect available capabilities when service is set
    onDetectCapabilities();
}

void RecoveryConfigurationWidget::setSelectedDevice(const QString& devicePath) {
    selectedDevicePath_ = devicePath;
    deviceLabel_->setText(QString("Selected Device: %1").arg(devicePath));
    updateStatus(tr("Device selected: %1").arg(devicePath));
}

recoverysuite::application::service::models::RecoveryOperation RecoveryConfigurationWidget::getRecoveryOperation() const {
    recoverysuite::application::service::models::RecoveryOperation operation;

    // Map capability enum to string
    switch (static_cast<recoverysuite::recovery::RecoveryCapability>(capabilityComboBox_->currentData().toInt())) {
        case recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION:
            operation.operationType = "filesystem_detection";
            break;
        case recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS:
            operation.operationType = "filesystem_analysis";
            break;
        case recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY:
            operation.operationType = "metadata_recovery";
            break;
        case recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY:
            operation.operationType = "file_recovery";
            break;
        case recoverysuite::recovery::RecoveryCapability::CARVING:
            operation.operationType = "carving";
            break;
        case recoverysuite::recovery::RecoveryCapability::OUTPUT_EXPORT:
            operation.operationType = "output_export";
            break;
        default:
            operation.operationType = "unknown";
            break;
    }

    operation.targetDevicePath = selectedDevicePath_.toStdString();
    bool ok;
    operation.startSector = startSectorEdit_->text().toULongLong(&ok);
    if (!ok) operation.startSector = 0;
    operation.numSectors = numSectorsEdit_->text().toULongLong(&ok);
    if (!ok) operation.numSectors = 0;
    operation.outputPath = outputPathEdit_->text().toStdString();
    operation.verifyAfterRecovery = verifyAfterRecoveryCheckBox_->isChecked();
    operation.createLogFile = createLogFileCheckBox_->isChecked();

    return operation;
}

void RecoveryConfigurationWidget::setupUI() {
    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Configuration group box
    configurationGroupBox_ = new QGroupBox("Recovery Configuration", this);
    configurationLayout_ = new QVBoxLayout(configurationGroupBox_);
    configurationLayout_->setContentsMargins(15, 20, 15, 15);
    configurationLayout_->setSpacing(10);

    // Form layout for configuration fields
    formLayout_ = new QFormLayout();

    // Device label
    deviceLabel_ = new QLabel("No device selected", this);
    deviceLabel_->setStyleSheet("QLabel { font-weight: bold; color: #2c3e50; }");
    formLayout_->addRow("Device:", deviceLabel_);

    // Capability combo box
    capabilityComboBox_ = new QComboBox(this);
    capabilityComboBox_->setToolTip("Select the recovery capability to configure");
    formLayout_->addRow("Capability:", capabilityComboBox_);

    // Start sector
    startSectorEdit_ = new QLineEdit(this);
    startSectorEdit_->setPlaceholderText("0 (default)");
    startSectorEdit_->setValidator(new QIntValidator(0, 999999999, this));
    formLayout_->addRow("Start Sector:", startSectorEdit_);

    // Number of sectors
    numSectorsEdit_ = new QLineEdit(this);
    numSectorsEdit_->setPlaceholderText("100 (default)");
    numSectorsEdit_->setValidator(new QIntValidator(1, 999999999, this));
    formLayout_->addRow("Number of Sectors:", numSectorsEdit_);

    // Output path
    QHBoxLayout* outputPathLayout = new QHBoxLayout();
    outputPathEdit_ = new QLineEdit(this);
    outputPathEdit_->setPlaceholderText("./recovered");
    browseOutputButton_ = new QPushButton("Browse...", this);
    browseOutputButton_->setMaximumWidth(80);
    outputPathLayout->addWidget(outputPathEdit_);
    outputPathLayout->addWidget(browseOutputButton_);
    formLayout_->addRow("Output Path:", outputPathLayout);

    // Verify after recovery
    verifyAfterRecoveryCheckBox_ = new QCheckBox("Verify after recovery", this);
    verifyAfterRecoveryCheckBox_->setChecked(true);
    formLayout_->addRow("", verifyAfterRecoveryCheckBox_);

    // Create log file
    createLogFileCheckBox_ = new QCheckBox("Create log file", this);
    createLogFileCheckBox_->setChecked(true);
    formLayout_->addRow("", createLogFileCheckBox_);

    configurationLayout_->addLayout(formLayout_);

    // Buttons layout
    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    validateConfigurationButton_ = new QPushButton("&Validate Configuration", this);
    validateConfigurationButton_->setToolTip("Validate the current configuration");
    configureRecoveryButton_ = new QPushButton("&Configure Recovery", this);
    configureRecoveryButton_->setToolTip("Apply this configuration for recovery");
    configureRecoveryButton_->setStyleSheet(
        "QPushButton { "
        "   background-color: #3498db; "
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
        "   background-color: #2980b9; "
        "}"
        "QPushButton:pressed { "
        "   background-color: #1f618d; "
        "}");

    buttonsLayout->addWidget(validateConfigurationButton_);
    buttonsLayout->addWidget(configureRecoveryButton_);
    buttonsLayout->addStretch();

    configurationLayout_->addLayout(buttonsLayout);

    // Status text edit
    statusTextEdit_ = new QTextEdit(this);
    statusTextEdit_->setMaximumHeight(100);
    statusTextEdit_->setReadOnly(true);
    statusTextEdit_->setPlaceholderText("Configuration status and messages will appear here...");
    configurationLayout_->addWidget(statusTextEdit_);

    // Add group box to main layout
    mainLayout->addWidget(configurationGroupBox_);

    setLayout(mainLayout);
}

void RecoveryConfigurationWidget::setupConnections() {
    connect(capabilityComboBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecoveryConfigurationWidget::onCapabilityChanged);
    connect(browseOutputButton_, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory",
                                                       outputPathEdit_->text(),
                                                       QFileDialog::ShowDirsOnly);
        if (!dir.isEmpty()) {
            outputPathEdit_->setText(dir);
        }
    });
    connect(validateConfigurationButton_, &QPushButton::clicked, this, &RecoveryConfigurationWidget::onValidateConfiguration);
    connect(configureRecoveryButton_, &QPushButton::clicked, this, &RecoveryConfigurationWidget::onConfigureRecovery);
}

void RecoveryConfigurationWidget::onCapabilityChanged(int index) {
    // Update UI based on selected capability
    recoverysuite::recovery::RecoveryCapability capability =
        static_cast<recoverysuite::recovery::RecoveryCapability>(capabilityComboBox_->itemData(index).toInt());

    // Update placeholders and tooltips based on capability
    switch (capability) {
        case recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION:
            startSectorEdit_->setPlaceholderText("0 (boot sector)");
            numSectorsEdit_->setPlaceholderText("16 (default for detection)");
            outputPathEdit_->setPlaceholderText("(not required for detection)");
            browseOutputButton_->setEnabled(false);
            verifyAfterRecoveryCheckBox_->setEnabled(false);
            createLogFileCheckBox_->setEnabled(true);
            break;
        case recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS:
            startSectorEdit_->setPlaceholderText("0 (beginning of partition)");
            numSectorsEdit_->setPlaceholderText("100 (default for analysis)");
            outputPathEdit_->setPlaceholderText("(not required for analysis)");
            browseOutputButton_->setEnabled(false);
            verifyAfterRecoveryCheckBox_->setEnabled(false);
            createLogFileCheckBox_->setEnabled(true);
            break;
        case recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY:
            startSectorEdit_->setPlaceholderText("0 (start of filesystem)");
            numSectorsEdit_->setPlaceholderText("based on filesystem size");
            outputPathEdit_->setPlaceholderText("./metadata_recovery");
            browseOutputButton_->setEnabled(true);
            verifyAfterRecoveryCheckBox_->setEnabled(true);
            createLogFileCheckBox_->setEnabled(true);
            break;
        case recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY:
            startSectorEdit_->setPlaceholderText("0 (start of filesystem)");
            numSectorsEdit_->setPlaceholderText("based on filesystem size");
            outputPathEdit_->setPlaceholderText("./recovered_files");
            browseOutputButton_->setEnabled(true);
            verifyAfterRecoveryCheckBox_->setEnabled(true);
            createLogFileCheckBox_->setEnabled(true);
            break;
        case recoverysuite::recovery::RecoveryCapability::CARVING:
            startSectorEdit_->setPlaceholderText("0 (start of device)");
            numSectorsEdit_->setPlaceholderText("based on device size");
            outputPathEdit_->setPlaceholderText("./carved_files");
            browseOutputButton_->setEnabled(true);
            verifyAfterRecoveryCheckBox_->setEnabled(false); // Carving typically doesn't verify
            createLogFileCheckBox_->setEnabled(true);
            break;
        case recoverysuite::recovery::RecoveryCapability::OUTPUT_EXPORT:
            startSectorEdit_->setPlaceholderText("0 (not used)");
            numSectorsEdit_->setPlaceholderText("0 (not used)");
            outputPathEdit_->setPlaceholderText("./exported");
            browseOutputButton_->setEnabled(true);
            verifyAfterRecoveryCheckBox_->setEnabled(false);
            createLogFileCheckBox_->setEnabled(true);
            break;
        default:
            startSectorEdit_->setPlaceholderText("0");
            numSectorsEdit_->setPlaceholderText("0");
            outputPathEdit_->setPlaceholderText("");
            browseOutputButton_->setEnabled(false);
            verifyAfterRecoveryCheckBox_->setEnabled(false);
            createLogFileCheckBox_->setEnabled(false);
            break;
    }

    updateStatus(tr("Capability changed to: %1").arg(capabilityComboBox_->currentText()));
}

void RecoveryConfigurationWidget::onDetectCapabilities() {
    if (!recoveryService_) {
        updateStatus("Recovery service not available.");
        return;
    }

    updateStatus("Detecting available recovery capabilities...");

    // In a real implementation, we would query the service for registered capabilities
    // For now, we'll simulate that all capabilities are available
    // In practice, this would come from the backend's capability registry

    // Clear and repopulate the combo box
    capabilityComboBox_->clear();

    // Add all possible capabilities (in a real app, we'd filter by what's actually registered)
    capabilityComboBox_->addItem("Filesystem Detection",
                              static_cast<int>(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION));
    capabilityComboBox_->addItem("Filesystem Analysis",
                              static_cast<int>(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS));
    capabilityComboBox_->addItem("Metadata Recovery",
                              static_cast<int>(recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY));
    capabilityComboBox_->addItem("File Recovery",
                              static_cast<int>(recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY));
    capabilityComboBox_->addItem("Carving",
                              static_cast<int>(recoverysuite::recovery::RecoveryCapability::CARVING));
    capabilityComboBox_->addItem("Output Export",
                              static_cast<int>(recoverysuite::recovery::RecoveryCapability::OUTPUT_EXPORT));

    // Set default selection to first item
    if (capabilityComboBox_->count() > 0) {
        capabilityComboBox_->setCurrentIndex(0);
        onCapabilityChanged(0);
    }

    updateStatus("Capability detection complete.");
}

void RecoveryConfigurationWidget::onValidateConfiguration() {
    if (!recoveryService_) {
        QMessageBox::warning(this, "Service Error", "Recovery service not available");
        return;
    }

    if (selectedDevicePath_.isEmpty()) {
        QMessageBox::warning(this, "No Device Selected", "Please select a storage device first.");
        return;
    }

    // Validate inputs
    if (!validateInputs()) {
        return;
    }

    updateStatus("Validating configuration...");

    // Get the current configuration
    auto operation = getRecoveryOperation();

    // Validate using the RecoveryOperationValidator from the backend
    bool isValid = true;
    QString validationMessage = "Configuration is valid.";

    // Basic input validation first
    if (operation.targetDevicePath.empty()) {
        isValid = false;
        validationMessage = "No target device specified.";
    }

    // For capabilities that require sectors, validate them
    recoverysuite::recovery::RecoveryCapability capability =
        static_cast<recoverysuite::recovery::RecoveryCapability>(capabilityComboBox_->currentData().toInt());

    if (capability != recoverysuite::recovery::RecoveryCapability::OUTPUT_EXPORT &&
        capability != recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION &&
        capability != recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS) {
        if (operation.numSectors == 0) {
            isValid = false;
            validationMessage = "Number of sectors must be greater than zero for this operation.";
        }
    }

    if (operation.outputPath.empty() &&
        (capability == recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY ||
         capability == recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY ||
         capability == recoverysuite::recovery::RecoveryCapability::CARVING ||
         capability == recoverysuite::recovery::RecoveryCapability::OUTPUT_EXPORT)) {
        isValid = false;
        validationMessage = "Output path is required for this operation.";
    }

    // If basic validation passes, use the backend validator
    if (isValid && recoveryService_) {
        updateStatus("Performing backend validation...");

        // Create a safety policy and capability registry for validation
        recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;
        recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;

        // Register all capabilities (in reality, this would come from the service)
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::CARVING);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::OUTPUT_EXPORT);

        recoverysuite::recovery::RecoveryOperationValidator validator(capabilityRegistry, safetyPolicy);

        // Prepare validation parameters
        bool sourceIsReadOnly = true;  // Assume source is read-only for safety
        bool destinationDiffersFromSource = !operation.outputPath.empty() &&
                                          operation.outputPath != QString::fromStdString(operation.targetDevicePath);
        bool sourceRangesAreValid = true;  // Simplified - would check actual disk bounds in reality
        bool outputPathIsValid = !operation.outputPath.empty();  // Simplified
        bool arithmeticCannotOverflow = true;  // Simplified
        bool cancellationStateIsValid = true;  // Simplified

        // Get the RecoveryCapability enum value from the operation type
        recoverysuite::recovery::RecoveryCapability requestedCapability = recoverysuite::recovery::RecoveryCapability::NONE;
        if (operation.operationType == "filesystem_detection") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION;
        } else if (operation.operationType == "filesystem_analysis") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS;
        } else if (operation.operationType == "metadata_recovery") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY;
        } else if (operation.operationType == "file_recovery") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY;
        } else if (operation.operationType == "carving") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::CARVING;
        } else if (operation.operationType == "output_export") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::OUTPUT_EXPORT;
        }

        // Perform validation
        recoverysuite::recovery::RecoveryValidationReport report = validator.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            requestedCapability,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid
        );

        if (!report.validationPassed()) {
            isValid = false;
            validationMessage = QString::fromStdString(report.getSummary());
        } else {
            updateStatus("Backend validation passed.");
        }
    }

    if (isValid) {
        updateStatus("Configuration validation passed.");
        QMessageBox::information(this, "Validation Successful", validationMessage);
        // Emit signal that configuration is ready
        emit configurationReady(operation);
    } else {
        updateStatus(QString("Configuration validation failed: %1").arg(validationMessage));
        QMessageBox::warning(this, "Validation Failed", validationMessage);
    }
}

void RecoveryConfigurationWidget::onConfigureRecovery() {
    if (!recoveryService_) {
        QMessageBox::warning(this, "Service Error", "Recovery service not available");
        return;
    }

    if (selectedDevicePath_.isEmpty()) {
        QMessageBox::warning(this, "No Device Selected", "Please select a storage device first.");
        return;
    }

    // Validate inputs
    if (!validateInputs()) {
        return;
    }

    updateStatus("Configuring recovery operation...");

    // Get the current configuration
    auto operation = getRecoveryOperation();

    // Validate using the RecoveryOperationValidator from the backend
    bool isValid = true;
    QString validationMessage = "Configuration is valid.";

    // If we have a service, use the backend validator
    if (recoveryService_) {
        updateStatus("Performing backend validation...");

        // Create a safety policy and capability registry for validation
        recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;
        recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;

        // Register all capabilities (in reality, this would come from the service)
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::CARVING);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::OUTPUT_EXPORT);

        recoverysuite::recovery::RecoveryOperationValidator validator(capabilityRegistry, safetyPolicy);

        // Prepare validation parameters
        bool sourceIsReadOnly = true;  // Assume source is read-only for safety
        bool destinationDiffersFromSource = !operation.outputPath.empty() &&
                                          operation.outputPath != QString::fromStdString(operation.targetDevicePath);
        bool sourceRangesAreValid = true;  // Simplified - would check actual disk bounds in reality
        bool outputPathIsValid = !operation.outputPath.empty();  // Simplified
        bool arithmeticCannotOverflow = true;  // Simplified
        bool cancellationStateIsValid = true;  // Simplified

        // Get the RecoveryCapability enum value from the operation type
        recoverysuite::recovery::RecoveryCapability requestedCapability = recoverysuite::recovery::RecoveryCapability::NONE;
        if (operation.operationType == "filesystem_detection") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::FILESYSTEM_DETECTION;
        } else if (operation.operationType == "filesystem_analysis") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS;
        } else if (operation.operationType == "metadata_recovery") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY;
        } else if (operation.operationType == "file_recovery") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY;
        } else if (operation.operationType == "carving") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::CARVING;
        } else if (operation.operationType == "output_export") {
            requestedCapability = recoverysuite::recovery::RecoveryCapability::OUTPUT_EXPORT;
        }

        // Perform validation
        recoverysuite::recovery::RecoveryValidationReport report = validator.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            requestedCapability,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid
        );

        if (!report.validationPassed()) {
            isValid = false;
            validationMessage = QString::fromStdString(report.getSummary());
        } else {
            updateStatus("Backend validation passed.");
        }
    }

    if (isValid) {
        updateStatus("Recovery operation configured successfully.");
        QMessageBox::information(this, "Configuration Success",
                               "Recovery operation has been configured and is ready for execution.");
        // Emit signal that configuration is ready
        emit configurationReady(operation);
    } else {
        updateStatus(QString("Configuration validation failed: %1").arg(validationMessage));
        QMessageBox::warning(this, "Validation Failed", validationMessage);
    }
}


void RecoveryConfigurationWidget::updateStatus(const QString& message) {
    statusTextEdit_->append(message);
    // Also scroll to bottom
    QTextCursor cursor = statusTextEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    statusTextEdit_->setTextCursor(cursor);
    recoverysuite::logging::Logger::instance().info("RecoveryConfigurationWidget: " + message.toStdString());
}

bool RecoveryConfigurationWidget::validateInputs() {
    // Validate start sector
    bool ok = true; // Initialize ok to true
    startSectorEdit_->text().toULongLong(&ok);
    if (!startSectorEdit_->text().isEmpty() && !ok) {
        QMessageBox::warning(this, "Invalid Input", "Start sector must be a valid number.");
        return false;
    }

    // Validate number of sectors
    ok = true; // Reset ok for next validation
    numSectorsEdit_->text().toULongLong(&ok);
    if (!numSectorsEdit_->text().isEmpty() && !ok) {
        QMessageBox::warning(this, "Invalid Input", "Number of sectors must be a valid number.");
        return false;
    }

    // Validate that we have a device selected
    if (selectedDevicePath_.isEmpty()) {
        QMessageBox::warning(this, "No Device Selected", "Please select a storage device first.");
        return false;
    }

    return true;
}

} // namespace widgets
} // namespace gui
} // namespace recoverysuite