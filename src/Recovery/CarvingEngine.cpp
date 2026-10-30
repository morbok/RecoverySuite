#include "CarvingEngine.h"
#include "signature_carver.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace recoverysuite {
namespace recovery {

CarvingEngine::CarvingEngine(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      validator_(capabilityRegistry, safetyPolicy_) {
}

bool CarvingEngine::carveFiles(
    bool sourceIsReadOnly, bool destinationDiffersFromSource, uint64_t startSector, uint64_t numSectors,
    bool sourceRangesAreValid, bool outputPathIsValid, bool arithmeticCannotOverflow,
    bool cancellationStateIsValid, std::map<std::string,std::vector<uint8_t>>& carvedFiles) {
    carvedFiles.clear();
    const auto report = validator_.validateOperation(sourceIsReadOnly,destinationDiffersFromSource,
        RecoveryCapability::CARVING,sourceRangesAreValid,outputPathIsValid,arithmeticCannotOverflow,cancellationStateIsValid);
    if (!report.validationPassed() || !diskReader_) return false;
    uint64_t bytes = 0;
    try {
        const auto summary = carveSignatures(*diskReader_,startSector,numSectors,[&](const auto& name,const auto& data) {
            if (carvedFiles.size() >= 10000 || data.size() > 64*1024*1024-bytes)
                throw std::runtime_error("Buffered carving result limit reached");
            bytes += data.size();carvedFiles.emplace(name,data);
        });
        return !carvedFiles.empty() && !summary.skippedCandidates;
    } catch (const std::exception&) { carvedFiles.clear();return false; }
}
bool CarvingEngine::carveFileType(
    bool sourceIsReadOnly, bool destinationDiffersFromSource, uint64_t startSector, uint64_t numSectors,
    bool sourceRangesAreValid, bool outputPathIsValid, bool arithmeticCannotOverflow,
    bool cancellationStateIsValid, const std::string& fileType,
    std::vector<std::pair<uint64_t,std::vector<uint8_t>>>& carvedFiles) {
    carvedFiles.clear();
    const auto report = validator_.validateOperation(sourceIsReadOnly,destinationDiffersFromSource,
        RecoveryCapability::CARVING,sourceRangesAreValid,outputPathIsValid,arithmeticCannotOverflow,cancellationStateIsValid);
    if (!report.validationPassed() || !diskReader_) return false;
    std::string type = fileType;
    std::transform(type.begin(),type.end(),type.begin(),[](unsigned char value) {return static_cast<char>(std::tolower(value));});
    if (type == "jpeg") type = "jpg";
    if (type != "jpg" && type != "pdf" && type != "zip") return false;
    uint64_t bytes = 0;
    try {
        const auto summary = carveSignatures(*diskReader_,startSector,numSectors,{}, {}, {},
            [&](uint64_t offset,const std::string& name,const auto& data) {
                if (name.substr(name.size()-3) != type) return;
                if (carvedFiles.size() >= 10000 || data.size() > 64*1024*1024-bytes)
                    throw std::runtime_error("Buffered carving result limit reached");
                bytes += data.size();
                carvedFiles.emplace_back(startSector*diskReader_->getSectorSize()+offset,data);
            });
        return !carvedFiles.empty() && !summary.skippedCandidates;
    } catch (const std::exception&) { carvedFiles.clear();return false; }
}
} // namespace recovery
} // namespace recoverysuite
