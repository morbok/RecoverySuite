#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATCONSTANTS_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATCONSTANTS_H

#include <cstdint>

namespace recoverysuite {
namespace filesystem {
namespace fat {

// FAT boot sector signature
constexpr uint16_t FAT_BOOT_SIGNATURE = 0xAA55;

// FAT12 constants
constexpr uint32_t FAT12_MAX_CLUSTERS = 4085;
constexpr uint32_t FAT12_FAT_SIZE_SECTORS_MAX = 7;

// FAT16 constants
constexpr uint32_t FAT16_MIN_CLUSTERS = 4086;
constexpr uint32_t FAT16_MAX_CLUSTERS = 65525;
constexpr uint32_t FAT16_FAT_SIZE_SECTORS_MAX = 255;

// FAT32 constants
constexpr uint32_t FAT32_MIN_CLUSTERS = 65526;
constexpr uint32_t FAT32_MAX_CLUSTERS = 268435445;
constexpr uint32_t FAT32_FAT_SIZE_SECTORS_MAX = 0xFFFFFFFF;

// FAT types
enum class FATType : uint8_t {
    FAT12 = 1,
    FAT16 = 2,
    FAT32 = 3,
    UNKNOWN = 0
};

// Convert FATType to string
inline std::string fatTypeToString(FATType type) {
    switch (type) {
        case FATType::FAT12: return "FAT12";
        case FATType::FAT16: return "FAT16";
        case FATType::FAT32: return "FAT32";
        default: return "Unknown";
    }
}

// Boot sector jump instruction types
enum class JumpInstruction : uint8_t {
    SHORT_JUMP_NOP = 0xEB,    // EB xx 90
    NEAR_JUMP_NOP = 0xE9      // E9 xx xx 90
};

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATCONSTANTS_H