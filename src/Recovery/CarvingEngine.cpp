#include "CarvingEngine.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>
#include <cstddef> // for std::byte
#include <limits>

namespace recoverysuite {
namespace recovery {

CarvingEngine::CarvingEngine(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy,
    recoverysuite::disk::IDiskReader* diskReader)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy),
      diskReader_(diskReader),
      validator_(capabilityRegistry, safetyPolicy) {
}

bool CarvingEngine::carveFiles(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    uint64_t startSector,
    uint64_t numSectors,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid,
    std::map<std::string, std::vector<uint8_t>>& carvedFiles) {

    // Clear any previous results
    carvedFiles.clear();

    // Validate the operation using our validation framework
    auto report = validator_.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            RecoveryCapability::CARVING,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid);

    if (!report.validationPassed()) {
        return false;
    }

    // Check if we have a disk reader
    if (diskReader_ == nullptr) {
        return false;
    }

    // Validate sector range
    if (!isSectorRangeValid(startSector, numSectors)) {
        return false;
    }

    // Calculate total bytes to process
    // Get actual sector size from disk reader
    uint32_t sectorSize = diskReader_->getSectorSize();
    if (sectorSize == 0) {
        return false; // Invalid sector size
    }

    // Check for overflow in numSectors * sectorSize
    // If numSectors > UINT64_MAX / sectorSize, overflow would occur
    if (sectorSize > 0 && numSectors > std::numeric_limits<uint64_t>::max() / sectorSize) {
        return false; // Overflow would occur
    }

    uint64_t totalBytes = numSectors * sectorSize;

    // For memory efficiency, we'll process the disk by scanning for signatures in chunks
    // When a signature is found, we'll read the appropriate data to extract the file
    const size_t scanChunkSize = 64 * 1024; // 64KB chunks for scanning
    bool success = false;

    // Process the disk in chunks for signature scanning
    for (uint64_t byteOffset = 0; byteOffset < totalBytes; byteOffset += scanChunkSize) {
        // Check for cancellation
        if (!cancellationStateIsValid) {
            return false;
        }

        // Calculate how many bytes to read in this scan chunk
        uint64_t bytesLeft = totalBytes - byteOffset;
        size_t currentScanSize = std::min(scanChunkSize, static_cast<size_t>(bytesLeft));

        if (currentScanSize == 0) {
            break;
        }

        // Calculate which sector this scan chunk starts at
        uint64_t scanStartSector = startSector + (byteOffset / sectorSize);
        uint64_t sectorsInScan = (currentScanSize + sectorSize - 1) / sectorSize; // Round up

        // Make sure we don't read beyond our allocated range
        uint64_t maxSectors = startSector + numSectors - scanStartSector;
        if (sectorsInScan > maxSectors) {
            sectorsInScan = maxSectors;
        }

        if (sectorsInScan == 0) {
            break;
        }

        // Read the scan chunk from disk
        std::vector<uint8_t> scanData;
        scanData.resize(sectorsInScan * sectorSize, 0);

        // Create vector of std::byte for the disk reader interface
        std::vector<std::byte> byteBuffer(sectorsInScan * sectorSize);

        // Read sectors from disk
        if (!diskReader_->readSectors(scanStartSector, sectorsInScan, byteBuffer)) {
            return false;
        }

        // Copy data from std::byte vector to uint8_t vector
        std::copy(byteBuffer.begin(), byteBuffer.end(),
                  reinterpret_cast<std::byte*>(scanData.data()));

        // Trim to actual scan size if we read extra sectors due to rounding
        if (scanData.size() > currentScanSize) {
            scanData.resize(currentScanSize);
        }

        // Scan for JPEG signatures and extract files
        size_t jpegSearchPos = 0;
        while (jpegSearchPos < scanData.size()) {
            // Check for cancellation
            if (!cancellationStateIsValid) {
                return false;
            }

            // Find the start of a JPEG image
            const std::vector<uint8_t> jpegStart = {0xFF, 0xD8, 0xFF};
            auto startIt = std::search(scanData.begin() + jpegSearchPos, scanData.end(), jpegStart.begin(), jpegStart.end());
            if (startIt == scanData.end()) {
                break; // No more JPEG signatures found
            }

            size_t jpegStartOffset = std::distance(scanData.begin(), startIt);
            size_t absoluteJpegStartOffset = byteOffset + jpegStartOffset;

            // Find the end of the JPEG image
            const std::vector<uint8_t> jpegEnd = {0xFF, 0xD9};
            auto endIt = std::search(startIt + jpegStart.size(), scanData.end(), jpegEnd.begin(), jpegEnd.end());

            size_t jpegEndOffset;
            if (endIt != scanData.end()) {
                jpegEndOffset = std::distance(scanData.begin(), endIt) + jpegEnd.size(); // Include the end marker
            } else {
                // If we can't find the end in this chunk, we need to read more data
                // For simplicity in this implementation, we'll skip this JPEG
                // A more complete implementation would read additional chunks to find the end
                jpegSearchPos = jpegStartOffset + 1; // Move past this start to look for another
                continue;
            }

            size_t absoluteJpegEndOffset = byteOffset + jpegEndOffset;
            uint64_t jpegFileSize = absoluteJpegEndOffset - absoluteJpegStartOffset;

            // Extract the JPEG data by reading from disk
            std::vector<uint8_t> jpegData;
            if (extractFileData(absoluteJpegStartOffset, jpegFileSize, sectorSize, jpegData)) {
                // Add to our carved files
                std::string fileName = "carved_" + std::to_string(carvedFiles.size()) + ".jpg";
                carvedFiles[fileName] = jpegData;
                success = true;

                // Continue searching after this JPEG
                jpegSearchPos = jpegEndOffset;
            } else {
                // If we couldn't extract the data, move past this start to look for another
                jpegSearchPos = jpegStartOffset + 1;
            }
        }

        // Scan for PDF signatures and extract files
        size_t pdfSearchPos = 0;
        while (pdfSearchPos < scanData.size()) {
            // Check for cancellation
            if (!cancellationStateIsValid) {
                return false;
            }

            // Find the start of a PDF file
            const std::vector<uint8_t> pdfStart = {0x25, 0x50, 0x44, 0x46};
            auto startIt = std::search(scanData.begin() + pdfSearchPos, scanData.end(), pdfStart.begin(), pdfStart.end());
            if (startIt == scanData.end()) {
                break; // No more PDF signatures found
            }

            size_t pdfStartOffset = std::distance(scanData.begin(), startIt);
            size_t absolutePdfStartOffset = byteOffset + pdfStartOffset;

            // For simplicity, we'll assume a fixed size or look for %%EOF
            // In a real implementation, we would parse the PDF structure to find the actual end
            const std::vector<uint8_t> pdfEnd = {0x25, 0x25, 0x45, 0x46}; // %%EOF
            auto endIt = std::search(startIt + pdfStart.size(), scanData.end(), pdfEnd.begin(), pdfEnd.end());

            size_t pdfEndOffset;
            if (endIt != scanData.end()) {
                pdfEndOffset = std::distance(scanData.begin(), endIt) + pdfEnd.size(); // Include the EOF marker
            } else {
                // If we can't find the end, assume a reasonable size
                pdfEndOffset = std::min(pdfStartOffset + 1024 * 100, scanData.size()); // Up to 100KB
            }

            size_t absolutePdfEndOffset = byteOffset + pdfEndOffset;
            uint64_t pdfFileSize = absolutePdfEndOffset - absolutePdfStartOffset;

            // Extract the PDF data by reading from disk
            std::vector<uint8_t> pdfData;
            if (extractFileData(absolutePdfStartOffset, pdfFileSize, sectorSize, pdfData)) {
                // Add to our carved files
                std::string fileName = "carved_" + std::to_string(carvedFiles.size()) + ".pdf";
                carvedFiles[fileName] = pdfData;
                success = true;

                // Continue searching after this PDF
                pdfSearchPos = pdfEndOffset;
            } else {
                // If we couldn't extract the data, move past this start to look for another
                pdfSearchPos = pdfStartOffset + 1;
            }
        }

        // Scan for ZIP signatures and extract files
        size_t zipSearchPos = 0;
        while (zipSearchPos < scanData.size()) {
            // Check for cancellation
            if (!cancellationStateIsValid) {
                return false;
            }

            // Find the start of a ZIP local file header
            const std::vector<uint8_t> zipLocalHeader = {0x50, 0x4B, 0x03, 0x04};
            auto startIt = std::search(scanData.begin() + zipSearchPos, scanData.end(), zipLocalHeader.begin(), zipLocalHeader.end());
            if (startIt == scanData.end()) {
                break; // No more ZIP signatures found
            }

            size_t zipStartOffset = std::distance(scanData.begin(), startIt);
            size_t absoluteZipStartOffset = byteOffset + zipStartOffset;

            // For simplicity, we'll extract a fixed size chunk
            // In a real implementation, we would parse the ZIP structure to find the actual file size
            size_t chunkSize = std::min(static_cast<size_t>(64 * 1024), scanData.size() - zipStartOffset); // Up to 64KB
            size_t zipEndOffset = zipStartOffset + chunkSize;

            size_t absoluteZipEndOffset = byteOffset + zipEndOffset;
            uint64_t zipFileSize = absoluteZipEndOffset - absoluteZipStartOffset;

            // Extract the ZIP data by reading from disk
            std::vector<uint8_t> zipData;
            if (extractFileData(absoluteZipStartOffset, zipFileSize, sectorSize, zipData)) {
                // Add to our carved files
                std::string fileName = "carved_" + std::to_string(carvedFiles.size()) + ".zip";
                carvedFiles[fileName] = zipData;
                success = true;

                // Continue searching after this ZIP chunk
                zipSearchPos = zipEndOffset;
            } else {
                // If we couldn't extract the data, move past this start to look for another
                zipSearchPos = zipStartOffset + 1;
            }
        }
    }

    return success;
}

// Helper function to extract file data from disk
bool CarvingEngine::extractFileData(uint64_t startOffset, uint64_t fileSize, uint32_t sectorSize, std::vector<uint8_t>& data) const {
    if (fileSize == 0) {
        return false;
    }

    // Check for overflow in startOffset + fileSize
    if (startOffset > std::numeric_limits<uint64_t>::max() - fileSize) {
        return false; // Overflow would occur
    }

    uint64_t endOffset = startOffset + fileSize;

    // Calculate start sector and sector count
    uint64_t startSector = startOffset / sectorSize;
    uint64_t endSector = (endOffset + sectorSize - 1) / sectorSize; // Round up
    uint64_t numSectors = endSector - startSector;

    // Read the data from disk
    std::vector<uint8_t> rawData;
    rawData.resize(numSectors * sectorSize, 0);

    // Create vector of std::byte for the disk reader interface
    std::vector<std::byte> byteBuffer(numSectors * sectorSize);

    // Read sectors from disk
    if (!diskReader_->readSectors(startSector, numSectors, byteBuffer)) {
        return false;
    }

    // Copy data from std::byte vector to uint8_t vector
    std::copy(byteBuffer.begin(), byteBuffer.end(),
              reinterpret_cast<std::byte*>(rawData.data()));

    // Extract the exact file data (trim any extra bytes from sector rounding)
    if (startOffset % sectorSize != 0) {
        // We have leading bytes to skip
        size_t leadingBytes = startOffset % sectorSize;
        if (leadingBytes >= rawData.size()) {
            return false; // Invalid offset
        }
        rawData.erase(rawData.begin(), rawData.begin() + leadingBytes);
    }

    // Trim to exact file size
    if (rawData.size() > fileSize) {
        rawData.resize(fileSize);
    }

    data = std::move(rawData);
    return true;
}

// Helper to check if a sector range is valid for the disk
bool CarvingEngine::isSectorRangeValid(uint64_t startSector, uint64_t numSectors) const {
    // Validate that we have a disk reader
    if (diskReader_ == nullptr) {
        return false;
    }

    // Check for zero-length range
    if (numSectors == 0) {
        return false;
    }

    // Get total sectors from disk
    uint64_t totalSectors = diskReader_->getTotalSectors();

    // Check for overflow in startSector + numSectors
    // If startSector + numSectors < startSector, overflow occurred
    if (startSector > std::numeric_limits<uint64_t>::max() - numSectors) {
        return false; // Overflow would occur
    }

    uint64_t endSector = startSector + numSectors;

    // Check if start is beyond the disk
    if (startSector >= totalSectors) {
        return false;
    }

    // Check if range extends beyond the disk
    if (endSector > totalSectors) {
        return false;
    }

    return true;
}

bool CarvingEngine::carveFileType(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    uint64_t startSector,
    uint64_t numSectors,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid,
    const std::string& fileType,
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>>& carvedFiles) {

    // Clear any previous results
    carvedFiles.clear();

    // Validate the operation using our validation framework
    auto report = validator_.validateOperation(
            sourceIsReadOnly,
            destinationDiffersFromSource,
            RecoveryCapability::CARVING,
            sourceRangesAreValid,
            outputPathIsValid,
            arithmeticCannotOverflow,
            cancellationStateIsValid);

    if (!report.validationPassed()) {
        return false;
    }

    // Check if we have a disk reader
    if (diskReader_ == nullptr) {
        return false;
    }

    // Validate sector range
    if (!isSectorRangeValid(startSector, numSectors)) {
        return false;
    }

    // Calculate total bytes to process
    // Get actual sector size from disk reader
    uint32_t sectorSize = diskReader_->getSectorSize();
    if (sectorSize == 0) {
        return false; // Invalid sector size
    }

    // Check for overflow in numSectors * sectorSize
    // If numSectors > UINT64_MAX / sectorSize, overflow would occur
    if (sectorSize > 0 && numSectors > std::numeric_limits<uint64_t>::max() / sectorSize) {
        return false; // Overflow would occur
    }

    uint64_t totalBytes = numSectors * sectorSize;

    // For memory efficiency, we'll process the disk by scanning for signatures in chunks
    // When a signature is found, we'll read the appropriate data to extract the file
    const size_t scanChunkSize = 64 * 1024; // 64KB chunks for scanning
    bool success = false;

    // Process the disk in chunks for signature scanning
    for (uint64_t byteOffset = 0; byteOffset < totalBytes; byteOffset += scanChunkSize) {
        // Check for cancellation
        if (!cancellationStateIsValid) {
            return false;
        }

        // Calculate how many bytes to read in this scan chunk
        uint64_t bytesLeft = totalBytes - byteOffset;
        size_t currentScanSize = std::min(scanChunkSize, static_cast<size_t>(bytesLeft));

        if (currentScanSize == 0) {
            break;
        }

        // Calculate which sector this scan chunk starts at
        uint64_t scanStartSector = startSector + (byteOffset / sectorSize);
        uint64_t sectorsInScan = (currentScanSize + sectorSize - 1) / sectorSize; // Round up

        // Make sure we don't read beyond our allocated range
        uint64_t maxSectors = startSector + numSectors - scanStartSector;
        if (sectorsInScan > maxSectors) {
            sectorsInScan = maxSectors;
        }

        if (sectorsInScan == 0) {
            break;
        }

        // Read the scan chunk from disk
        std::vector<uint8_t> scanData;
        scanData.resize(sectorsInScan * sectorSize, 0);

        // Create vector of std::byte for the disk reader interface
        std::vector<std::byte> byteBuffer(sectorsInScan * sectorSize);

        // Read sectors from disk
        if (!diskReader_->readSectors(scanStartSector, sectorsInScan, byteBuffer)) {
            return false;
        }

        // Copy data from std::byte vector to uint8_t vector
        std::copy(byteBuffer.begin(), byteBuffer.end(),
                  reinterpret_cast<std::byte*>(scanData.data()));

        // Trim to actual scan size if we read extra sectors due to rounding
        if (scanData.size() > currentScanSize) {
            scanData.resize(currentScanSize);
        }

        bool chunkSuccess = false;

        if (fileType == "JPEG" || fileType == "JPG") {
            // Scan for JPEG signatures and extract files
            size_t jpegSearchPos = 0;
            while (jpegSearchPos < scanData.size()) {
                // Check for cancellation
                if (!cancellationStateIsValid) {
                    return false;
                }

                // Find the start of a JPEG image
                const std::vector<uint8_t> jpegStart = {0xFF, 0xD8, 0xFF};
                auto startIt = std::search(scanData.begin() + jpegSearchPos, scanData.end(), jpegStart.begin(), jpegStart.end());
                if (startIt == scanData.end()) {
                    break; // No more JPEG signatures found
                }

                size_t jpegStartOffset = std::distance(scanData.begin(), startIt);
                size_t absoluteJpegStartOffset = byteOffset + jpegStartOffset;

                // Find the end of the JPEG image
                const std::vector<uint8_t> jpegEnd = {0xFF, 0xD9};
                auto endIt = std::search(startIt + jpegStart.size(), scanData.end(), jpegEnd.begin(), jpegEnd.end());

                size_t jpegEndOffset;
                if (endIt != scanData.end()) {
                    jpegEndOffset = std::distance(scanData.begin(), endIt) + jpegEnd.size(); // Include the end marker
                } else {
                    // If we can't find the end in this chunk, we need to read more data
                    // For simplicity in this implementation, we'll skip this JPEG
                    jpegSearchPos = jpegStartOffset + 1; // Move past this start to look for another
                    continue;
                }

                size_t absoluteJpegEndOffset = byteOffset + jpegEndOffset;
                uint64_t jpegFileSize = absoluteJpegEndOffset - absoluteJpegStartOffset;

                // Extract the JPEG data by reading from disk
                std::vector<uint8_t> jpegData;
                if (extractFileData(absoluteJpegStartOffset, jpegFileSize, sectorSize, jpegData)) {
                    // For simplicity, we'll use sequential offsets
                    // In a more accurate implementation, we would track the actual file offset
                    static uint64_t offset = 0;
                    carvedFiles.emplace_back(offset + absoluteJpegStartOffset, jpegData);
                    offset += jpegData.size();
                    chunkSuccess = true;

                    // Continue searching after this JPEG
                    jpegSearchPos = jpegEndOffset;
                } else {
                    // If we couldn't extract the data, move past this start to look for another
                    jpegSearchPos = jpegStartOffset + 1;
                }
            }
        } else if (fileType == "PDF") {
            // Scan for PDF signatures and extract files
            size_t pdfSearchPos = 0;
            while (pdfSearchPos < scanData.size()) {
                // Check for cancellation
                if (!cancellationStateIsValid) {
                    return false;
                }

                // Find the start of a PDF file
                const std::vector<uint8_t> pdfStart = {0x25, 0x50, 0x44, 0x46};
                auto startIt = std::search(scanData.begin() + pdfSearchPos, scanData.end(), pdfStart.begin(), pdfStart.end());
                if (startIt == scanData.end()) {
                    break; // No more PDF signatures found
                }

                size_t pdfStartOffset = std::distance(scanData.begin(), startIt);
                size_t absolutePdfStartOffset = byteOffset + pdfStartOffset;

                // For simplicity, we'll assume a fixed size or look for %%EOF
                // In a real implementation, we would parse the PDF structure to find the actual end
                const std::vector<uint8_t> pdfEnd = {0x25, 0x25, 0x45, 0x46}; // %%EOF
                auto endIt = std::search(startIt + pdfStart.size(), scanData.end(), pdfEnd.begin(), pdfEnd.end());

                size_t pdfEndOffset;
                if (endIt != scanData.end()) {
                    pdfEndOffset = std::distance(scanData.begin(), endIt) + pdfEnd.size(); // Include the EOF marker
                } else {
                    // If we can't find the end, assume a reasonable size
                    pdfEndOffset = std::min(pdfStartOffset + 1024 * 100, scanData.size()); // Up to 100KB
                }

                size_t absolutePdfEndOffset = byteOffset + pdfEndOffset;
                uint64_t pdfFileSize = absolutePdfEndOffset - absolutePdfStartOffset;

                // Extract the PDF data by reading from disk
                std::vector<uint8_t> pdfData;
                if (extractFileData(absolutePdfStartOffset, pdfFileSize, sectorSize, pdfData)) {
                    // For simplicity, we'll use sequential offsets
                    static uint64_t offset = 0;
                    carvedFiles.emplace_back(offset + absolutePdfStartOffset, pdfData);
                    offset += pdfData.size();
                    chunkSuccess = true;

                    // Continue searching after this PDF
                    pdfSearchPos = pdfEndOffset;
                } else {
                    // If we couldn't extract the data, move past this start to look for another
                    pdfSearchPos = pdfStartOffset + 1;
                }
            }
        } else if (fileType == "ZIP") {
            // Scan for ZIP signatures and extract files
            size_t zipSearchPos = 0;
            while (zipSearchPos < scanData.size()) {
                // Check for cancellation
                if (!cancellationStateIsValid) {
                    return false;
                }

                // Find the start of a ZIP local file header
                const std::vector<uint8_t> zipLocalHeader = {0x50, 0x4B, 0x03, 0x04};
                auto startIt = std::search(scanData.begin() + zipSearchPos, scanData.end(), zipLocalHeader.begin(), zipLocalHeader.end());
                if (startIt == scanData.end()) {
                    break; // No more ZIP signatures found
                }

                size_t zipStartOffset = std::distance(scanData.begin(), startIt);
                size_t absoluteZipStartOffset = byteOffset + zipStartOffset;

                // For simplicity, we'll extract a fixed size chunk
                // In a real implementation, we would parse the ZIP structure to find the actual file size
                size_t chunkSize = std::min(static_cast<size_t>(64 * 1024), scanData.size() - zipStartOffset); // Up to 64KB
                size_t zipEndOffset = zipStartOffset + chunkSize;

                size_t absoluteZipEndOffset = byteOffset + zipEndOffset;
                uint64_t zipFileSize = absoluteZipEndOffset - absoluteZipStartOffset;

                // Extract the ZIP data by reading from disk
                std::vector<uint8_t> zipData;
                if (extractFileData(absoluteZipStartOffset, zipFileSize, sectorSize, zipData)) {
                    // For simplicity, we'll use sequential offsets
                    static uint64_t offset = 0;
                    carvedFiles.emplace_back(offset + absoluteZipStartOffset, zipData);
                    offset += zipData.size();
                    chunkSuccess = true;

                    // Continue searching after this ZIP chunk
                    zipSearchPos = zipEndOffset;
                } else {
                    // If we couldn't extract the data, move past this start to look for another
                    zipSearchPos = zipStartOffset + 1;
                }
            }
        }

        if (chunkSuccess) {
            success = true;
        }
    }

    return success;
}

} // namespace recovery
} // namespace recoverysuite