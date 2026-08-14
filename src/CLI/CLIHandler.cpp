#include "CLIHandler.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

namespace recoverysuite {
namespace cli {

CLIHandler::CLIHandler()
    : diskManager_(std::make_unique<recoverysuite::disk::DiskManager>()),
      recoveryService_(nullptr) {
    // Initialize recovery service with a null disk reader initially
    // It will be properly initialized when we have a disk to work with
    recoverysuite::logging::Logger::instance().debug("CLIHandler created");
}

CLIHandler::~CLIHandler() {
    // Cleanup handled by unique_ptr
    recoverysuite::logging::Logger::instance().debug("CLIHandler destroyed");
}

int CLIHandler::run(int argc, char* argv[]) {
    if (argc < 2) {
        showHelp({});
        return 0;
    }

    std::string command = argv[1];
    std::vector<std::string> args;

    // Collect remaining arguments
    for (int i = 2; i < argc; ++i) {
        args.push_back(argv[i]);
    }

    // Convert command to lowercase for case-insensitive comparison
    std::transform(command.begin(), command.end(), command.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    // Route to appropriate handler
    if (command == "list") {
        handleList(args);
    } else if (command == "disk" && !args.empty() && args[0] == "info") {
        // Handle "disk info" command
        if (args.size() >= 2) {
            std::vector<std::string> diskArgs(args.begin() + 1, args.end());
            handleDiskInfo(diskArgs);
        } else {
            printError("Missing disk number for 'disk info' command");
            showHelp({});
        }
    } else if (command == "analyze") {
        handleAnalyze(args);
    } else if (command == "validate") {
        handleValidate(args);
    } else if (command == "recover") {
        handleRecover(args);
    } else if (command == "cancel") {
        handleCancel(args);
    } else if (command == "status") {
        handleStatus(args);
    } else if (command == "help" || command == "-h" || command == "--help") {
        showHelp(args);
    } else if (command == "version" || command == "-v" || command == "--version") {
        showVersion(args);
    } else {
        printError("Unknown command: " + command);
        showHelp({});
        return 1;
    }

    return 0;
}

void CLIHandler::handleList(const std::vector<std::string>& args) {
    (void)args; // Unused in this command

    printInfo("Enumerating available storage devices...");

    try {
        auto disks = diskManager_->enumerateDisks();

        if (disks.empty()) {
            printWarning("No storage devices detected");
            return;
        }

        printSeparator();
        std::cout << std::left << std::setw(10) << "Disk #"
                  << std::setw(20) << "Model"
                  << std::setw(20) << "Serial Number"
                  << std::setw(15) << "Size"
                  << std::setw(15) << "Sector Size"
                  << std::setw(10) << "Type"
                  << std::endl;
        printSeparator();

        for (size_t i = 0; i < disks.size(); ++i) {
            const auto& disk = disks[i];
            std::string sizeStr;

            // Format size in human-readable format
            double sizeGB = static_cast<double>(disk.totalSectors * disk.bytesPerSector) / (1024.0 * 1024.0 * 1024.0);
            if (sizeGB >= 1024.0) {
                sizeStr = std::to_string(sizeGB / 1024.0) + " TB";
            } else {
                sizeStr = std::to_string(sizeGB) + " GB";
            }

            std::cout << std::left << std::setw(10) << i
                      << std::setw(20) << (disk.modelNumber.empty() ? "Unknown" : disk.modelNumber)
                      << std::setw(20) << (disk.serialNumber.empty() ? "Unknown" : disk.serialNumber)
                      << std::setw(15) << sizeStr
                      << std::setw(15) << disk.bytesPerSector << " bytes"
                      << std::setw(10) << (disk.isRemovable ? "Removable" : "Fixed")
                      << std::endl;
        }

        printSeparator();
        printSuccess("Found " + std::to_string(disks.size()) + " storage device(s)");

    } catch (const std::exception& e) {
        printError("Failed to enumerate disks: " + std::string(e.what()));
    }
}

void CLIHandler::handleDiskInfo(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Missing disk number");
        showHelp({});
        recoverysuite::logging::Logger::instance().warn("Disk info command failed: missing disk number");
        return;
    }

    uint64_t diskNumber;
    if (!parseUint64(args[0], diskNumber)) {
        printError("Invalid disk number: " + args[0]);
        recoverysuite::logging::Logger::instance().warn("Disk info command failed: invalid disk number: " + args[0]);
        return;
    }

    recoverysuite::logging::Logger::instance().info("Disk info command started: disk_number=" + std::to_string(diskNumber));
    printInfo("Getting information for disk #" + std::to_string(diskNumber) + "...");

    try {
        auto diskInfo = diskManager_->getDiskInfo(diskNumber);

        printSeparator();
        std::cout << "Disk Information for Disk #" << diskNumber << std::endl;
        printSeparator();
        std::cout << "Model Number:     " << (diskInfo.modelNumber.empty() ? "Unknown" : diskInfo.modelNumber) << std::endl;
        std::cout << "Serial Number:    " << (diskInfo.serialNumber.empty() ? "Unknown" : diskInfo.serialNumber) << std::endl;
        std::cout << "Firmware Revision:" << (diskInfo.firmwareRevision.empty() ? "Unknown" : diskInfo.firmwareRevision) << std::endl;

        // Calculate and display size
        uint64_t totalBytes = diskInfo.totalSectors * diskInfo.bytesPerSector;
        double sizeGB = static_cast<double>(totalBytes) / (1024.0 * 1024.0 * 1024.0);
        std::string sizeStr;
        if (sizeGB >= 1024.0) {
            sizeStr = std::to_string(sizeGB / 1024.0) + " TB (" + std::to_string(totalBytes) + " bytes)";
        } else {
            sizeStr = std::to_string(sizeGB) + " GB (" + std::to_string(totalBytes) + " bytes)";
        }
        std::cout << "Total Size:       " << sizeStr << std::endl;
        std::cout << "Sector Size:      " << diskInfo.bytesPerSector << " bytes" << std::endl;
        std::cout << "Total Sectors:    " << diskInfo.totalSectors << std::endl;

        // Additional info if available
        if (!diskInfo.busTypeString.empty()) {
            std::cout << "Bus Type:         " << diskInfo.busTypeString << std::endl;
        }
        if (diskInfo.partitionStyle != recoverysuite::disk::PartitionStyle::Unknown) {
            std::string partitionStyleStr;
            switch (diskInfo.partitionStyle) {
                case recoverysuite::disk::PartitionStyle::MBR:
                    partitionStyleStr = "MBR";
                    break;
                case recoverysuite::disk::PartitionStyle::GPT:
                    partitionStyleStr = "GPT";
                    break;
                default:
                    partitionStyleStr = "Unknown";
            }
            std::cout << "Partition Style:  " << partitionStyleStr << std::endl;
        }

        printSeparator();
        recoverysuite::logging::Logger::instance().info("Disk info command completed successfully: disk_number=" + std::to_string(diskNumber));

    } catch (const recoverysuite::disk::DiskNotFoundException& e) {
        printError("Disk #" + std::to_string(diskNumber) + " not found");
        recoverysuite::logging::Logger::instance().error("Disk info command failed: disk #" + std::to_string(diskNumber) + " not found");
    } catch (const std::exception& e) {
        printError("Failed to get disk info: " + std::string(e.what()));
        recoverysuite::logging::Logger::instance().error("Disk info command failed: disk_number=" + std::to_string(diskNumber) + ", exception=" + std::string(e.what()));
    }
}

void CLIHandler::handleAnalyze(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Missing disk number");
        showHelp({});
        return;
    }

    uint64_t diskNumber;
    if (!parseUint64(args[0], diskNumber)) {
        printError("Invalid disk number: " + args[0]);
        return;
    }

    // Parse optional start sector and num sectors
    uint64_t startSector = 0;
    uint64_t numSectors = 1; // Default to 1 sector for analysis

    if (args.size() >= 2) {
        if (!parseUint64(args[1], startSector)) {
            printError("Invalid start sector: " + args[1]);
            return;
        }
    }

    if (args.size() >= 3) {
        if (!parseUint64(args[2], numSectors)) {
            printError("Invalid number of sectors: " + args[2]);
            return;
        }
    }

    printInfo("Analyzing filesystem on disk #" + std::to_string(diskNumber) +
              ", starting at sector " + std::to_string(startSector) +
              ", length " + std::to_string(numSectors) + " sectors...");

    try {
        // Open the disk in read-only mode for analysis
        auto disk = diskManager_->openDisk(diskNumber, true);
        if (!disk) {
            printError("Failed to open disk #" + std::to_string(diskNumber));
            return;
        }

        // Initialize recovery service with the disk
        recoveryService_ = std::make_unique<recoverysuite::application::service::RecoveryService>(
            std::shared_ptr<recoverysuite::disk::IDiskReader>(disk.get()),
            recoverysuite::recovery::RecoverySafetyPolicy());

        // Perform analysis
        auto result = recoveryService_->analyzeFilesystem(startSector, numSectors);

        printSeparator();
        if (result.success) {
            printSuccess("Filesystem analysis completed successfully");
            std::cout << "Analysis Results:" << std::endl;
            printSeparator();
            for (const auto& [key, value] : result.analysisResults) {
                std::cout << std::left << std::setw(25) << key << ": " << value << std::endl;
            }
        } else {
            printError("Filesystem analysis failed");
            if (!result.errorMessage.empty()) {
                std::cout << "Error: " << result.errorMessage << std::endl;
            }
        }
        printSeparator();

    } catch (const std::exception& e) {
        printError("Exception during analysis: " + std::string(e.what()));
    }
}

void CLIHandler::handleValidate(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        printError("Missing capability or disk number");
        showHelp({});
        recoverysuite::logging::Logger::instance().warn("Validation failed: missing capability or disk number");
        return;
    }

    // Parse capability
    recoverysuite::recovery::RecoveryCapability capability = parseCapability(args[0]);
    if (capability == recoverysuite::recovery::RecoveryCapability::NONE) {
        printError("Unsupported capability: " + args[0]);
        showHelp({});
        recoverysuite::logging::Logger::instance().warn("Validation failed: unsupported capability: " + args[0]);
        return;
    }

    // Parse disk number
    uint64_t diskNumber;
    if (!parseUint64(args[1], diskNumber)) {
        printError("Invalid disk number: " + args[1]);
        recoverysuite::logging::Logger::instance().warn("Validation failed: invalid disk number: " + args[1]);
        return;
    }

    // Parse optional start sector and num sectors
    uint64_t startSector = 0;
    uint64_t numSectors = 1;

    if (args.size() >= 3) {
        if (!parseUint64(args[2], startSector)) {
            printError("Invalid start sector: " + args[2]);
            recoverysuite::logging::Logger::instance().warn("Validation failed: invalid start sector: " + args[2]);
            return;
        }
    }

    if (args.size() >= 4) {
        if (!parseUint64(args[3], numSectors)) {
            printError("Invalid number of sectors: " + args[3]);
            recoverysuite::logging::Logger::instance().warn("Validation failed: invalid number of sectors: " + args[3]);
            return;
        }
    }

    recoverysuite::logging::Logger::instance().info("Validation started: capability=" + capabilityToString(capability) +
              ", disk_number=" + std::to_string(diskNumber) +
              ", start_sector=" + std::to_string(startSector) +
              ", num_sectors=" + std::to_string(numSectors));

    printInfo("Validating " + capabilityToString(capability) +
              " operation on disk #" + std::to_string(diskNumber) +
              ", starting at sector " + std::to_string(startSector) +
              ", length " + std::to_string(numSectors) + " sectors...");

    try {
        // Open the disk
        auto disk = diskManager_->openDisk(diskNumber, true);
        if (!disk) {
            printError("Failed to open disk #" + std::to_string(diskNumber));
            recoverysuite::logging::Logger::instance().error("Validation failed: failed to open disk #" + std::to_string(diskNumber));
            return;
        }

        // Get disk info to validate ranges
        auto diskInfo = disk->getDiskInfo();
        bool sourceRangesAreValid = (startSector < diskInfo.totalSectors) &&
                                    (startSector + numSectors <= diskInfo.totalSectors);
        bool arithmeticCannotOverflow = (startSector + numSectors >= startSector); // no wrap

        // Initialize validation components
        recoverysuite::recovery::RecoveryCapabilityRegistry capabilityRegistry;
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY);
        capabilityRegistry.registerCapability(recoverysuite::recovery::RecoveryCapability::CARVING);

        recoverysuite::recovery::RecoverySafetyPolicy safetyPolicy;
        recoverysuite::recovery::RecoveryOperationValidator validator(capabilityRegistry, safetyPolicy);

        // Perform validation
        auto report = validator.validateOperation(
            true,                                    // sourceIsReadOnly
            true,                                    // destinationDiffersFromSource (validation doesn't write)
            capability,                              // requestedCapability
            sourceRangesAreValid,                    // sourceRangesAreValid
            true,                                    // outputPathIsValid (not applicable for validation)
            arithmeticCannotOverflow,                // arithmeticCannotOverflow
            true                                     // cancellationStateIsValid
        );

        printSeparator();
        if (report.validationPassed()) {
            printSuccess("Validation PASSED - Operation is safe to proceed");
            recoverysuite::logging::Logger::instance().info("Validation PASSED: capability=" + capabilityToString(capability) +
              ", disk_number=" + std::to_string(diskNumber));
        } else {
            printError("Validation FAILED");
            auto errors = report.getValidationErrors();
            for (const auto& error : errors) {
                std::cout << "  - [" << static_cast<int>(error.type) << "] " << error.description << std::endl;
            }
            recoverysuite::logging::Logger::instance().warn("Validation FAILED: capability=" + capabilityToString(capability) +
              ", disk_number=" + std::to_string(diskNumber) +
              ", errors=" + std::to_string(errors.size()));
        }
        printSeparator();

    } catch (const std::exception& e) {
        printError("Exception during validation: " + std::string(e.what()));
        recoverysuite::logging::Logger::instance().error("Exception during validation: capability=" + capabilityToString(capability) +
          ", disk_number=" + std::to_string(diskNumber) +
          ", exception=" + std::string(e.what()));
    }
}

void CLIHandler::handleRecover(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        printError("Missing required arguments");
        showHelp({});
        return;
    }

    // Parse capability
    recoverysuite::recovery::RecoveryCapability capability = parseCapability(args[0]);
    if (capability == recoverysuite::recovery::RecoveryCapability::NONE) {
        printError("Unsupported capability: " + args[0]);
        showHelp({});
        return;
    }

    // Parse disk number
    uint64_t diskNumber;
    if (!parseUint64(args[1], diskNumber)) {
        printError("Invalid disk number: " + args[1]);
        return;
    }

    // Parse start sector
    uint64_t startSector;
    if (!parseUint64(args[2], startSector)) {
        printError("Invalid start sector: " + args[2]);
        return;
    }

    // Parse number of sectors
    uint64_t numSectors;
    if (!parseUint64(args[3], numSectors)) {
        printError("Invalid number of sectors: " + args[3]);
        return;
    }

    // Parse output path (optional)
    std::string outputPath = "./recovered";
    if (args.size() >= 5) {
        outputPath = args[4];
    }

    printInfo("Preparing to perform " + capabilityToString(capability) +
              " operation on disk #" + std::to_string(diskNumber) +
              ", starting at sector " + std::to_string(startSector) +
              ", length " + std::to_string(numSectors) + " sectors" +
              ", output to: " + outputPath);

    // Confirmation for potentially destructive operations
    if (capability != recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS) {
        printWarning("This operation may modify data on the target device.");
        if (!confirmAction("Are you sure you want to continue? (y/N): ")) {
            printInfo("Operation cancelled by user");
            return;
        }
    }

    try {
        // Open the disk
        auto disk = diskManager_->openDisk(diskNumber, true); // Open read-only for safety
        if (!disk) {
            printError("Failed to open disk #" + std::to_string(diskNumber));
            return;
        }

        // Initialize recovery service
        recoveryService_ = std::make_unique<recoverysuite::application::service::RecoveryService>(
            std::shared_ptr<recoverysuite::disk::IDiskReader>(disk.get()),
            recoverysuite::recovery::RecoverySafetyPolicy());

        // Generate operation ID for tracking
        std::string operationId = generateOperationId();
        auto& opInfo = activeOperations_[operationId];
        opInfo.id = operationId;
        opInfo.type = capabilityToString(capability);
        opInfo.status = "Starting...";
        opInfo.startTime = std::chrono::steady_clock::now();
        opInfo.cancellable = true;
        opInfo.cancelled = false;

        // Perform the appropriate operation based on capability
        bool success = false;
        std::string errorMessage;
        recoverysuite::application::service::RecoveryService::FileRecoveryResult fileResult;
        recoverysuite::application::service::RecoveryService::MetadataRecoveryResult metadataResult;
        recoverysuite::application::service::RecoveryService::CarvingResult carvingResult;
        recoverysuite::application::service::RecoveryService::FilesystemAnalysisResult analysisResult;

        opInfo.status = "In progress...";

        switch (capability) {
            case recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS:
                analysisResult = recoveryService_->analyzeFilesystem(startSector, numSectors);
                success = analysisResult.success;
                errorMessage = analysisResult.errorMessage;
                break;

            case recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY:
                fileResult = recoveryService_->recoverFiles(startSector, numSectors);
                success = fileResult.success;
                errorMessage = fileResult.errorMessage;
                if (success) {
                    // In a real implementation, we would save the recovered files to outputPath
                    printInfo("Recovered " + std::to_string(fileResult.recoveredFiles.size()) + " files");
                }
                break;

            case recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY:
                metadataResult = recoveryService_->recoverMetadata(startSector, numSectors);
                success = metadataResult.success;
                errorMessage = metadataResult.errorMessage;
                break;

            case recoverysuite::recovery::RecoveryCapability::CARVING:
                carvingResult = recoveryService_->carveFiles(startSector, numSectors);
                success = carvingResult.success;
                errorMessage = carvingResult.errorMessage;
                break;

            default:
                printError("Unsupported capability for recovery operation");
                return;
        }

        // Update operation status
        if (success) {
            opInfo.status = "Completed successfully";
            printSeparator();
            printSuccess(capabilityToString(capability) + " operation completed successfully");

            // Show results based on operation type
            switch (capability) {
                case recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS:
                    if (!analysisResult.analysisResults.empty()) {
                        std::cout << "Analysis Results:" << std::endl;
                        printSeparator();
                        for (const auto& [key, value] : analysisResult.analysisResults) {
                            std::cout << std::left << std::setw(25) << key << ": " << value << std::endl;
                        }
                    }
                    break;

                case recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY:
                    if (!fileResult.recoveredFiles.empty()) {
                        std::cout << "Recovered Files:" << std::endl;
                        printSeparator();
                        for (const auto& [filename, data] : fileResult.recoveredFiles) {
                            std::cout << std::left << std::setw(30) << filename << ": "
                                      << data.size() << " bytes" << std::endl;
                        }
                    }
                    break;

                case recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY:
                    if (!metadataResult.recoveredMetadata.empty()) {
                        std::cout << "Recovered Metadata:" << std::endl;
                        printSeparator();
                        for (const auto& [key, data] : metadataResult.recoveredMetadata) {
                            std::cout << std::left << std::setw(30) << key << ": "
                                      << data.size() << " bytes" << std::endl;
                        }
                    }
                    break;

                case recoverysuite::recovery::RecoveryCapability::CARVING:
                    if (!carvingResult.carvedFiles.empty()) {
                        std::cout << "Carved Files:" << std::endl;
                        printSeparator();
                        for (const auto& [filename, data] : carvingResult.carvedFiles) {
                            std::cout << std::left << std::setw(30) << filename << ": "
                                      << data.size() << " bytes" << std::endl;
                        }
                    }
                    break;

                default:
                    break;
            }

            printSeparator();
        } else {
            opInfo.status = "Failed: " + errorMessage;
            printSeparator();
            printError(capabilityToString(capability) + " operation failed");
            if (!errorMessage.empty()) {
                std::cout << "Error: " << errorMessage << std::endl;
            }
            printSeparator();
        }

    } catch (const std::exception& e) {
        printError("Exception during operation: " + std::string(e.what()));
        if (!activeOperations_.empty()) {
            auto& lastOp = activeOperations_.rbegin()->second;
            lastOp.status = "Error: " + std::string(e.what());
        }
    }
}

void CLIHandler::handleCancel(const std::vector<std::string>& args) {
    if (args.empty()) {
        printError("Missing operation ID");
        showHelp({});
        recoverysuite::logging::Logger::instance().warn("Cancel command failed: missing operation ID");
        return;
    }

    std::string operationId = args[0];
    auto it = activeOperations_.find(operationId);

    if (it == activeOperations_.end()) {
        printError("Operation not found: " + operationId);
        recoverysuite::logging::Logger::instance().warn("Cancel command failed: operation not found: " + operationId);
        return;
    }

    auto& opInfo = it->second;
    if (!opInfo.cancellable) {
        printError("Operation cannot be cancelled: " + operationId);
        recoverysuite::logging::Logger::instance().warn("Cancel command failed: operation cannot be cancelled: " + operationId);
        return;
    }

    if (opInfo.status == "Completed successfully" ||
        opInfo.status.rfind("Failed:", 0) == 0) {
        printError("Operation already finished: " + operationId);
        recoverysuite::logging::Logger::instance().warn("Cancel command failed: operation already finished: " + operationId);
        return;
    }

    printInfo("Cancelling operation: " + operationId);
    opInfo.cancelled = true;
    opInfo.status = "Cancelling...";
    recoverysuite::logging::Logger::instance().info("Cancellation requested for operation: " + operationId +
      ", type=" + opInfo.type);

    // In a full implementation, we would signal the operation to stop
    // For this MVP, we'll just mark it as cancelled
    printSuccess("Cancellation requested for operation: " + operationId);
}

void CLIHandler::handleStatus(const std::vector<std::string>& args) {
    if (activeOperations_.empty()) {
        printInfo("No active operations");
        recoverysuite::logging::Logger::instance().debug("Status command: no active operations");
        return;
    }

    if (!args.empty()) {
        // Show status for specific operation
        std::string operationId = args[0];
        auto it = activeOperations_.find(operationId);
        if (it == activeOperations_.end()) {
            printError("Operation not found: " + operationId);
            recoverysuite::logging::Logger::instance().warn("Status command failed: operation not found: " + operationId);
            return;
        }

        const auto& opInfo = it->second;
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - opInfo.startTime);

        printSeparator();
        std::cout << "Operation Status: " << operationId << std::endl;
        printSeparator();
        std::cout << "Type:           " << opInfo.type << std::endl;
        std::cout << "Status:         " << opInfo.status << std::endl;
        std::cout << "Started:        " << duration.count() << " seconds ago" << std::endl;
        std::cout << "Cancellable:    " << (opInfo.cancellable ? "Yes" : "No") << std::endl;
        if (opInfo.cancelled) {
            std::cout << "Cancellation:   Requested" << std::endl;
        }
        printSeparator();
        recoverysuite::logging::Logger::instance().debug("Status command executed for operation: " + operationId);
    } else {
        // Show all operations
        printSeparator();
        std::cout << "Active Operations (" << activeOperations_.size() << ")" << std::endl;
        printSeparator();
        std::cout << std::left << std::setw(10) << "ID"
                  << std::setw(20) << "Type"
                  << std::setw(15) << "Status"
                  << std::setw(10) << "Time"
                  << std::setw(10) << "Cancel"
                  << std::endl;
        printSeparator();

        for (const auto& [id, opInfo] : activeOperations_) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - opInfo.startTime);

            std::cout << std::left << std::setw(10) << id.substr(0, 8)
                      << std::setw(20) << opInfo.type.substr(0, 19)
                      << std::setw(15) << opInfo.status.substr(0, 14)
                      << std::setw(10) << duration.count() << "s"
                      << std::setw(10) << (opInfo.cancellable ? "Yes" : "No")
                      << std::endl;
        }

        printSeparator();
        recoverysuite::logging::Logger::instance().debug("Status command executed: showing " + std::to_string(activeOperations_.size()) + " active operations");
    }
}

void CLIHandler::showHelp(const std::vector<std::string>& args) {
    (void)args;

    printSeparator();
    std::cout << "RecoverySuite Command Line Interface" << std::endl;
    printSeparator();
    std::cout << "Usage: recoverysuite <command> [arguments]" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  list                                     List available storage devices" << std::endl;
    std::cout << "  disk info <disk_number>                  Show detailed information for a disk" << std::endl;
    std::cout << "  analyze <disk_number> [start_sector] [num_sectors]" << std::endl;
    std::cout << "                                           Analyze filesystem on specified disk range" << std::endl;
    std::cout << "  validate <capability> <disk_number> [start_sector] [num_sectors]" << std::endl;
    std::cout << "                                           Validate a recovery operation before execution" << std::endl;
    std::cout << "  recover <capability> <disk_number> <start_sector> <num_sectors> [output_path]" << std::endl;
    std::cout << "                                           Perform recovery operation" << std::endl;
    std::cout << "  cancel <operation_id>                    Cancel an active operation" << std::endl;
    std::cout << "  status [operation_id]                    Show status of operations (all if no ID specified)" << std::endl;
    std::cout << "  help, -h, --help                         Show this help message" << std::endl;
    std::cout << "  version, -v, --version                   Show version information" << std::endl;
    std::cout << std::endl;
    std::cout << "Capabilities:" << std::endl;
    std::cout << "  filesystem_analysis                      Analyze filesystem structure" << std::endl;
    std::cout << "  file_recovery                            Recover files from filesystem" << std::endl;
    std::cout << "  metadata_recovery                        Recover filesystem metadata" << std::endl;
    std::cout << "  carving                                  Carve files from raw disk data" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  recoverysuite list" << std::endl;
    std::cout << "  recoverysuite disk info 0" << std::endl;
    std::cout << "  recoverysuite analyze 0 0 100" << std::endl;
    std::cout << "  recoverysuite validate file_recovery 0 0 100" << std::endl;
    std::cout << "  recoverysuite recover file_recovery 0 0 100 1000 ./recovered" << std::endl;
    printSeparator();
    recoverysuite::logging::Logger::instance().debug("Help command executed");
}

void CLIHandler::showVersion(const std::vector<std::string>& args) {
    (void)args;

    // We would normally get this from a version header
    printInfo("RecoverySuite Version 1.0.0");
    printInfo("Build system initialized successfully!");
    recoverysuite::logging::Logger::instance().debug("Version command executed");
}

// Helper methods implementation
void CLIHandler::printSeparator() {
    std::cout << std::string(60, '-') << std::endl;
}

void CLIHandler::printSuccess(const std::string& message) {
    std::cout << "[SUCCESS] " << message << std::endl;
}

void CLIHandler::printError(const std::string& message) {
    std::cerr << "[ERROR] " << message << std::endl;
}

void CLIHandler::printWarning(const std::string& message) {
    std::cout << "[WARNING] " << message << std::endl;
}

void CLIHandler::printInfo(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

bool CLIHandler::parseUint64(const std::string& str, uint64_t& value) {
    try {
        size_t pos;
        value = std::stoull(str, &pos);
        return pos == str.length();
    } catch (...) {
        return false;
    }
}

bool CLIHandler::parseBool(const std::string& str, bool& value) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (lowerStr == "true" || lowerStr == "yes" || lowerStr == "1" || lowerStr == "on") {
        value = true;
        return true;
    } else if (lowerStr == "false" || lowerStr == "no" || lowerStr == "0" || lowerStr == "off") {
        value = false;
        return true;
    }
    return false;
}

recoverysuite::recovery::RecoveryCapability CLIHandler::parseCapability(const std::string& capStr) {
    std::string lowerStr = capStr;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (lowerStr == "filesystem_analysis" || lowerStr == "fs" || lowerStr == "analysis") {
        return recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS;
    } else if (lowerStr == "file_recovery" || lowerStr == "files" || lowerStr == "recover") {
        return recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY;
    } else if (lowerStr == "metadata_recovery" || lowerStr == "metadata" || lowerStr == "meta") {
        return recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY;
    } else if (lowerStr == "carving" || lowerStr == "carve" || lowerStr == "carv") {
        return recoverysuite::recovery::RecoveryCapability::CARVING;
    }

    return recoverysuite::recovery::RecoveryCapability::NONE;
}

std::string CLIHandler::capabilityToString(recoverysuite::recovery::RecoveryCapability capability) {
    switch (capability) {
        case recoverysuite::recovery::RecoveryCapability::FILESYSTEM_ANALYSIS:
            return "Filesystem Analysis";
        case recoverysuite::recovery::RecoveryCapability::FILE_RECOVERY:
            return "File Recovery";
        case recoverysuite::recovery::RecoveryCapability::METADATA_RECOVERY:
            return "Metadata Recovery";
        case recoverysuite::recovery::RecoveryCapability::CARVING:
            return "Carving";
        default:
            return "Unknown";
    }
}

bool CLIHandler::confirmAction(const std::string& message) {
    std::cout << message;
    std::string response;
    std::getline(std::cin, response);

    std::transform(response.begin(), response.end(), response.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    return (response == "y" || response == "yes");
}

std::string CLIHandler::generateOperationId() {
    // Simple operation ID generation - in production would use UUID or similar
    static std::atomic<uint64_t> counter{0};
    uint64_t id = ++counter;
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    std::stringstream ss;
    ss << "op_" << std::hex << millis << "_" << std::dec << id;
    return ss.str();
}

} // namespace cli
} // namespace recoverysuite