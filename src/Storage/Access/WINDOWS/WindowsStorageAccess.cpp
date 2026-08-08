#include "WindowsStorageAccess.h"
#include <stdexcept>
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <windows.storage.h>
#include <propvarutil.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <ntddstor.h>
#include <wtypes.h>
#include <wbemidl.h>
#include <comdef.h>

namespace recoverysuite {
namespace storage {

class WindowsStorageAccess::WindowsStorageAccessImpl {
public:
    WindowsStorageAccessImpl()
        : initialized_(false), wbemServices_(nullptr) {
        // Initialize COM and WMI
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(hr)) {
            hr = CoInitializeSecurity(
                nullptr,
                -1,
                nullptr,
                nullptr,
                RPC_C_AUTHN_LEVEL_DEFAULT,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                nullptr,
                EOAC_NONE,
                nullptr
            );
            if (SUCCEEDED(hr)) {
                hr = CoCreateInstance(
                    CLSID_WbemLocator,
                    nullptr,
                    CLSCTX_INPROC_SERVER,
                    IID_IWbemLocator,
                    reinterpret_cast<void**>(&wbemLocator_)
                );
                if (SUCCEEDED(hr)) {
                    hr = wbemLocator_->ConnectServer(
                        _bstr_t(L"ROOT\\CIMV2"),
                        nullptr,
                        nullptr,
                        nullptr,
                        0,
                        nullptr,
                        nullptr,
                        &wbemServices_
                    );
                    if (SUCCEEDED(hr)) {
                        hr = CoSetProxyBlanket(
                            wbemServices_,
                            RPC_C_AUTHN_WINNT,
                            RPC_C_AUTHZ_NONE,
                            nullptr,
                            RPC_C_AUTHN_LEVEL_CALL,
                            RPC_C_IMP_LEVEL_IMPERSONATE,
                            nullptr,
                            EOAC_NONE
                        );
                        initialized_ = SUCCEEDED(hr);
                    }
                }
            }
        }
    }

    ~WindowsStorageAccessImpl() {
        if (wbemServices_) {
            wbemServices_->Release();
            wbemServices_ = nullptr;
        }
        if (wbemLocator_) {
            wbemLocator_->Release();
            wbemLocator_ = nullptr;
        }
        CoUninitialize();
    }

    bool initialized_;
    IWbemLocator* wbemLocator_ = nullptr;
    IWbemServices* wbemServices_ = nullptr;

    // Helper methods
    std::string bstrToString(BSTR bstr);
    std::wstring stringToWstring(const std::string& str);
    bool queryWmiProperty(const std::wstring& className, const std::wstring& propertyName,
                         std::string& result, const std::wstring& whereClause = L"");
    uint64_t queryWmiUint64(const std::wstring& className, const std::wstring& propertyName,
                           const std::wstring& whereClause = L"");
};

WindowsStorageAccess::WindowsStorageAccess() : pImpl(std::make_unique<WindowsStorageAccessImpl>()) {}

WindowsStorageAccess::~WindowsStorageAccess() = default;

StorageDeviceInfo WindowsStorageAccess::getDeviceInfo(uint64_t diskNumber) {
    if (!pImpl->initialized_) {
        throw StorageException("Windows storage access not initialized");
    }

    StorageDeviceInfo storageInfo;

    try {
        // Construct the device path
        storageInfo.device_path = "\\\\.\\PhysicalDrive" + std::to_string(diskNumber);

        // Query WMI for disk drive information
        std::wstringstream wss;
        wss << L"SELECT * FROM Win32_DiskDrive WHERE Index = " << diskNumber;
        std::wstring query = wss.str();

        // Get model
        std::string model;
        if (pImpl->queryWmiProperty(L"Win32_DiskDrive", L"Model", model, query)) {
            storageInfo.model = model;
        }

        // Get vendor/manufacturer
        std::string manufacturer;
        if (pImpl->queryWmiProperty(L"Win32_DiskDrive", L"Manufacturer", manufacturer, query)) {
            storageInfo.vendor = manufacturer;
        }

        // Get serial number
        std::string serialNumber;
        if (pImpl->queryWmiProperty(L"Win32_DiskDrive", L"SerialNumber", serialNumber, query)) {
            storageInfo.serial_number = serialNumber;
        }

        // Get revision/firmware version
        std::string revision;
        if (pImpl->queryWmiProperty(L"Win32_DiskDrive", L"FirmwareRevision", revision, query)) {
            storageInfo.revision = revision;
        }

        // Get size
        uint64_t size = pImpl->queryWmiUint64(L"Win32_DiskDrive", L"Size", query);
        if (size > 0) {
            storageInfo.user_capacity_bytes = size;
        }

        // Get bytes per sector
        uint32_t bytesPerSector = static_cast<uint32_t>(pImpl->queryWmiUint64(L"Win32_DiskDrive", L"BytesPerSector", query));
        if (bytesPerSector > 0) {
            storageInfo.sector_size_bytes = bytesPerSector;
            storageInfo.block_size_bytes = bytesPerSector;
        }

        // Get interface type
        std::string interfaceTypeStr;
        if (pImpl->queryWmiProperty(L"Win32_DiskDrive", L"InterfaceType", interfaceTypeStr, query)) {
            std::transform(interfaceTypeStr.begin(), interfaceTypeStr.end(), interfaceTypeStr.begin(), ::toupper);
            if (interfaceTypeStr == "SATA") {
                storageInfo.interface_type = StorageInterfaceType::SATA;
            } else if (interfaceTypeStr == "NVMe" || interfaceTypeStr == "PCIe") {
                storageInfo.interface_type = StorageInterfaceType::NVMe;
            } else if (interfaceTypeStr == "USB") {
                // For USB, we need to determine if it's SATA or NVMe USB
                // For simplicity, we'll check the model for NVMe indicators
                if (storageInfo.model.find("NVMe") != std::string::npos ||
                    storageInfo.model.find("PCIe") != std::string::npos) {
                    storageInfo.interface_type = StorageInterfaceType::USB_NVMe;
                } else {
                    storageInfo.interface_type = StorageInterfaceType::USB_SATA;
                }
            } else {
                storageInfo.interface_type = StorageInterfaceType::UNKNOWN;
            }
        }

        // Get media type to determine if removable
        std::string mediaTypeStr;
        if (pImpl->queryWmiProperty(L"Win32_DiskDrive", L"MediaType", mediaTypeStr, query)) {
            std::transform(mediaTypeStr.begin(), mediaTypeStr.end(), mediaTypeStr.begin(), ::toupper);
            if (mediaTypeStr.find("REMOVABLE") != std::string::npos ||
                mediaTypeStr.find("EXTERNAL") != std::string::npos) {
                storageInfo.is_removable = true;
            }
        }

        // Get form factor from chassis types or other properties
        // For now, we'll use a simple heuristic based on size and other factors
        if (storageInfo.user_capacity_bytes > 0) {
            double sizeGB = static_cast<double>(storageInfo.user_capacity_bytes) / (1024.0 * 1024.0 * 1024.0);
            if (sizeGB < 2.0) {
                storageInfo.form_factor = "M.2";
            } else if (sizeGB < 16.0) {
                storageInfo.form_factor = "2.5\"";
            } else {
                storageInfo.form_factor = "3.5\"";
            }
        }

        // Estimate NAND capacity (typically slightly larger than user capacity for over-provisioning)
        if (storageInfo.user_capacity_bytes > 0) {
            // Assume 7% over-provisioning as a typical value for SSDs
            storageInfo.nand_capacity_bytes = static_cast<uint64_t>(storageInfo.user_capacity_bytes * 1.07);
        }

        return storageInfo;
    } catch (const std::exception& ex) {
        throw StorageException("Windows storage access failed: " + std::string(ex.what()));
    }
}

StorageHealthData WindowsStorageAccess::getHealthData(uint64_t diskNumber) {
    if (!pImpl->initialized_) {
        throw StorageException("Windows storage access not initialized");
    }

    StorageHealthData healthData{};
    // Initialize with zeros/defaults
    healthData.critical_warning = 0;
    healthData.temperature_kelvin = 0;
    healthData.available_spare_percent = 0;
    healthData.spare_threshold_percent = 0;
    healthData.percentage_used = 0;
    healthData.data_read_gb = 0;
    healthData.data_written_gb = 0;
    healthData.host_read_commands = 0;
    healthData.host_write_commands = 0;
    healthData.power_cycles = 0;
    healthData.power_on_hours = 0;
    healthData.unsafe_shutdowns = 0;
    healthData.media_errors = 0;
    healthData.error_log_entries = 0;

    try {
        // For now, return basic health data - in a full implementation, we would:
        // 1. Open the physical disk using CreateFile
        // 2. Send SMART IOCTLs (SMART_READ_DATA, etc.) to get health information
        // 3. Parse the SMART attributes according to the ATA/NVMe specifications
        // 4. Extract temperature, usage percentage, error counts, etc.

        // Since implementing full SMART support is complex and requires handling
        // different protocols (ATA, NVMe, etc.), we'll return basic information
        // that can be enhanced later

        // Try to get temperature from WMI (though this is often not reliable for disk temp)
        std::string tempStr;
        std::wstringstream wss;
        wss << L"SELECT Temperature FROM Win32_PerfFormattedData_counters_Temperature WHERE Name LIKE '%PhysicalDrive"
            << diskNumber << L"%'";
        std::wstring tempQuery = wss.str();

        if (pImpl->queryWmiProperty(L"Win32_PerfFormattedData_counters_Temperature", L"Temperature", tempStr, tempQuery)) {
            try {
                double tempDouble = std::stod(tempStr);
                // Convert from Celsius to Kelvin if needed
                if (tempDouble > 0 && tempDouble < 100) { // Likely Celsius
                    healthData.temperature_kelvin = StorageUtils::celsiusToKelvin(tempDouble);
                } else if (tempDouble > 200) { // Likely already Kelvin
                    healthData.temperature_kelvin = static_cast<uint16_t>(tempDouble);
                }
            } catch (...) {
                // Ignore parsing errors
            }
        }

        // Try to get power on hours
        std::string powerOnHoursStr;
        wss.str(L"");
        wss << L"SELECT PowerOnHours FROM Win32_PerfFormattedData_counters_DeviceBus WHERE Name LIKE '%PhysicalDrive"
            << diskNumber << L"%'";
        std::wstring powerQuery = wss.str();

        if (pImpl->queryWmiProperty(L"Win32_PerfFormattedData_counters_DeviceBus", L"PowerOnHours", powerOnHoursStr, powerQuery)) {
            try {
                healthData.power_on_hours = static_cast<uint32_t>(std::stoul(powerOnHoursStr));
            } catch (...) {
                // Ignore parsing errors
            }
        }

        // Try to get power cycle count
        std::string powerCycleCountStr;
        wss.str(L"");
        wss << L"SELECT PowerCycleCount FROM Win32_PerfFormattedData_counters_DeviceBus WHERE Name LIKE '%PhysicalDrive"
            << diskNumber << L"%'";
        std::wstring powerCycleQuery = wss.str();

        if (pImpl->queryWmiProperty(L"Win32_PerfFormattedData_counters_DeviceBus", L"PowerCycleCount", powerCycleCountStr, powerCycleQuery)) {
            try {
                healthData.power_cycles = static_cast<uint32_t>(std::stoul(powerCycleCountStr));
            } catch (...) {
                // Ignore parsing errors
            }
        }

    } catch (const std::exception& ex) {
        throw StorageException("Failed to get Windows storage health data: " + std::string(ex.what()));
    }

    return healthData;
}

uint32_t WindowsStorageAccess::sendVendorCommand(
    uint64_t diskNumber,
    const std::vector<uint8_t>& command,
    std::vector<uint8_t>& buffer) {
    try {
        // In a real implementation, we would:
        // 1. Open the physical disk using CreateFile
        // 2. Send the vendor command via DeviceIoControl with appropriate IOCTL codes
        // 3. For ATA passthrough: use IOCTL_ATA_PASS_THROUGH or IOCTL_ATA_PASS_THROUGH_DIRECT
        // 4. For NVMe passthrough: use IOCTL_NVME_PASS_THROUGH_IOCTL
        // 5. Copy the response data to the buffer
        // 6. Return the number of bytes transferred

        // For now, return 0 indicating no data transferred
        // This would be enhanced with actual command implementation

        return 0;
    } catch (const std::exception& ex) {
        throw StorageException("Failed to send Windows vendor command: " + std::string(ex.what()));
    }
}

// Helper method implementations
std::string WindowsStorageAccess::WindowsStorageAccessImpl::bstrToString(BSTR bstr) {
    if (!bstr) return "";
    int length = SysStringLen(bstr);
    std::string result(length, 0);
    std::copy(bstr, bstr + length, result.begin());
    return result;
}

std::wstring WindowsStorageAccess::WindowsStorageAccessImpl::stringToWstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), nullptr, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], size_needed);
    return wstrTo;
}

bool WindowsStorageAccess::WindowsStorageAccessImpl::queryWmiProperty(const std::wstring& className, const std::wstring& propertyName,
                                                                     std::string& result, const std::wstring& whereClause) {
    if (!wbemServices_) return false;

    std::wstring query = L"SELECT " + propertyName + L" FROM " + className;
    if (!whereClause.empty()) {
        query += L" WHERE " + whereClause;
    }

    IEnumWbemClassObject* pEnum = nullptr;
    HRESULT hr = wbemServices_->ExecQuery(
        bstr_t(L"WQL"),
        bstr_t(query.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnum
    );

    if (FAILED(hr)) {
        return false;
    }

    IWbemClassObject* pclsObj = nullptr;
    ULONG uReturn = 0;

    while (pEnum) {
        hr = pEnum->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

        if (0 == uReturn || FAILED(hr)) {
            break;
        }

        VARIANT vtProp;
        hr = pclsObj->Get(propertyName.c_str(), 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr)) {
            if (vtProp.vt == VT_BSTR) {
                result = bstrToString(vtProp.bstrVal);
            } else if (vtProp.vt == VT_UI1 || vtProp.vt == VT_I1 ||
                      vtProp.vt == VT_UI2 || vtProp.vt == VT_I2 ||
                      vtProp.vt == VT_UI4 || vtProp.vt == VT_I4 ||
                      vtProp.vt == VT_INT || vtProp.vt == VT_UINT) {
                std::stringstream ss;
                ss << vtProp.intVal;
                result = ss.str();
            } else if (vtProp.vt == VT_UI8 || vtProp.vt == VT_I8) {
                std::stringstream ss;
                ss << vtProp.llVal;
                result = ss.str();
            }
            VariantClear(&vtProp);
        }
        pclsObj->Release();
        pclsObj = nullptr;
    }

    pEnum->Release();
    pEnum = nullptr;

    return !result.empty();
}

uint64_t WindowsStorageAccess::WindowsStorageAccessImpl::queryWmiUint64(const std::wstring& className, const std::wstring& propertyName,
                                                                       const std::wstring& whereClause) {
    std::string resultStr;
    if (queryWmiProperty(className, propertyName, resultStr, whereClause)) {
        try {
            return static_cast<uint64_t>(std::stoull(resultStr));
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

TRIMStatus WindowsStorageAccess::getTrimStatus(uint64_t diskNumber) {
    // Delegate to the implementation
    if (!pImpl->initialized_) {
        throw StorageException("Windows storage access not initialized");
    }

    // For now, we'll use the basic implementation that checks if it's an SSD
    // In a full implementation, we would check actual TRIM support via IOCTL
    try {
        StorageDeviceInfo deviceInfo = this->getDeviceInfo(diskNumber);
        if (!StorageUtils::isLikelySsd(deviceInfo)) {
            return TRIMStatus::DISABLED;
        }
        // Assume TRIM is enabled for SSDs (common default in modern Windows)
        return TRIMStatus::ENABLED;
    } catch (...) {
        return TRIMStatus::UNKNOWN;
    }
}

WearLevelingQuality WindowsStorageAccess::getWearLevelingQuality(uint64_t diskNumber) {
    // Delegate to the implementation
    if (!pImpl->initialized_) {
        throw StorageException("Windows storage access not initialized");
    }

    // For now, return a basic assessment based on health data
    try {
        StorageHealthData healthData = this->getHealthData(diskNumber);
        if (healthData.percentage_used <= 20) {
            return WearLevelingQuality::EXCELLENT;
        } else if (healthData.percentage_used <= 40) {
            return WearLevelingQuality::GOOD;
        } else if (healthData.percentage_used <= 60) {
            return WearLevelingQuality::FAIR;
        } else {
            return WearLevelingQuality::POOR;
        }
    } catch (...) {
        return WearLevelingQuality::UNKNOWN;
    }
}

GarbageCollectionAggressiveness WindowsStorageAccess::getGcAggressiveness(uint64_t diskNumber) {
    // Delegate to the implementation
    if (!pImpl->initialized_) {
        throw StorageException("Windows storage access not initialized");
    }

    // For now, return a basic assessment
    try {
        StorageHealthData healthData = this->getHealthData(diskNumber);
        // Simple heuristic: higher usage might indicate more aggressive GC
        if (healthData.percentage_used <= 30) {
            return GarbageCollectionAggressiveness::LOW;
        } else if (healthData.percentage_used <= 70) {
            return GarbageCollectionAggressiveness::MEDIUM;
        } else {
            return GarbageCollectionAggressiveness::HIGH;
        }
    } catch (...) {
        return GarbageCollectionAggressiveness::UNKNOWN;
    }
}

} // namespace storage
} // namespace recoverysuite