#ifndef RECOVERYSUITE_FILESYSTEM_FILESYSTEM_ANALYZER_H
#define RECOVERYSUITE_FILESYSTEM_FILESYSTEM_ANALYZER_H

#include <memory>
#include <string>
#include "FilesystemExceptions.h"
#include "FilesystemTypes.h"

namespace recoverysuite {
namespace filesystem {

// Forward declaration
class FilesystemReader;

// FilesystemAnalyzer provides methods to analyze the health and consistency of a filesystem.
// It works with a FilesystemReader to access the raw data.
class FilesystemAnalyzer {
public:
    virtual ~FilesystemAnalyzer() = default;

    // Analyze the filesystem for consistency and health
    // @param reader: the filesystem reader to use for accessing data
    // @return true if the filesystem appears consistent and healthy, false otherwise
    virtual bool analyze(const FilesystemReader& reader) const = 0;

    // Get a human-readable description of the last analysis
    virtual std::string getAnalysisDescription() const = 0;

    // Get detailed analysis results (implementation-defined)
    virtual std::string getAnalysisDetails() const = 0;

    // Check if the filesystem needs repair (e.g., chkdsk)
    virtual bool needsRepair() const = 0;

    // Get the estimated number of errors found
    virtual uint32_t getErrorCount() const = 0;
};

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FILESYSTEM_ANALYZER_H
