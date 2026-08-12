#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <string>
#include "DeviceInformation.h"
#include "DriveGeometry.h"
#include "IORequest.h"
#include "DiskExceptions.h"
#include "Core/Version.h"
#include "../IDiskReader.hpp"

namespace recoverysuite {
namespace disk {

/**
 * @brief Class representing a physical disk device
 *
 * Provides low-level access to a physical disk device for reading and writing sectors.
 * This class handles the platform-specific details of disk access while providing
 * a clean, consistent interface.
 */
class PhysicalDisk : public recoverysuite::disk::IDiskReader {
public:
    /**
     * @brief Open a physical disk
     *
     * @param diskNumber The physical disk number to open (0-based)
     * @param readOnly Whether to open the disk in read-only mode (default: true)
     * @throws AccessDeniedException if access is denied
     * @throws DiskNotFoundException if disk is not found
     * @throws DiskException if opening fails
     */
    explicit PhysicalDisk(uint64_t diskNumber, bool readOnly = true);

    /**
     * @brief Destructor - closes the disk handle
     */
    ~PhysicalDisk();

    // Disable copy/move - PhysicalDisk manages a resource that shouldn't be duplicated
    PhysicalDisk(const PhysicalDisk&) = delete;
    PhysicalDisk& operator=(const PhysicalDisk&) = delete;
    PhysicalDisk(PhysicalDisk&&) = delete;
    PhysicalDisk& operator=(PhysicalDisk&&) = delete;

    // IDiskReader interface implementation
    /**
     * @brief Open the disk device for reading
     *
     * @param devicePath Path to the disk device (e.g., "\\\\.\\PhysicalDrive0" on Windows, "/dev/sda" on Linux)
     * @throws DiskException if opening fails
     */
    void open(const std::string& devicePath) override;

    /**
     * @brief Close the disk device
     */
    void close() override;

    /**
     * @brief Check if the disk device is open
     *
     * @return true if the disk is open, false otherwise
     */
    bool isOpen() const noexcept override;

    /**
     * @brief Get information about the disk device
     *
     * @return DeviceInformation structure containing disk details
     * @throws DiskException if querying fails
     */
    DeviceInformation getDiskInfo() const override;

    /**
     * @brief Get the sector size of the disk (usually 512 or 4096 bytes)
     *
     * @return Sector size in bytes
     */
    uint32_t getSectorSize() const noexcept override;

    /**
     * @brief Get the total number of sectors on the disk
     *
     * @return Total number of sectors
     */
    uint64_t getTotalSectors() const noexcept override;

    /**
     * @brief Read sectors from the disk
     *
     * @param startSector Starting sector to read from (0-based)
     * @param sectorCount Number of sectors to read
     * @param buffer Buffer to store the read data
     * @return true if successful, false otherwise
     * @throws DiskException if reading fails
     */
    bool readSectors(uint64_t startSector, uint64_t sectorCount, std::vector<std::byte>& buffer) override;

    // Additional methods for backward compatibility and extended functionality
    /**
     * @brief Get the disk number
     *
     * @return The physical disk number
     */
    uint64_t getDiskNumber() const;

    /**
     * @brief Get whether the disk is opened in read-only mode
     *
     * @return True if read-only, false otherwise
     */
    bool isReadOnly() const;

    /**
     * @brief Get detailed information about the disk
     *
     * @return DeviceInformation structure with disk details
     * @throws DiskException if query fails
     */
    DeviceInformation getDeviceInformation() const;

    /**
     * @brief Get the drive geometry of the disk
     *
     * @return DriveGeometry structure with cylinder/head/sector info
     * @throws DiskException if query fails
     */
    DriveGeometry getDriveGeometry() const;

    /**
     * @brief Write sectors to the disk
     *
     * @note This function will only work if the disk was opened in read/write mode
     *
     * @param startSector The starting sector to write to (0-based)
     * @param sectorCount Number of sectors to write
     * @param buffer Buffer containing the data to write
     * @return true if successful, false otherwise
     * @throws AccessDeniedException if disk was opened read-only
     * @throws DiskException if write fails
     * @throws InvalidParameterException if parameters are invalid
     */
    bool writeSectors(uint64_t startSector, uint64_t sectorCount, const std::vector<std::byte>& buffer) override;

    /**
     * @brief Get the disk's sector size in bytes
     *
     * @return Sector size in bytes (typically 512 or 4096)
     */
    uint32_t getBytesPerSector() const;

    /**
     * @brief Get the total size of the disk in bytes
     *
     * @return Total size in bytes
     */
    uint64_t getTotalSizeBytes() const;

private:
    class PhysicalDiskImpl;
    std::unique_ptr<PhysicalDiskImpl> pImpl;
};

} // namespace disk
} // namespace recoverysuite