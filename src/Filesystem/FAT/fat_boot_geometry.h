#pragma once
#include "../FilesystemReader.h"
#include "FATStructures.h"
#include <cstring>
#include <stdexcept>
#include <vector>

namespace recoverysuite::filesystem::fat {
inline bool supportedFatSectorSize(uint32_t size) {
  return size == 512 || size == 1024 || size == 2048 || size == 4096;
}

// Validate declared boot geometry only; this does not inspect allocation chains
// or establish whole-volume consistency. Use wide arithmetic before
// subtraction.
inline FATType validatedFatType(const FATBootSectorCommon &boot,
                                uint32_t sectorSize) {
  if (!supportedFatSectorSize(sectorSize) ||
      boot.bytesPerSector != sectorSize ||
      boot.bootSignature != FAT_BOOT_SIGNATURE || !boot.sectorsPerCluster ||
      boot.sectorsPerCluster > 128 ||
      (boot.sectorsPerCluster & (boot.sectorsPerCluster - 1)) ||
      !boot.reservedSectorCount || !boot.fatCount || boot.fatCount > 2)
    throw std::runtime_error("Invalid FAT boot geometry");
  const uint64_t total =
      boot.totalSectors16 ? boot.totalSectors16 : boot.totalSectors32;
  const uint64_t fat =
      boot.sectorsPerFat16 ? boot.sectorsPerFat16 : boot.fat32.sectorsPerFat32;
  const uint64_t root =
      (uint64_t(boot.rootEntryCount) * 32 + sectorSize - 1) / sectorSize;
  const uint64_t metadata =
      boot.reservedSectorCount + uint64_t(boot.fatCount) * fat + root;
  if (!fat || metadata >= total)
    throw std::runtime_error("FAT metadata exceeds declared volume");
  const uint64_t clusters = (total - metadata) / boot.sectorsPerCluster;
  if (!clusters || clusters >= FAT32_MAX_CLUSTERS)
    throw std::runtime_error("Invalid FAT cluster count");
  const auto type = clusters < FAT12_MAX_CLUSTERS   ? FATType::FAT12
                    : clusters < FAT16_MAX_CLUSTERS ? FATType::FAT16
                                                    : FATType::FAT32;
  if (type == FATType::FAT32) {
    if (boot.sectorsPerFat16 || boot.rootEntryCount || boot.totalSectors16 ||
        boot.fat32.fsVersion || boot.fat32.rootCluster < 2 ||
        boot.fat32.rootCluster > clusters + 1 ||
        ((boot.fat32.extFlags & 0x80) &&
         (boot.fat32.extFlags & 0xf) >= boot.fatCount))
      throw std::runtime_error("Invalid FAT32 boot fields");
  } else if (!boot.sectorsPerFat16 || !boot.rootEntryCount) {
    throw std::runtime_error("Invalid FAT12/16 boot fields");
  }
  const uint64_t bits = type == FATType::FAT12   ? 12
                        : type == FATType::FAT16 ? 16
                                                 : 32;
  if ((clusters + 2) * bits > fat * sectorSize * 8)
    throw std::runtime_error("FAT table cannot address declared clusters");
  return type;
}

inline FATBootSectorCommon readFatBootSector(FilesystemReader &reader) {
  const auto size = reader.getSectorSize();
  if (!supportedFatSectorSize(size))
    throw std::runtime_error("Unsupported FAT reader sector size");
  std::vector<uint8_t> bytes(size);
  if (reader.readSector(0, bytes.data(), size) != size)
    throw std::runtime_error("Incomplete FAT boot sector read");
  FATBootSectorCommon boot{};
  static_assert(sizeof(boot) == 512);
  std::memcpy(&boot, bytes.data(), sizeof(boot));
  (void)validatedFatType(boot, size);
  return boot;
}
} // namespace recoverysuite::filesystem::fat
