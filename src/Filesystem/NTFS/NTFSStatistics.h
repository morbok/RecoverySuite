#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_STATISTICS_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_STATISTICS_H

#include <cstdint>
#include <chrono>

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// NTFS Statistics
struct Statistics {
    // Counters
    uint64_t totalFileRecordsProcessed;   // Total MFT records processed
    uint64_t totalFilesFound;             // Total files found
    uint64_t totalDirectoriesFound;       // Total directories found
    uint64_t totalAttributesProcessed;    // Total attributes processed
    uint64_t totalIndexEntriesProcessed;  // Total index entries processed
    
    uint64_t corruptedFileRecords;        // Corrupted MFT records found
    uint64_t corruptedAttributes;         // Corrupted attributes found
    uint64_t corruptedIndexEntries;       // Corrupted index entries found
    uint64_t orphanedFileRecords;         // Orphaned MFT records found
    
    uint64_t totalBytesRead;              // Total bytes read from disk
    uint64_t totalBytesAllocated;         // Total allocated space found
    uint64_t totalBytesUsed;              // Total actual data size found
    
    // Timing
    std::chrono::duration<double> parseTime;     // Time spent parsing
    std::chrono::duration<double> ioTime;        // Time spent on I/O
    std::chrono::duration<double> analysisTime;  // Time spent analyzing
    
    // Rates
    double recordsPerSecond() const {
        return parseTime.count() > 0 ? 
               static_cast<double>(totalFileRecordsProcessed) / parseTime.count() : 0.0;
    }
    
    double bytesPerSecond() const {
        return ioTime.count() > 0 ? 
               static_cast<double>(totalBytesRead) / ioTime.count() : 0.0;
    }
    
    // Reset statistics
    void reset() {
        totalFileRecordsProcessed = 0;
        totalFilesFound = 0;
        totalDirectoriesFound = 0;
        totalAttributesProcessed = 0;
        totalIndexEntriesProcessed = 0;
        corruptedFileRecords = 0;
        corruptedAttributes = 0;
        corruptedIndexEntries = 0;
        orphanedFileRecords = 0;
        totalBytesRead = 0;
        totalBytesAllocated = 0;
        totalBytesUsed = 0;
        parseTime = std::chrono::duration<double>::zero();
        ioTime = std::chrono::duration<double>::zero();
        analysisTime = std::chrono::duration<double>::zero();
    }
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_STATISTICS_H
