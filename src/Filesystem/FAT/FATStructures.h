#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATSTRUCTURES_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATSTRUCTURES_H

#include <cstdint>
#include <string>
#include "FATConstants.h"

namespace recoverysuite {
namespace filesystem {
namespace fat {

#pragma pack(push, 1)

// Common FAT Boot Sector Structure (BIOS Parameter Block)
struct FATBootSectorCommon {
    // Jump Instruction and OEM Name
    uint8_t jumpInstruction[3];     // 0x00-0x02: Jump instruction (either EB xx 90 or E9 xx xx 90)
    char oemName[8];                // 0x03-0x0A: OEM Name (8 bytes)

    // BPB (BIOS Parameter Block) - Common to all FAT types
    uint16_t bytesPerSector;        // 0x0B-0x0C: Bytes per sector (usually 512)
    uint8_t sectorsPerCluster;      // 0x0D: Sectors per cluster (power of 2, max 128)
    uint16_t reservedSectorCount;   // 0x0E-0x0F: Reserved sector count (including boot sector)
    uint8_t fatCount;               // 0x10: Number of FAT copies (usually 2)
    uint16_t rootEntryCount;        // 0x11-0x12: Maximum number of FAT12/FAT16 root directory entries
    uint16_t totalSectors16;        // 0x13-0x14: Total sectors (if 0, use totalSectors32)
    uint8_t mediaType;              // 0x15: Media type (0xF0 = removable, 0xF8 = fixed disk)
    uint16_t sectorsPerFat16;       // 0x16-0x17: Sectors per FAT (FAT12/FAT16 only)
    uint16_t sectorsPerTrack;       // 0x18-0x19: Sectors per track
    uint16_t headCount;             // 0x1A-0x1B: Number of heads
    uint32_t hiddenSectors;         // 0x1C-0x1F: Hidden sector count
    uint32_t totalSectors32;        // 0x20-0x23: Total sectors (if totalSectors16 == 0)

    // Extended BPB - differs by FAT type
    union {
        struct {                    // FAT12/FAT16 Extended BPB (starts at offset 0x24)
            uint8_t driveNumber;            // 0x24: Drive number (0x00 = floppy, 0x80 = hard disk)
            uint8_t reserved1;              // 0x25: Reserved (usually 0x00)
            uint8_t bootSignature;          // 0x26: Extended boot signature (0x29)
            uint32_t volumeID;              // 0x27-0x2A: Volume serial number
            char volumeLabel[11];           // 0x2B-0x35: Volume label (padded with spaces)
            char fatTypeLabel[8];           // 0x36-0x3D: FAT type label (e.g., "FAT16   ")
        } fat16;

        struct {                    // FAT32 Extended BPB (starts at offset 0x24)
            uint32_t sectorsPerFat32;       // 0x24-0x27: Sectors per FAT (FAT32 only)
            uint16_t extFlags;              // 0x28-0x29: Extended flags
            uint16_t fsVersion;             // 0x2A-0x2B: Filesystem version (usually 0x0000)
            uint32_t rootCluster;           // 0x2C-0x2F: First cluster of root directory (usually 2)
            uint16_t fsInfoSector;          // 0x30-0x31: FSInfo sector number (usually 1)
            uint16_t backupBootSector;      // 0x32-0x33: Backup boot sector number (usually 6)
            uint8_t reserved2[12];          // 0x34-0x3F: Reserved
            uint8_t driveNumber;            // 0x40: Drive number
            uint8_t reserved3;              // 0x41: Reserved
            uint8_t bootSignature;          // 0x42: Extended boot signature (0x29)
            uint32_t volumeID;              // 0x43-0x46: Volume serial number
            char volumeLabel[11];           // 0x47-0x51: Volume label (padded with spaces)
            char fatTypeLabel[8];           // 0x52-0x59: FAT type label (e.g., "FAT32   ")
        } fat32;
    };

    // Boot sector signature
    uint16_t bootSignature;         // 0x1FE-0x1FF: Boot sector signature (0xAA55)
};

#pragma pack(pop)

// FSInfo Structure (FAT32 only)
#pragma pack(push, 1)
struct FSInfoSector {
    uint32_t leadingSignature;      // 0x000-0x003: Leading signature (0x41615252)
    uint8_t reserved1[480];         // 0x004-0x1EF: Reserved
    uint32_t strucSignature;        // 0x1F0-0x1F3: Structure signature (0x61417272)
    uint32_t freeClusterCount;      // 0x1F4-0x1F7: Free cluster count (0xFFFFFFFF = unknown)
    uint32_t nextFreeCluster;       // 0x1F8-0x1FB: Next free cluster hint
    uint8_t reserved2[12];          // 0x1FC-0x207: Reserved
    uint32_t trailingSignature;     // 0x208-0x20B: Trailing signature (0xAA550000)
};
#pragma pack(pop)

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATSTRUCTURES_H