#ifndef RECOVERYSUITE_CLI_CLIHANDLER_H
#define RECOVERYSUITE_CLI_CLIHANDLER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <atomic>

#include "../Application/Service/RecoveryService.h"
#include "../Disk/IDiskReader.hpp"
#include "../Disk/DiskManager.h"
#include "../Disk/IDiskReader.hpp"
#include "../Recovery/RecoveryCapability.h"
#include "../Recovery/RecoverySafetyPolicy.h"
#include "../Recovery/RecoveryOperationValidator.h"

namespace recoverysuite {
namespace cli {

class CLIHandler {
public:
    CLIHandler();
    ~CLIHandler();

    // Main CLI loop
    int run(int argc, char* argv[]);

    // Command handlers
    void handleList(const std::vector<std::string>& args);
    void handleDiskInfo(const std::vector<std::string>& args);
    void handleAnalyze(const std::vector<std::string>& args);
    void handleValidate(const std::vector<std::string>& args);
    void handleRecover(const std::vector<std::string>& args);
    void handleCancel(const std::vector<std::string>& args);
    void handleStatus(const std::vector<std::string>& args);
    void showHelp(const std::vector<std::string>& args);
    void showVersion(const std::vector<std::string>& args);

private:
    // Helper methods
    void printSeparator();
    void printSuccess(const std::string& message);
    void printError(const std::string& message);
    void printWarning(const std::string& message);
    void printInfo(const std::string& message);

    bool parseUint64(const std::string& str, uint64_t& value);
    bool parseBool(const std::string& str, bool& value);

    recoverysuite::recovery::RecoveryCapability parseCapability(const std::string& capStr);
    std::string capabilityToString(recoverysuite::recovery::RecoveryCapability capability);

    bool confirmAction(const std::string& message);

    // Components
    std::unique_ptr<recoverysuite::disk::DiskManager> diskManager_;
    std::unique_ptr<recoverysuite::application::service::RecoveryService> recoveryService_;

    // Operation tracking (simplified for MVP)
    struct OperationInfo {
        std::string id;
        std::string type;
        std::string status;
        std::chrono::time_point<std::chrono::steady_clock> startTime;
        bool cancellable;
        std::atomic<bool> cancelled;
    };

    std::map<std::string, OperationInfo> activeOperations_;
    std::string generateOperationId();

    // Disable copy/move
    CLIHandler(const CLIHandler&) = delete;
    CLIHandler& operator=(const CLIHandler&) = delete;
};

} // namespace cli
} // namespace recoverysuite

#endif // RECOVERYSUITE_CLI_CLIHANDLER_H