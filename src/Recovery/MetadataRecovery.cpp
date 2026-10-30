#include "MetadataRecovery.h"
#include "BootSectorDetection.h"
#include "source_bytes.h"
#include <utility>

namespace recoverysuite {
namespace recovery {

MetadataRecovery::MetadataRecovery(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader, std::function<bool()> cancelled)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      cancelled_(std::move(cancelled)),
      validator_(capabilityRegistry, safetyPolicy_) {
}

bool MetadataRecovery::recoverMetadata(
    bool sourceIsReadOnly, bool destinationDiffersFromSource, uint64_t startSector, uint64_t numSectors,
    bool sourceRangesAreValid, bool outputPathIsValid, bool arithmeticCannotOverflow,
    bool cancellationStateIsValid, std::map<std::string, std::vector<uint8_t>>& recoveredMetadata) {
    recoveredMetadata.clear();
    const auto report = validator_.validateOperation(sourceIsReadOnly, destinationDiffersFromSource,
        RecoveryCapability::METADATA_RECOVERY, sourceRangesAreValid, outputPathIsValid,
        arithmeticCannotOverflow, cancellationStateIsValid);
    if (!report.validationPassed() || !diskReader_) return false;
    std::vector<uint8_t> boot;
    if (!readSourceBytes(diskReader_, startSector, numSectors, 0, diskReader_->getSectorSize(), boot, cancelled_)) return false;
    const auto type = detectBootSectorType(boot);
    // MFT parsing is not implemented. Never return invented NTFS metadata.
    if (type != "FAT12" && type != "FAT16" && type != "FAT32") return false;
    // Microsoft FAT Specification, sections 3 and 6.6: all BPB locations are volume-relative.
    const auto sectorSize = sourceLe16(boot,11), reserved = sourceLe16(boot,14), fats = uint64_t{boot[16]};
    const auto rootEntries = sourceLe16(boot,17), fat16 = sourceLe16(boot,22);
    const auto sectors = sourceLe16(boot,19) ? sourceLe16(boot,19) : sourceLe32(boot,32);
    const auto fatSize = fat16 ? fat16 : sourceLe32(boot,36);
    if (sectorSize != diskReader_->getSectorSize() || fats > 2 ||
        sectors > diskReader_->getTotalSectors() - startSector ||
        (type == "FAT32" ? (rootEntries != 0 || fat16 != 0) : (rootEntries == 0 || fat16 == 0))) return false;
    std::map<std::string,std::vector<uint8_t>> result;
    result[type+"_BootSector"] = boot;
    for (uint64_t copy=0; copy<fats; ++copy) {
        std::vector<uint8_t> bytes;
        if (!readSourceBytes(diskReader_,startSector,numSectors,(reserved+copy*fatSize)*sectorSize,
                             fatSize*sectorSize,bytes,cancelled_)) return false;
        result[type+"_FAT"+std::to_string(copy)] = std::move(bytes);
    }
    if (rootEntries) {
        const uint64_t rootSectors = (rootEntries*32+sectorSize-1)/sectorSize;
        std::vector<uint8_t> root;
        if (!readSourceBytes(diskReader_,startSector,numSectors,(reserved+fats*fatSize)*sectorSize,
                             rootSectors*sectorSize,root,cancelled_)) return false;
        root.resize(static_cast<size_t>(rootEntries*32));
        result["RootDirectory"] = std::move(root);
    }
    recoveredMetadata = std::move(result);
    return true;
}
bool MetadataRecovery::recoverMetadataType(
    bool sourceIsReadOnly, bool destinationDiffersFromSource, uint64_t startSector, uint64_t numSectors,
    bool sourceRangesAreValid, bool outputPathIsValid, bool arithmeticCannotOverflow,
    bool cancellationStateIsValid, const std::string& metadataType, uint64_t offset, uint64_t size,
    std::vector<uint8_t>& recoveredData) {
    recoveredData.clear();
    const auto report = validator_.validateOperation(sourceIsReadOnly, destinationDiffersFromSource,
        RecoveryCapability::METADATA_RECOVERY, sourceRangesAreValid, outputPathIsValid,
        arithmeticCannotOverflow, cancellationStateIsValid);
    return report.validationPassed() && !metadataType.empty() &&
           readSourceBytes(diskReader_,startSector,numSectors,offset,size,recoveredData,cancelled_);
}
bool MetadataRecovery::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    if (!diskReader_ || !diskReader_->isOpen() || !numSectors) return false;
    const auto total = diskReader_->getTotalSectors();
    return startSector < total && numSectors <= total-startSector;
}
} // namespace recovery
} // namespace recoverysuite
