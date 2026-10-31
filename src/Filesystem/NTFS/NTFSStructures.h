#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_STRUCTURES_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_STRUCTURES_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include "NTFSConstants.h"

#pragma pack(push, 1)

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// Boot Sector Structure
struct BootSector {
    uint8_t jumpInstruction[3];
    uint8_t oemId[8];
    uint16_t bytesPerSector;             // 0x0B
    uint8_t sectorsPerCluster;           // 0x0D
    uint16_t reservedSectorCount;
    uint8_t reserved1[3];
    uint16_t totalSectorCount16;         // Must be zero on NTFS
    uint8_t mediaDescriptor;
    uint16_t sectorsPerFat;              // Must be zero on NTFS
    uint16_t sectorsPerTrack;
    uint16_t headCount;
    uint32_t hiddenSectorCount;
    uint32_t totalSectorCount32;         // Must be zero on NTFS
    uint32_t reserved2;
    uint64_t totalSectorCount64;         // 0x28
    uint64_t mftCluster;                 // 0x30
    uint64_t mftMirrorCluster;           // 0x38
    int8_t clustersPerFileRecord;        // 0x40; negative encodes a byte-size exponent
    uint8_t reserved3[3];
    int8_t clustersPerIndexBuffer;       // 0x44
    uint8_t reserved4[3];
    uint8_t volumeSerialNumber[8];       // 0x48
    uint32_t checksum;                   // 0x50
    uint8_t bootstrapCode[426];
    uint16_t bootSignature;              // 0x1FE
};
static_assert(sizeof(BootSector)==512);
static_assert(offsetof(BootSector,totalSectorCount64)==40);
static_assert(offsetof(BootSector,mftCluster)==48);
static_assert(offsetof(BootSector,clustersPerFileRecord)==64);
static_assert(offsetof(BootSector,volumeSerialNumber)==72);
static_assert(offsetof(BootSector,bootSignature)==510);

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
    uint16_t reserved;
    uint32_t recordNumber;            // NTFS 3.x record number
};

// Common attribute header plus resident-only fields; nonresident headers use the extension below.
struct AttributeHeader {
    uint32_t type;                    // Attribute type
    uint32_t length;                  // Length of attribute
    uint8_t  nonResident;             // 0 = resident, 1 = non-resident
    uint8_t  nameLength;              // Length of attribute name (in chars)
    uint16_t nameOffset;              // Offset to attribute name
    uint16_t flags;                   // Attribute flags (compressed, encrypted, sparse)
    uint16_t instance;                // Attribute instance
    uint32_t valueLength;             // Length of attribute value
    uint16_t valueOffset;             // Offset to attribute value
    uint8_t  indexedFlag;             // Resident value is indexed
    uint8_t  reserved;                // Reserved for alignment
};

// Resident Attribute Value
struct ResidentAttribute {
    // Value follows immediately after header
};

// Nonresident header extension, starting at byte 16 of an attribute record.
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

// Decoded run descriptor; mapping pairs themselves have variable-length encoding.
struct DataRun {
    uint8_t  lengthSize;              // Mapping-pairs code: low nibble length width, high offset width
    int64_t  offset;                  // Cluster offset (signed)
    uint64_t length;                  // Cluster length
};

// Index Entry
struct IndexEntry {
    uint64_t fileReference;           // File reference (MFT number + sequence)
    uint16_t length;                  // Length of index entry
    uint16_t keyLength;               // Length of indexed key
    uint16_t flags;                   // Index entry flags
    uint16_t reserved;                // Reserved (zero)
    // Attribute header follows
};

static_assert(sizeof(FileRecordHeader)==48);
static_assert(offsetof(AttributeHeader,flags)==12);
static_assert(sizeof(AttributeHeader)==24);
static_assert(sizeof(IndexEntry)==16);

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#pragma pack(pop)

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_STRUCTURES_H
