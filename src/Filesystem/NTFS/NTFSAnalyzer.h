#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_ANALYZER_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_ANALYZER_H

#include <memory>
#include <string>
#include "../FilesystemAnalyzer.h"
#include "NTFSContext.h"

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// NTFS Analyzer implementation
class NTFSAnalyzer : public FilesystemAnalyzer {
public:
    explicit NTFSAnalyzer(std::shared_ptr<FilesystemReader> reader);
    ~NTFSAnalyzer() override = default;

    // FilesystemAnalyzer interface
    bool analyze(const FilesystemReader& reader) const override;
    std::string getAnalysisDescription() const override;
    std::string getAnalysisDetails() const override;
    // Repair state/error count are unknown; these getters throw std::logic_error.
    bool needsRepair() const override;
    uint32_t getErrorCount() const override;

private:
    std::shared_ptr<FilesystemReader> reader_;
    std::shared_ptr<NTFSContext> context_;

    // Internal analysis methods
    bool validateBootSector() const;
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_ANALYZER_H
