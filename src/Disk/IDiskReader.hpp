#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <string>
#include "DiskExceptions.h"
#include "Core/Version.h"
#include "DeviceInformation.h"

namespace recoverysuite {
namespace disk {

/**
 * @brief Interface for reading sectors from a disk device
 *
 * This interface allows for platform-independent disk sector reading.
 * Platform-specific implementations will be provided in Platform-specific directories.
 */
class IDiskReader {
public:
    virtual ~IDiskReader() = default;

    /**
     * @brief Open the disk device for reading
     *
     * @param devicePath Path to the disk device (e.g., "\\\\.\\PhysicalDrive0" on Windows, "/dev/sda" on Linux)
     * @throws DiskException if opening fails
     */
    virtual void open(const std::string& devicePath) = 0;

    /**
     * @brief Close the disk device
     */
    virtual void close() = 0;

    /**
     * @brief Check if the disk device is open
     *
     * @return true if the disk is open, false otherwise
     */
    virtual bool isOpen() const noexcept = 0;

    /**
     * @brief Read sectors from the disk
     *
     * @param startSector Starting sector to read from (0-based)
     * @param sectorCount Number of sectors to read
     * @param buffer Buffer to store the read data
     * @return true if successful, false otherwise
     * @throws DiskException if reading fails
     */
    virtual bool readSectors(uint64_t startSector, uint64_t sectorCount, std::vector<std::byte>& buffer) = 0;

    /**
     * @brief Get information about the disk device
     *
     * @return DeviceInformation structure containing disk details
     * @throws DiskException if querying fails
     */
    virtual DeviceInformation getDiskInfo() const = 0;

    /**
     * @brief Get the sector size of the disk (usually 512 or 4096 bytes)
     *
     * @return Sector size in bytes
     */
    virtual uint32_t getSectorSize() const noexcept = 0;

    /**
     * @brief Get the total number of sectors on the disk
     *
     * @return Total number of sectors
     */
    virtual uint64_t getTotalSectors() const noexcept = 0;
};

} // namespace disk
} // namespace recoverysuite