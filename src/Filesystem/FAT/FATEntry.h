#ifndef RECOVERYSUITE_FILESYSTEM_FAT_FATENTRY_H
#define RECOVERYSUITE_FILESYSTEM_FAT_FATENTRY_H

#include <cstdint>
#include <string>

namespace recoverysuite {
namespace filesystem {
namespace fat {

// FAT entry values that are consistent across FAT12/16/32
enum class FATEntryType : uint32_t {
    FREE = 0x00000000,           // Free cluster
    RESERVED = 0xFFFFFFF0,       // Reserved cluster range start
    BAD = 0xFFFFFFF7,            // Bad cluster
    END_OF_CHAIN = 0xFFFFFFF8    // End of chain marker (actual value depends on FAT type)
};

// FAT12 specific entry handling
class FAT12Entry {
public:
    // FAT12 entries are packed 12 bits per entry, 3 entries per 4 bytes
    // Entry n (0-based) starts at byte offset: (n * 3) / 2
    // If n is even: bits 0-11 of byte offset
    // If n is odd:  bits 4-15 of byte offset (shifted right 4)

    static uint16_t getEntry(const uint8_t* fatBuffer, uint32_t clusterIndex, uint32_t sectorsPerFat, uint32_t bytesPerSector) {
        uint32_t fatOffset = clusterIndex * 3 / 2;
        uint32_t sectorIndex = fatOffset / bytesPerSector;
        uint32_t sectorOffset = fatOffset % bytesPerSector;

        if (sectorIndex >= sectorsPerFat) {
            return 0xFFF; // Treat as end of chain if out of bounds
        }

        const uint8_t* sectorData = fatBuffer + (sectorIndex * bytesPerSector);

        if (sectorOffset + 2 > bytesPerSector) {
            // Handle cross-sector case (simplified - in reality we'd need to read next sector)
            return 0xFFF;
        }

        uint16_t value;
        std::memcpy(&value, sectorData + sectorOffset, 2);

        if (clusterIndex % 2 == 0) {
            // Even cluster index: lower 12 bits
            return value & 0x0FFF;
        } else {
            // Odd cluster index: upper 12 bits
            return (value & 0xF000) >> 4;
        }
    }

    static void setEntry(uint8_t* fatBuffer, uint32_t clusterIndex, uint16_t value,
                        uint32_t sectorsPerFat, uint32_t bytesPerSector) {
        // Read-only implementation - setting not implemented for safety
        (void)fatBuffer; (void)clusterIndex; (void)value;
        (void)sectorsPerFat; (void)bytesPerSector;
    }

    static bool isEndOfChain(uint16_t value) {
        return (value & 0x0FF8) == 0x0FF8; // FAT12 end of chain: 0xFF8-0xFFF
    }

    static bool isBad(uint16_t value) {
        return value == 0xFF7;
    }

    static bool isFree(uint16_t value) {
        return value == 0x000;
    }
};

// FAT16 specific entry handling
class FAT16Entry {
public:
    static uint16_t getEntry(const uint8_t* fatBuffer, uint32_t clusterIndex,
                            uint32_t sectorsPerFat, uint32_t bytesPerSector) {
        uint32_t fatOffset = clusterIndex * 2;
        uint32_t sectorIndex = fatOffset / bytesPerSector;
        uint32_t sectorOffset = fatOffset % bytesPerSector;

        if (sectorIndex >= sectorsPerFat || sectorOffset + 2 > bytesPerSector) {
            return 0xFFFF; // Treat as end of chain if out of bounds
        }

        const uint8_t* sectorData = fatBuffer + (sectorIndex * bytesPerSector);
        uint16_t value;
        std::memcpy(&value, sectorData + sectorOffset, 2);
        return value;
    }

    static void setEntry(uint8_t* fatBuffer, uint32_t clusterIndex, uint16_t value,
                        uint32_t sectorsPerFat, uint32_t bytesPerSector) {
        // Read-only implementation
        (void)fatBuffer; (void)clusterIndex; (void)value;
        (void)sectorsPerFat; (void)bytesPerSector;
    }

    static bool isEndOfChain(uint16_t value) {
        return (value & 0xFFF8) == 0xFFF8; // FAT16 end of chain: 0xFFF8-0xFFFF
    }

    static bool isBad(uint16_t value) {
        return value == 0xFFF7;
    }

    static bool isFree(uint16_t value) {
        return value == 0x0000;
    }
};

// FAT32 specific entry handling
class FAT32Entry {
public:
    static uint32_t getEntry(const uint8_t* fatBuffer, uint32_t clusterIndex,
                            uint32_t sectorsPerFat, uint32_t bytesPerSector) {
        uint32_t fatOffset = clusterIndex * 4;
        uint32_t sectorIndex = fatOffset / bytesPerSector;
        uint32_t sectorOffset = fatOffset % bytesPerSector;

        if (sectorIndex >= sectorsPerFat || sectorOffset + 4 > bytesPerSector) {
            return 0x0FFFFFFF; // Treat as end of chain if out of bounds
        }

        const uint8_t* sectorData = fatBuffer + (sectorIndex * bytesPerSector);
        uint32_t value;
        std::memcpy(&value, sectorData + sectorOffset, 4);
        return value & 0x0FFFFFFF; // Mask to 28 bits as per FAT32 spec
    }

    static void setEntry(uint8_t* fatBuffer, uint32_t clusterIndex, uint32_t value,
                        uint32_t sectorsPerFat, uint32_t bytesPerSector) {
        // Read-only implementation
        (void)fatBuffer; (void)clusterIndex; (void)value;
        (void)sectorsPerFat; (void)bytesPerSector;
    }

    static bool isEndOfChain(uint32_t value) {
        return (value & 0x0FFFFFF8) == 0x0FFFFFF8; // FAT32 end of chain: 0x0FFFFFF8-0x0FFFFFFF
    }

    static bool isBad(uint32_t value) {
        return value == 0x0FFFFFF7;
    }

    static bool isFree(uint32_t value) {
        return value == 0x00000000;
    }

    static bool isReserved(uint32_t value) {
        return (value >= 0x0FFFFFF0 && value <= 0x0FFFFFF6);
    }
};

// Unified FAT entry interface
class FATEntry {
public:
    FATEntry() : type_(FATType::UNKNOWN), value_(0) {}

    FATEntry(FATType type, uint32_t value) : type_(type), value_(value) {}

    // Get the entry value based on FAT type
    static FATEntry fromFatBuffer(const uint8_t* fatBuffer, FATType fatType,
                                 uint32_t clusterIndex, uint32_t sectorsPerFat,
                                 uint32_t bytesPerSector) {
        switch (fatType) {
            case FATType::FAT12:
                return FATEntry(FATType::FAT12, FAT12Entry::getEntry(fatBuffer, clusterIndex, sectorsPerFat, bytesPerSector));
            case FATType::FAT16:
                return FATEntry(FATType::FAT16, FAT16Entry::getEntry(fatBuffer, clusterIndex, sectorsPerFat, bytesPerSector));
            case FATType::FAT32:
                return FATEntry(FATType::FAT32, FAT32Entry::getEntry(fatBuffer, clusterIndex, sectorsPerFat, bytesPerSector));
            default:
                return FATEntry(FATType::UNKNOWN, 0);
        }
    }

    // Getters
    FATType getType() const { return type_; }
    uint32_t getValue() const { return value_; }

    // Cluster state checks
    bool isFree() const {
        switch (type_) {
            case FATType::FAT12: return FAT12Entry::isFree(static_cast<uint16_t>(value_));
            case FATType::FAT16: return FAT16Entry::isFree(static_cast<uint16_t>(value_));
            case FATType::FAT32: return FAT32Entry::isFree(value_);
            default: return false;
        }
    }

    bool isBad() const {
        switch (type_) {
            case FATType::FAT12: return FAT12Entry::isBad(static_cast<uint16_t>(value_));
            case FATType::FAT16: return FAT16Entry::isBad(static_cast<uint16_t>(value_));
            case FATType::FAT32: return FAT32Entry::isBad(value_);
            default: return false;
        }
    }

    bool isReserved() const {
        switch (type_) {
            case FATType::FAT12:
                return (static_cast<uint16_t>(value_) >= 0xFFF0 &&
                       static_cast<uint16_t>(value_) <= 0xFFFE);
            case FATType::FAT16:
                return (static_cast<uint16_t>(value_) >= 0xFFF0 &&
                       static_cast<uint16_t>(value_) <= 0xFFFE);
            case FATType::FAT32:
                return FAT32Entry::isReserved(value_);
            default: return false;
        }
    }

    bool isEndOfChain() const {
        switch (type_) {
            case FATType::FAT12: return FAT12Entry::isEndOfChain(static_cast<uint16_t>(value_));
            case FATType::FAT16: return FAT16Entry::isEndOfChain(static_cast<uint16_t>(value_));
            case FATType::FAT32: return FAT32Entry::isEndOfChain(value_);
            default: return false;
        }
    }

    uint32_t getNextCluster() const {
        // Return the cluster ID for the next cluster in chain
        // For end of chain, return a special value
        if (isEndOfChain()) {
            return END_OF_CHAIN_CLUSTER;
        }
        return value_;
    }

    std::string toString() const {
        switch (type_) {
            case FATType::FAT12:
                return "FAT12: 0x" + toHex(static_cast<uint16_t>(value_), 3);
            case FATType::FAT16:
                return "FAT16: 0x" + toHex(static_cast<uint16_t>(value_), 4);
            case FATType::FAT32:
                return "FAT32: 0x" + toHex(value_, 8);
            default:
                return "Unknown FAT type";
        }
    }

private:
    FATType type_;
    uint32_t value_;

    static constexpr uint32_t END_OF_CHAIN_CLUSTER = 0xFFFFFFFF;

    static std::string toHex(uint32_t value, int digits) {
        const char* hexChars = "0123456789ABCDEF";
        std::string result;
        for (int i = digits - 1; i >= 0; --i) {
            result += hexChars[(value >> (i * 4)) & 0xF];
        }
        return result;
    }
};

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite

#endif // RECOVERYSUITE_FILESYSTEM_FAT_FATENTRY_H