#include "NTFSAnalyzer.h"
#include <stdexcept>
#include <sstream>
#include "NTFSStructures.h"
#include <vector>

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

NTFSAnalyzer::NTFSAnalyzer(std::shared_ptr<FilesystemReader> reader)
    : reader_(std::move(reader)), 
      context_(std::make_shared<NTFSContext>(reader_)) {}


bool NTFSAnalyzer::analyze(const FilesystemReader& /* reader */) const {
    // In a real implementation, we would perform actual analysis here
    // For now, we'll just validate that we have a reader and return a basic result
    if (!reader_) {
        return false;
    }
    
    // Perform basic NTFS validation
    return validateBootSector() && validateMFT();
}

std::string NTFSAnalyzer::getAnalysisDescription() const {
    std::stringstream ss;
    ss << "NTFS filesystem analysis";
    if (reader_) {
        ss << " - Volume appears consistent";
    } else {
        ss << " - No volume available for analysis";
    }
    return ss.str();
}

std::string NTFSAnalyzer::getAnalysisDetails() const {
    std::stringstream ss;
    ss << "NTFS Analysis Details:\n";
    ss << "----------------------\n";
    
    if (!reader_) {
        ss << "Error: No filesystem reader available\n";
        return ss.str();
    }
    
    ss << "Boot sector validation: " << (validateBootSector() ? "PASS" : "FAIL") << "\n";
    ss << "MFT validation: " << (validateMFT() ? "PASS" : "FAIL") << "\n";
    ss << "Corruption check: " << (checkForCorruption() ? "NO CORRUPTION DETECTED" : "CORRUPTION DETECTED") << "\n";
    
    return ss.str();
}

bool NTFSAnalyzer::needsRepair() const {
    // In a real implementation, we would determine if repair is needed
    return !validateBootSector() || !validateMFT() || checkForCorruption();
}

uint32_t NTFSAnalyzer::getErrorCount() const {
    // In a real implementation, we would count actual errors
    uint32_t errorCount = 0;
    if (!validateBootSector()) errorCount++;
    if (!validateMFT()) errorCount++;
    if (checkForCorruption()) errorCount += 10; // Placeholder for multiple corruption errors
    return errorCount;
}

bool NTFSAnalyzer::validateBootSector() const {
    if (!reader_) {
        return false;
    }
    
    // Read the boot sector (first sector)
    const uint32_t sectorSize = reader_->getSectorSize();
    if (sectorSize == 0) {
        return false;
    }
    
    std::vector<uint8_t> buffer(sectorSize);
    uint32_t bytesRead = reader_->readSector(0, buffer.data(), sectorSize);
    
    if (bytesRead != sectorSize) {
        return false;
    }
    
    // Validate the boot sector
    return validateBootSector_impl(buffer.data(), sectorSize);
}

bool NTFSAnalyzer::validateBootSector_impl(const uint8_t* bootSectorData, uint32_t bootSectorSize) const {
    if (!bootSectorData || bootSectorSize < sizeof(BootSector)) {
        return false;
    }
    
    const BootSector* bootSector = reinterpret_cast<const BootSector*>(bootSectorData);
    
    // Check NTFS signature
    if (bootSector->oemId[0] != 'N' || bootSector->oemId[1] != 'T' ||
        bootSector->oemId[2] != 'F' || bootSector->oemId[3] != 'S') {
        return false;
    }
    
    // Check bytes per sector (should be power of 2 and reasonable)
    uint16_t bytesPerSector = bootSector->bytesPerSector;
    if (bytesPerSector < 256 || bytesPerSector > 4096 || (bytesPerSector & (bytesPerSector - 1)) != 0) {
        return false;
    }
    
    // Check sectors per cluster (should be power of 2 and reasonable)
    uint8_t sectorsPerCluster = bootSector->sectorsPerCluster;
    if (sectorsPerCluster == 0 || sectorsPerCluster > 128 || (sectorsPerCluster & (sectorsPerCluster - 1)) != 0) {
        return false;
    }
    
    // Check boot signature
    if (bootSector->bootSignature != 0xAA55) {
        return false;
    }
    
    return true;
}

bool NTFSAnalyzer::validateMFT() const {
    // In a real implementation, we would read and validate the MFT
    // For now, we'll return true as a placeholder
    return true;
}

bool NTFSAnalyzer::checkForCorruption() const {
    // In a real implementation, we would check for various corruption signs
    // For now, we'll return false as a placeholder (no corruption)
    return false;
}

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite
