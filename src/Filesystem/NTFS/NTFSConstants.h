#ifndef RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_CONSTANTS_H
#define RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_CONSTANTS_H

#include <cstdint>

namespace recoverysuite {
namespace filesystem {
namespace ntfs {

// NTFS Signature Constants
static constexpr uint32_t NTFS_SIGNATURE = 0x45585453; // "NTFS"
static constexpr uint16_t NTFS_SECTOR_SIZE_DEFAULT = 512;
static constexpr uint64_t NTFS_MAX_VOLUME_SIZE = 0xFFFFFFFFFFFFFFFF; // 2^64 - 1 bytes

// MFT Constants
static constexpr uint32_t MFT_FILE_RECORD_SIGNATURE = 0x454C4946; // "FILE"
static constexpr uint32_t MFT_SEQUENCE_NUMBER_DEFAULT = 1;
static constexpr uint32_t MFT_HARD_LINK_COUNT_DEFAULT = 1;

// Attribute Types
enum class AttributeType : uint32_t {
    STANDARD_INFORMATION = 0x10,
    ATTRIBUTE_LIST = 0x20,
    FILE_NAME = 0x30,
    OBJECT_ID = 0x40,
    SECURITY_DESCRIPTOR = 0x50,
    VOLUME_NAME = 0x60,
    VOLUME_INFORMATION = 0x70,
    DATA = 0x80,
    INDEX_ROOT = 0x90,
    INDEX_ALLOCATION = 0xA0,
    BITMAP = 0xB0,
    REPARSE_POINT = 0xC0,
    EA_INFORMATION = 0xD0,
    EA = 0xE0,
    PROPERTY_SET = 0xF0,
    LOGGED_UTILITY_STREAM = 0x100
};

// Collation Rules
enum class CollationRule : uint8_t {
    FILE_NAME = 0,
    FILE_NAME_REVERSE = 1,
    COLLATION_MAX = 0xFF
};

// Index Entry Flags
enum class IndexEntryFlags : uint16_t {
    INDEX_ENTRY_NODE = 0x0001,
    INDEX_ENTRY_END = 0x0002
};

// File Record Flags
enum class FileRecordFlags : uint16_t {
    FILE_RECORD_FLAG_IN_USE = 0x0001,
    FILE_RECORD_FLAG_IS_DIRECTORY = 0x0002
};

} // namespace ntfs
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_NTFS_NTFS_CONSTANTS_H
