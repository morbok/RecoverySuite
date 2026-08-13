#ifndef RECOVERYSUITE_RECOVERY_CARVINGENGINE_H
#define RECOVERYSUITE_RECOVERY_CARVINGENGINE_H

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "RecoveryCapability.h"
#include "RecoveryOperationValidator.h"
#include "../Disk/IDiskReader.hpp"

namespace recoverysuite {
namespace recovery {

/**
 * CarvingEngine
 * Recovers files from storage media using signature-based carving (file carving)
 */
class CarvingEngine {
public:
    // Constructor
    CarvingEngine(
        const RecoveryCapabilityRegistry& capabilityRegistry,
        const RecoverySafetyPolicy& safetyPolicy = RecoverySafetyPolicy(),
        recoverysuite::disk::IDiskReader* diskReader = nullptr
    );

    // Destructor
    ~CarvingEngine() = default;

    // Perform file carving on the specified disk
    // Returns true if carving was successful
    bool carveFiles(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        std::map<std::string, std::vector<uint8_t>>& carvedFiles
    );

    // Carve files of a specific type (e.g., JPEG, PDF, ZIP)
    bool carveFileType(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        uint64_t startSector,
        uint64_t numSectors,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid,
        const std::string& fileType,
        std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& carvedFiles
    );

private:
    // The capability registry to check against
    const RecoveryCapabilityRegistry& capabilityRegistry_;

    // The safety policy to use for validation
    const RecoverySafetyPolicy& safetyPolicy_;

    // The disk reader to use for accessing disk data
    recoverysuite::disk::IDiskReader* diskReader_;

    // The operation validator
    RecoveryOperationValidator validator_;

    // Helper to check if a sector range is valid for the disk
    bool isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const;

    // Helper to read sectors from disk
    bool readSectors(
        uint64_t startSector,
        uint64_t numSectors,
        std::vector<uint8_t>& buffer
    ) const;

    // Helper to check if data matches a file signature
    bool matchesSignature(
        const std::vector<uint8_t>& data,
        size_t offset,
        const std::vector<uint8_t>& signature
    ) const;

    // Helper to get file signature information
    std::vector<std::pair<std::string, std::vector<uint8_t>>> getFileSignatures() const;

    // Helper to carve JPG/JPEG files
    bool carveJpegFiles(
        const std::vector<uint8_t>& data,
        std::map<std::string, std::vector<uint8_t>>& carvedFiles
    ) const;

    // Helper to carve PDF files
    bool carvePdfFiles(
        const std::vector<uint8_t>& data,
        std::map<std::string, std::vector<uint8_t>>& carvedFiles
    ) const;

    // Helper to carve ZIP files
    bool carveZipFiles(
        const std::vector<uint8_t>& data,
        std::map<std::string, std::vector<uint8_t>>& carvedFiles
    ) const;

    // Helper to extract file data from disk
    bool extractFileData(
        uint64_t startOffset,
        uint64_t fileSize,
        uint32_t sectorSize,
        std::vector<uint8_t>& data
    ) const;
};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_CARVINGENGINE_H