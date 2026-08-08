#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_STRUCTURES_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_STRUCTURES_H

#include <cstdint>
#include <string>
#include <vector>
#include "NTFSConstants.h"

#pragma pack(push, 1)

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// Boot Sector Structure
struct BootSector {
    uint8_t  jumpInstruction[3];      // 0x00 - Jump instruction
    uint8_t  oemId[8];                // 0x03 - OEM identifier
    uint16_t bytesPerSector;          // 0x0B - Bytes per sector
    uint8_t  sectorsPerCluster;       // 0x0D - Sectors per cluster
    uint16_t reservedSectorCount;     // 0x0E - Reserved sector count
    uint8_t  mediaDescriptor;         // 0x10 - Media descriptor
    uint16_t sectorsPerFat;           // 0x11 - Sectors per FAT (0 for NTFS)
    uint16_t sectorsPerTrack;         // 0x13 - Sectors per track
    uint16_t headCount;               // 0x15 - Number of heads
    uint32_t hiddenSectorCount;       // 0x16 - Hidden sector count
    uint32_t totalSectorCount32;      // 0x1A - Total sectors (32-bit)
    uint32_t totalSectorCount64;      // 0x1E - Total sectors (64-bit, high)
    uint32_t mftCluster;              // 0x24 - MFT start cluster
    uint32_t mftMirrorCluster;        // 0x28 - MFT mirror start cluster
    int32_t  clustersPerFileRecord;   // 0x2C - Clusters per file record (-ve = compress)
    int32_t  clustersPerIndexBuffer;  // 0x30 - Clusters per index buffer (-ve = compress)
    uint8_t  volumeSerialNumber[16];  // 0x34 - Volume serial number
    uint32_t checksum;                // 0x44 - Boot sector checksum
    // ... rest of boot sector (reserved, bootstrap code)
    uint8_t  bootstrapCode[426];      // 0x48 - Bootstrap code
    uint16_t bootSignature;           // 0x1FE - Boot signature (0x55AA)
};

// MFT File Record Header
struct FileRecordHeader {
    uint32_t signature;               // "FILE"
    uint16_t updateSequenceOffset;    // Offset to update sequence array
    uint16_t updateSequenceSize;      // Size of update sequence array
    uint64_t logFileSequenceNumber;   // $LogFile sequence number
    uint16_t sequenceNumber;          // Sequence number
    uint16_t hardLinkCount;           // Hard link count
    uint16_t firstAttributeOffset;    // Offset to first attribute
    uint16_t flags;                   // File record flags (in use, directory)
    uint32_t bytesInUse;              // Bytes used in file record
    uint32_t bytesAllocated;          // Bytes allocated for file record
    uint64_t baseFileRecord;          // Base file record reference
    uint16_t nextAttributeInstance;   // Next attribute instance number
};

// Attribute Header
struct AttributeHeader {
    uint32_t type;                    // Attribute type
    uint32_t length;                  // Length of attribute
    uint8_t  nonResident;             // 0 = resident, 1 = non-resident
    uint8_t  nameLength;              // Length of attribute name (in chars)
    uint16_t nameOffset;              // Offset to attribute name
    uint16_t instance;                // Attribute instance
    uint32_t valueLength;             // Length of attribute value
    uint16_t valueOffset;             // Offset to attribute value
    uint8_t  flags;                   // Attribute flags (compressed, encrypted, sparse)
    uint8_t  reserved;                // Reserved for alignment
};

// Resident Attribute Value
struct ResidentAttribute {
    // Value follows immediately after header
};

// Non-Resident Attribute Value
struct NonResidentAttribute {
    uint64_t startingVcn;             // Starting VCN
    uint64_t endingVcn;               // Ending VCN
    uint16_t dataRunOffset;           // Offset to data run
    uint16_t compressionUnitSize;     // Compression unit size (0 = none)
    uint32_t padding;                 // Padding to align
    uint64_t allocatedSize;           // Allocated size
    uint64_t dataSize;                // Actual data size
    uint64_t initializedSize;         // Initialized size
};

// Data Run Entry
struct DataRun {
    uint8_t  lengthSize;              // High 4 nibbits = length size, low 4 = offset size
    int64_t  offset;                  // Cluster offset (signed)
    uint64_t length;                  // Cluster length
};

// Index Entry
struct IndexEntry {
    uint64_t fileReference;           // File reference (MFT number + sequence)
    uint16_t length;                  // Length of index entry
    uint8_t  flags;                   // Index entry flags
    uint16_t reserved;                // Reserved (zero)
    // Attribute header follows
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#pragma pack(pop)

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_STRUCTURES_H
