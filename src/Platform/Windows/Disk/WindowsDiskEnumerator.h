#pragma once

#include <vector>
#include <memory>
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <iostream>
#include <sstream>
#include <string>
#include <RecoverySuite/Core/Version.h>
#include "../IDiskEnumerator.h"

namespace recoverysuite {
namespace platform {
namespace windows {

/**
 * @brief Windows-specific implementation of disk enumerator
 *
 * Uses Windows APIs like SetupDiGetClassDevs, SetupDiEnumDeviceInfo,
 * and IOCTL_STORAGE_QUERY_PROPERTY to enumerate disks and gather
 * detailed information about each disk.
 */
class WindowsDiskEnumerator : public IDiskEnumerator {
public:
    WindowsDiskEnumerator();
    ~WindowsDiskEnumerator() override;

    // Disable copy/move
    WindowsDiskEnumerator(const WindowsDiskEnumerator&) = delete;
    WindowsDiskEnumerator& operator=(const WindowsDiskEnumerator&) = delete;
    WindowsDiskEnumerator(WindowsDiskEnumerator&&) = delete;
    WindowsDiskEnumerator& operator=(WindowsDiskEnumerator&&) = delete;

    /**
     * @brief Enumerate all available disk devices
     *
     * @return Vector of DeviceInformation for each detected disk
     * @throws DiskException if enumeration fails
     */
    std::vector<recoverysuite::disk::DeviceInformation> enumerateDisks() override;

    /**
     * @brief Get information for a specific disk by number
     *
     * @param diskNumber The physical disk number to query
     * @return DeviceInformation for the specified disk
     * @throws DiskNotFoundException if disk is not found
     * @throws DiskException if query fails
     */
    recoverysuite::disk::DeviceInformation getDiskInfo(uint64_t diskNumber) override;

    /**
     * @brief Get the number of disks detected
     *
     * @return Number of disks detected
     * @throws DiskException if query fails
     */
    uint64_t getDiskCount() override;

private:
    /**
     * @brief Get detailed information for a disk using its device interface
     *
     * @param deviceInterface The device interface path
     * @param diskNumber The disk number to verify
     * @return DeviceInformation with detailed info
     */
    recoverysuite::disk::DeviceInformation getDiskDetails(const std::wstring& deviceInterface, uint64_t diskNumber);

    /**
     * @brief Query a specific property from a storage device
     *
     * @param handle Handle to the storage device
     * @param propertyId The storage property ID to query
     * @param outBuffer Buffer to receive the property data
     * @param outBufferSize Size of the output buffer
     * @return True if successful, false otherwise
     */
    bool queryStorageProperty(HANDLE handle, STORAGE_PROPERTY_ID propertyId,
                             void* outBuffer, DWORD outBufferSize);

    /**
     * @brief Get the disk number from a storage device
     *
     * @param handle Handle to the storage device
     * @return Disk number or UINT64_MAX if failed
     */
    uint64_t getDiskNumberFromHandle(HANDLE handle);

    /**
     * @brief Get the model number from a storage device
     *
     * @param handle Handle to the storage device
     * @return Model number string
     */
    std::string getModelNumber(HANDLE handle);

    /**
     * @brief Get the serial number from a storage device
     *
     * @param handle Handle to the storage device
     * @return Serial number string
     */
    std::string getSerialNumber(HANDLE handle);

    /**
     * @brief Get the bus type from a storage device
     *
     * @param handle Handle to the storage device
     * @return Bus type
     */
    recoverysuite::disk::BusType getBusType(HANDLE handle);

    /**
     * @brief Get whether the disk supports TRIM
     *
     * @param handle Handle to the storage device
     * @return True if TRIM is supported
     */
    bool supportsTrim(HANDLE handle);

    /**
     * @brief Get whether the disk is removable
     *
     * @param handle Handle to the storage device
     * @return True if removable
     */
    bool isRemovable(HANDLE handle);

    /**
     * @brief Get whether the disk is read-only
     *
     * @param handle Handle to the storage device
     * @return True if read-only
     */
    bool isReadOnly(HANDLE handle);

    // Cache of disk information to avoid repeated queries
    std::vector<recoverysuite::disk::DeviceInformation> m_diskCache;
    bool m_cacheValid = false;
};

} // namespace windows
} // namespace platform
} // namespace recoverysuite