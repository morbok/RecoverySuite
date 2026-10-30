#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
namespace recoverysuite::recovery {
/** Identify a supported boot sector only after validating arithmetic inputs. */
inline std::string detectBootSectorType(const std::vector<uint8_t>& data) {
    if (data.size() < 512 || data[510] != 0x55 || data[511] != 0xAA) return {};
    const auto le16 = [&data](size_t offset) -> uint64_t { return data[offset] | (uint64_t{data[offset + 1]} << 8); };
    const auto le32 = [&data](size_t offset) -> uint64_t {
        return data[offset] | (uint64_t{data[offset + 1]} << 8) |
               (uint64_t{data[offset + 2]} << 16) | (uint64_t{data[offset + 3]} << 24);
    };
    const uint64_t sectorSize = le16(11), clusterSize = data[13];
    if (sectorSize < 512 || sectorSize > 4096 || (sectorSize & (sectorSize - 1)) ||
        clusterSize == 0 || (clusterSize & (clusterSize - 1))) return {};
    if (std::memcmp(data.data() + 3, "NTFS    ", 8) == 0) return "NTFS";
    const uint64_t reserved = le16(14), fats = data[16], rootEntries = le16(17);
    const uint64_t sectors = le16(19) ? le16(19) : le32(32);
    const uint64_t fatSize = le16(22) ? le16(22) : le32(36);
    if (!reserved || !fats || !fatSize) return {};
    const uint64_t overhead = reserved + fats * fatSize + (rootEntries * 32 + sectorSize - 1) / sectorSize;
    if (sectors <= overhead) return {};
    const uint64_t clusters = (sectors - overhead) / clusterSize;
    return clusters < 4085 ? "FAT12" : (clusters < 65525 ? "FAT16" : "FAT32");
}
}
