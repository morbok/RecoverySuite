#ifndef RECOVERYSUITE_FILESYSTEM_FILESYSTEM_TYPES_H
#define RECOVERYSUITE_FILESYSTEM_FILESYSTEM_TYPES_H

#include <cstdint>
#include <string>

namespace recoverysuite {
namespace filesystem {

// Filesystem type enumeration
enum class FilesystemType : uint8_t {
    UNKNOWN = 0,
    NTFS = 1,
    FAT12 = 2,
    FAT16 = 3,
    FAT32 = 4,
    EXFAT = 5,
    EXT2 = 6,
    EXT3 = 7,
    EXT4 = 8,
    APFS = 9,
    HFS_PLUS = 10,
    BTRFS = 11,
    XFS = 12,
    REFS = 13
};

// Convert FilesystemType to string
inline std::string filesystemTypeToString(FilesystemType type) {
    switch (type) {
        case FilesystemType::NTFS: return "NTFS";
        case FilesystemType::FAT12: return "FAT12";
        case FilesystemType::FAT16: return "FAT16";
        case FilesystemType::FAT32: return "FAT32";
        case FilesystemType::EXFAT: return "exFAT";
        case FilesystemType::EXT2: return "EXT2";
        case FilesystemType::EXT3: return "EXT3";
        case FilesystemType::EXT4: return "EXT4";
        case FilesystemType::APFS: return "APFS";
        case FilesystemType::HFS_PLUS: return "HFS+";
        case FilesystemType::BTRFS: return "Btrfs";
        case FilesystemType::XFS: return "XFS";
        case FilesystemType::REFS: return "ReFS";
        default: return "Unknown";
    }
}

// Convert string to FilesystemType
inline FilesystemType stringToFilesystemType(const std::string& str) {
    if (str == "NTFS") return FilesystemType::NTFS;
    if (str == "FAT12") return FilesystemType::FAT12;
    if (str == "FAT16") return FilesystemType::FAT16;
    if (str == "FAT32") return FilesystemType::FAT32;
    if (str == "exFAT") return FilesystemType::EXFAT;
    if (str == "EXT2") return FilesystemType::EXT2;
    if (str == "EXT3") return FilesystemType::EXT3;
    if (str == "EXT4") return FilesystemType::EXT4;
    if (str == "APFS") return FilesystemType::APFS;
    if (str == "HFS+") return FilesystemType::HFS_PLUS;
    if (str == "Btrfs") return FilesystemType::BTRFS;
    if (str == "XFS") return FilesystemType::XFS;
    if (str == "ReFS") return FilesystemType::REFS;
    return FilesystemType::UNKNOWN;
}

// Flags for filesystem capabilities
enum class FilesystemCapability : uint64_t {
    NONE = 0,
    READ_SUPPORT = 1ULL << 0,
    WRITE_SUPPORT = 1ULL << 1,
    RECOVERY_SUPPORT = 1ULL << 2,
    METADATA_SUPPORT = 1ULL << 3,
    JOURNAL_SUPPORT = 1ULL << 4,
    COMPRESSION_SUPPORT = 1ULL << 5,
    ENCRYPTION_SUPPORT = 1ULL << 6,
    SPARSE_FILE_SUPPORT = 1ULL << 7,
    EXTENDED_ATTRIBUTES_SUPPORT = 1ULL << 8,
    HARD_LINKS_SUPPORT = 1ULL << 9,
    SYMBOLIC_LINKS_SUPPORT = 1ULL << 10,
    ACL_SUPPORT = 1ULL << 11,
    QUOTA_SUPPORT = 1ULL << 12
};

// Bitmask for filesystem capabilities
using FilesystemCapabilities = uint64_t;

// Enable bitwise operations for FilesystemCapability
inline FilesystemCapability operator|(FilesystemCapability a, FilesystemCapability b) {
    return static_cast<FilesystemCapability>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
}
inline FilesystemCapability operator&(FilesystemCapability a, FilesystemCapability b) {
    return static_cast<FilesystemCapability>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
}
inline FilesystemCapability& operator|=(FilesystemCapability& a, FilesystemCapability b) {
    a = a | b;
    return a;
}
inline FilesystemCapability& operator&=(FilesystemCapability& a, FilesystemCapability b) {
    a = a & b;
    return a;
}

} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FILESYSTEM_TYPES_H
