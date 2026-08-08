#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATANALYZER_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATANALYZER_H

#include <memory>
#include <string>
#include "../FilesystemAnalyzer.h"
#include "../FilesystemReader.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

// FAT Analyzer provides methods to analyze the health and consistency of a FAT filesystem.
class FATAnalyzer : public FilesystemAnalyzer {
public:
    // Constructor
    explicit FATAnalyzer(const FilesystemReader& reader);

    // Destructor
    ~FATAnalyzer() override = default;

    // Analyze the filesystem for consistency and health
    // @param reader: the filesystem reader to use for accessing data
    // @return true if the filesystem appears consistent and healthy, false otherwise
    bool analyze(const FilesystemReader& reader) const override;

    // Get a human-readable description of the last analysis
    virtual std::string getAnalysisDescription() const override;

    // Get detailed analysis results (implementation-defined)
    virtual std::string getAnalysisDetails() const override;

    // Check if the filesystem needs repair (e.g., chkdsk)
    virtual bool needsRepair() const override;

    // Get the estimated number of errors found
    virtual uint32_t getErrorCount() const override;

protected:
    // Reference to the filesystem reader (not owned)
    const FilesystemReader& reader_;
};

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATANALYZER_H