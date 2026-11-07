#include "FATMetadata.h"
#include "fat_boot_geometry.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace recoverysuite {
namespace filesystem {
namespace fat {

FATMetadata::FATMetadata(FilesystemReader &reader) { initialize(reader); }

FilesystemType FATMetadata::getType() const {
  switch (fatType_) {
  case fat::FATType::FAT12:
    return FilesystemType::FAT12;
  case fat::FATType::FAT16:
    return FilesystemType::FAT16;
  case fat::FATType::FAT32:
    return FilesystemType::FAT32;
  default:
    return FilesystemType::UNKNOWN;
  }
}

std::string FATMetadata::getName() const {
  return fat::fatTypeToString(fatType_);
}

std::string FATMetadata::getVersion() const {
  // FAT filesystems don't have a version in the traditional sense
  return "";
}

std::string FATMetadata::getSerialNumber() const {
  // Return the volume serial number as a string
  uint32_t serial = getVolumeSerialNumber();
  char buffer[11];
  // Format as 8-digit hex with leading zeros
  snprintf(buffer, sizeof(buffer), "%08lX", static_cast<unsigned long>(serial));
  return std::string(buffer);
}

uint32_t FATMetadata::getVolumeSerialNumber() const {
  // Return the volume serial number from the BPB
  if (fatType_ == fat::FATType::FAT32) {
    return bootSector_.fat32.volumeID;
  } else {
    return bootSector_.fat16.volumeID;
  }
}

std::string FATMetadata::getVolumeLabel() const {
  // Volume label is 11 bytes, padded with spaces
  char label[12] = {0};
  if (fatType_ == fat::FATType::FAT32) {
    std::memcpy(label, bootSector_.fat32.volumeLabel, 11);
  } else {
    std::memcpy(label, bootSector_.fat16.volumeLabel, 11);
  }
  // Remove trailing spaces
  std::string str(label);
  str.erase(str.find_last_not_of(' ') + 1);
  return str;
}

fat::FATType FATMetadata::getFatType() const { return fatType_; }

uint16_t FATMetadata::getBytesPerSector() const {
  return bootSector_.bytesPerSector;
}

uint8_t FATMetadata::getSectorsPerCluster() const {
  return bootSector_.sectorsPerCluster;
}

uint16_t FATMetadata::getReservedSectorCount() const {
  return bootSector_.reservedSectorCount;
}

uint8_t FATMetadata::getFatCount() const { return bootSector_.fatCount; }

uint16_t FATMetadata::getRootEntryCount() const {
  return bootSector_.rootEntryCount;
}

uint16_t FATMetadata::getTotalSectors16() const {
  return bootSector_.totalSectors16;
}

uint32_t FATMetadata::getTotalSectors32() const {
  return bootSector_.totalSectors32;
}

uint16_t FATMetadata::getSectorsPerFat16() const {
  return bootSector_.sectorsPerFat16;
}

uint32_t FATMetadata::getSectorsPerFat32() const {
  if (fatType_ == fat::FATType::FAT32) {
    return bootSector_.fat32.sectorsPerFat32;
  }
  return 0;
}

uint64_t FATMetadata::getTotalSize() const {
  uint32_t totalSectors = (bootSector_.totalSectors16 != 0)
                              ? bootSector_.totalSectors16
                              : bootSector_.totalSectors32;
  return static_cast<uint64_t>(totalSectors) * bootSector_.bytesPerSector;
}

uint64_t FATMetadata::getFreeSpace() const {
  throw std::logic_error(
      "FAT free space is unknown without allocation analysis");
}

uint64_t FATMetadata::getUsedSpace() const {
  throw std::logic_error(
      "FAT used space is unknown without allocation analysis");
}

uint32_t FATMetadata::getBlockSize() const {
  return bootSector_.bytesPerSector;
}

uint64_t FATMetadata::getBlockCount() const {
  uint32_t totalSectors = (bootSector_.totalSectors16 != 0)
                              ? bootSector_.totalSectors16
                              : bootSector_.totalSectors32;
  return static_cast<uint64_t>(totalSectors);
}

uint64_t FATMetadata::getFreeBlockCount() const {
  throw std::logic_error(
      "FAT free block count is unknown without allocation analysis");
}

bool FATMetadata::isReadOnly() const {
  // For now, we are implementing read-only foundation, so return true.
  return true;
}

bool FATMetadata::isMounted() const {
  throw std::logic_error(
      "FAT mount state is not observed by boot metadata inspection");
}

void FATMetadata::initialize(FilesystemReader &reader) {
  bootSector_ = readFatBootSector(reader);
  fatType_ = validatedFatType(bootSector_, reader.getSectorSize());
}

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite
