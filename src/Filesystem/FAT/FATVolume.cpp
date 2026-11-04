#include "FATVolume.h"
#include "fat_boot_geometry.h"
#include <iomanip>
#include <limits>
#include <sstream>

namespace recoverysuite {
namespace filesystem {
namespace fat {

FATVolume::FATVolume(uint64_t startOffset, uint32_t sectorSize,
                     const FATBootSectorCommon &bootSector)
    : startOffset_(startOffset), sectorSize_(sectorSize),
      bootSector_(bootSector) {}

std::string FATVolume::getVolumeIdentifier() const {
  std::stringstream ss;
  ss << "Offset: 0x" << std::hex << std::uppercase << std::setw(16)
     << std::setfill('0') << startOffset_;
  return ss.str();
}

uint64_t FATVolume::getStartOffset() const { return startOffset_; }

uint64_t FATVolume::getSize() const { return calculateSize(); }

uint64_t FATVolume::getEndOffset() const {
  const auto size = getSize();
  if (!size || size - 1 > std::numeric_limits<uint64_t>::max() - startOffset_)
    throw std::runtime_error("FAT volume end offset overflow");
  return startOffset_ + (size - 1);
}

bool FATVolume::isBootable() const {
  throw std::logic_error("A FAT boot signature does not establish bootability");
}

// Check if the volume is marked as active (for BIOS boot)
bool FATVolume::isActive() const {
  throw std::logic_error(
      "FAT partition flags are not observed by boot metadata inspection");
}

// Check if the volume is hidden
bool FATVolume::isHidden() const {
  throw std::logic_error(
      "FAT partition flags are not observed by boot metadata inspection");
}

// Get the partition type (if applicable)
uint8_t FATVolume::getPartitionType() const {
  throw std::logic_error(
      "FAT partition type is not observed by boot metadata inspection");
}

FilesystemType FATVolume::getFilesystemType() const {
  switch (validatedFatType(bootSector_, sectorSize_)) {
  case FATType::FAT12:
    return FilesystemType::FAT12;
  case FATType::FAT16:
    return FilesystemType::FAT16;
  case FATType::FAT32:
    return FilesystemType::FAT32;
  default:
    return FilesystemType::UNKNOWN;
  }
}

bool FATVolume::isValid() const {
  try {
    (void)validatedFatType(bootSector_, sectorSize_);
    const auto size = calculateSize();
    return size &&
           size - 1 <= std::numeric_limits<uint64_t>::max() - startOffset_;
  } catch (const std::exception &) {
    return false;
  }
}

uint64_t FATVolume::calculateSize() const {
  (void)validatedFatType(bootSector_, sectorSize_);
  uint32_t totalSectors = (bootSector_.totalSectors16 != 0)
                              ? bootSector_.totalSectors16
                              : bootSector_.totalSectors32;
  return static_cast<uint64_t>(totalSectors) * sectorSize_;
}

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite