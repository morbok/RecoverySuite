#include "CLIHandler.h"
#include <charconv>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <limits>
#include <set>
#include "Application/Service/GUIRecoveryService.h"

namespace recoverysuite {
namespace cli {

CLIHandler::CLIHandler(std::function<bool()> cancellation)
    : cancellation_(std::move(cancellation)), diskManager_(std::make_unique<recoverysuite::disk::DiskManager>()),
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
    exitCode_ = 0;
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
    if (command == "image") {
        return handleImage(args);
    } else if (command == "list") {
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

    return exitCode_;
}

int CLIHandler::handleImage(const std::vector<std::string>& args) {
    const auto usageError = [&](const std::string& message) { printError(message); return 2; };
    if (args.size() < 2 || (args[0] != "info" && args[0] != "carve" && args[0] != "metadata" && args[0] != "analyze" && args[0] != "files" && args[0] != "tree"))
        return usageError("Use image info/analyze <path> or image carve/metadata/files/tree <path> --output <existing-directory>");
    const bool carve = args[0] == "carve" || args[0] == "metadata" || args[0] == "files" || args[0] == "tree";
    const bool analyze = args[0] == "analyze";
    uint64_t start = 0, count = 0, sectorSize = 512;
    bool countGiven = false, verify = true;
    std::string output;
    std::set<std::string> seen;
    for (size_t i = 2; i < args.size(); ++i) {
        const auto& option = args[i];
        if (!seen.insert(option).second) return usageError("Duplicate option: " + option);
        if (carve && option == "--no-verify") { verify = false; continue; }
        if (option != "--sector-size" && !((carve || analyze) && (option == "--start" || option == "--count")) && !(carve && option == "--output"))
            return usageError("Unknown option: " + option);
        if (++i == args.size()) return usageError("Missing value for " + option);
        if (option == "--output") { output = args[i]; continue; }
        uint64_t value = 0;
        if (!parseUint64(args[i], value)) return usageError("Invalid unsigned number for " + option);
        if (option == "--sector-size") sectorSize = value;
        else if (option == "--start") start = value;
        else { count = value; countGiven = true; }
    }
    if (sectorSize != 512 && sectorSize != 1024 && sectorSize != 2048 && sectorSize != 4096)
        return usageError("Sector size must be 512, 1024, 2048 or 4096");
    if (carve && output.empty()) return usageError("Choose an existing output directory with --output");
    try {
        application::service::GUIRecoveryService service(nullptr);
        const auto source = service.openImage(args[1], static_cast<uint32_t>(sectorSize));
        const uint64_t total = source.totalBytes / sectorSize;
        if (args[0] == "info") {
            std::cout << "Source: " << source.devicePath << "\nRead-only: yes\nSector size: " << sectorSize
                      << "\nSectors: " << total << "\nBytes: " << source.totalBytes << '\n';
            return 0;
        }
        if (start >= total) return usageError("Start sector is outside the image");
        if (!countGiven) count = total - start;
        if (!count || count > total - start) return usageError("Sector range is outside the image");
        if (analyze) {
            const auto result=service.analyzeFilesystem(source.devicePath,start,count);
            if (!result.success) {printError(result.errorMessage);return 1;}
            for (const auto& [key,value]:result.properties) std::cout<<key<<": "<<value<<'\n';
            return 0;
        }
        application::service::models::RecoveryOperation operation{};
        operation.operationType = args[0] == "metadata" ? "metadata_recovery" : (args[0] == "files" ? "file_recovery" : (args[0] == "tree" ? "file_tree_recovery" : "carving")); operation.targetDevicePath = source.devicePath;
        operation.startSector = start; operation.numSectors = count; operation.outputPath = output;
        operation.verifyAfterRecovery = verify;
        unsigned lastPercent = std::numeric_limits<unsigned>::max();
        const auto result = service.executeOperation(operation, [&](const auto& progress) {
            if (progress.totalBytes && progress.percentage != lastPercent) {
                lastPercent = progress.percentage;
                std::cerr << "Progress: " << progress.percentage << "%; scanned " << progress.processedBytes << " bytes\n";
            }
        }, cancellation_);
        for (const auto& item : result.recoveredItems)
            std::cout << "Saved: " << item.name << " (" << item.sizeBytes << " bytes)\n";
        std::cout << "Saved items: " << result.recoveredItemsCount << "\nSaved bytes: " << result.recoveredBytes
                  << "\nFailed items: " << result.failedItemsCount << "\nProcessed bytes: " << result.processedBytes << '\n';
        for (const auto& [key, value] : result.details) std::cout << key << ": " << value << '\n';
        if (result.cancelled) { std::cerr << "Cancelled; completed output files were kept\n"; return 130; }
        if (!result.success) { printError(result.errorMessage); return 1; }
        printSuccess("Image scan complete"); return 0;
    } catch (const std::exception& failure) { printError(failure.what()); return 1; }
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
            std::shared_ptr<recoverysuite::disk::IDiskReader>(std::move(disk)),
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
                                    (numSectors > 0 && numSectors <= diskInfo.totalSectors - startSector);
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
            printSuccess("Source range validation passed; export destination and recovery support were not validated");
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
    if (args.size() >= 4 && parseCapability(args[0]) == recovery::RecoveryCapability::FILESYSTEM_ANALYSIS) {
        handleAnalyze({args[1], args[2], args[3]});
        return;
    }
    printError("Physical-device recovery export is unavailable. Use image carve <path> --output <existing-directory>.");
}

void CLIHandler::handleCancel(const std::vector<std::string>&) {
    printError("Recovery runs in the foreground. Press Ctrl+C in its terminal to cancel it.");
}
void CLIHandler::handleStatus(const std::vector<std::string>&) {
    printError("There is no background operation registry. Foreground recovery reports its own progress and result.");
}

void CLIHandler::showHelp(const std::vector<std::string>&) {
    std::cout << R"(RecoverySuite — read-only image recovery
Usage: RecoverySuite_CLI <command> [arguments]

  image info <image> [--sector-size 512|1024|2048|4096]
  image analyze <image> [--start <sector>] [--count <sectors>] [--sector-size <bytes>]
  image carve <image> --output <existing-folder>
  image metadata <image> --output <existing-folder>
  image files <image> --output <existing-folder>
  image tree <image> --output <existing-folder>
      [--start <sector>] [--count <sectors>] [--sector-size <bytes>] [--no-verify]
  list
  disk info <disk-number>
  analyze <disk-number> [start-sector] [sector-count]
  validate <capability> <disk-number> [start-sector] [sector-count]
  help | version

Image carving saves JPEG/PDF/ZIP signature candidates, with readback verification by default.
Existing files are not replaced. Limits: 16 MiB per candidate, 10,000 attempted outputs.
Candidates may contain damaged data; names and fragmented files are not reconstructed.
Omitted --count scans from --start to the end of the image. Press Ctrl+C to cancel;
completed output files are kept. Exit: 0 complete, 1 failed/partial, 2 usage, 130 cancelled.
Files copies allocated FAT12/16/32 root files using verified long names or 8.3 aliases (16 MiB/file).
Tree adds recursive FAT subfolders (32 levels, 1023 directories, 64 MiB directory reads).
Empty folders, deleted entries and NTFS files are not exported. Files mode skips subdirectories.
Metadata saves actual FAT boot/FAT copies and FAT12/16 fixed root entries (16 MiB per region).
Metadata mode does not reconstruct NTFS MFT or FAT32 root chains. Safe output export currently requires POSIX. Physical-device export is unavailable.
)";
}

void CLIHandler::showVersion(const std::vector<std::string>& args) {
    (void)args;

    // We would normally get this from a version header
    printInfo("RecoverySuite 1.0.0 (development)");
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
    exitCode_ = 1;
    std::cerr << "[ERROR] " << message << std::endl;
}

void CLIHandler::printWarning(const std::string& message) {
    std::cout << "[WARNING] " << message << std::endl;
}

void CLIHandler::printInfo(const std::string& message) {
    std::cout << "[INFO] " << message << std::endl;
}

bool CLIHandler::parseUint64(const std::string& str, uint64_t& value) {
    uint64_t parsed{};
    const auto [end, error] = std::from_chars(str.data(), str.data() + str.size(), parsed);
    if (error != std::errc{} || end != str.data() + str.size()) return false;
    value = parsed;
    return true;
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