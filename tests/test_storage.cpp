#include <iostream>
#include <cassert>
#include <cmath>
#include "Storage/StorageManager.h"
#include "Storage/StorageDeviceInfo.h"
#include "Storage/StorageExceptions.h"
#include "Storage/Utils/StorageUtils.h"

using namespace recoverysuite::storage;

// Mock storage manager for testing
class MockStorageManager : public StorageManager {
public:
    MockStorageManager() : StorageManager() {}

    StorageAnalysisReport analyzeStorage(uint64_t diskNumber) override {
        StorageDeviceInfo deviceInfo;
        deviceInfo.device_path = "\\\\.\\PhysicalDrive0";
        deviceInfo.interface_type = StorageInterfaceType::SATA;
        deviceInfo.vendor = "TestVendor";
        deviceInfo.model = "TestModel";
        deviceInfo.revision = "1.0";
        deviceInfo.serial_number = "TEST123";
        deviceInfo.user_capacity_bytes = 1000ULL * 1024 * 1024 * 1024; // 1 TB
        deviceInfo.sector_size_bytes = 512;
        deviceInfo.form_factor = "2.5\"";
        deviceInfo.is_removable = false;
        deviceInfo.block_size_bytes = 512;
        deviceInfo.nand_capacity_bytes = 1200ULL * 1024 * 1024 * 1024; // 1.2 TB

        StorageHealthData healthData;
        healthData.critical_warning = 0;
        healthData.temperature_kelvin = 298; // 25°C
        healthData.available_spare_percent = 80;
        healthData.spare_threshold_percent = 10;
        healthData.percentage_used = 50;
        healthData.data_read_gb = 1000;
        healthData.data_written_gb = 2000;
        healthData.host_read_commands = 10000;
        healthData.host_write_commands = 20000;
        healthData.power_cycles = 50;
        healthData.power_on_hours = 1000;
        healthData.unsafe_shutdowns = 2;
        healthData.media_errors = 0;
        healthData.error_log_entries = 0;

        StorageAnalysisReport report;
        report.device_info = deviceInfo;
        report.trim_status = TRIMStatus::ENABLED;
        report.trim_confidence = 0.95;
        report.gc_aggressiveness = GarbageCollectionAggressiveness::MEDIUM;
        report.gc_confidence = 0.90;
        report.wear_leveling = WearLevelingQuality::GOOD;
        report.wear_confidence = 0.85;
        report.estimated_overprovisioning_pct = 16.7; // (1.2TB - 1TB) / 1.2TB
        report.write_amplification_factor = 2.0;
        report.health_data = healthData;
        report.vendor_specific_notes = "Test vendor notes";
        report.analysis_timestamp = 1234567890;
        report.overall_health_summary = "Test health summary";
        report.recoverability_estimate = 75.0;

        return report;
    }

    StorageDeviceInfo getStorageInfo(uint64_t diskNumber) override {
        StorageDeviceInfo deviceInfo;
        deviceInfo.device_path = "\\\\.\\PhysicalDrive0";
        deviceInfo.interface_type = StorageInterfaceType::SATA;
        deviceInfo.vendor = "TestVendor";
        deviceInfo.model = "TestModel";
        deviceInfo.revision = "1.0";
        deviceInfo.serial_number = "TEST123";
        deviceInfo.user_capacity_bytes = 1000ULL * 1024 * 1024 * 1024; // 1 TB
        deviceInfo.sector_size_bytes = 512;
        deviceInfo.form_factor = "2.5\"";
        deviceInfo.is_removable = false;
        deviceInfo.block_size_bytes = 512;
        deviceInfo.nand_capacity_bytes = 1200ULL * 1024 * 1024 * 1024; // 1.2 TB
        return deviceInfo;
    }

    TRIMStatus checkTrimStatus(uint64_t diskNumber) override {
        return TRIMStatus::ENABLED;
    }

    WearLevelingQuality assessWearLeveling(uint64_t diskNumber) override {
        return WearLevelingQuality::GOOD;
    }

    GarbageCollectionAggressiveness assessGcAggressiveness(uint64_t diskNumber) override {
        return GarbageCollectionAggressiveness::MEDIUM;
    }
};

// Helper function to compare doubles with tolerance
bool double_equal(double a, double b, double epsilon = 0.0001) {
    return std::fabs(a - b) < epsilon;
}

// Test basic storage manager functionality
void test_basic_functionality() {
    MockStorageManager manager;

    // Test getStorageInfo
    StorageDeviceInfo info = manager.getStorageInfo(0);
    assert(info.vendor == "TestVendor");
    assert(info.model == "TestModel");
    assert(info.user_capacity_bytes == 1000ULL * 1024 * 1024 * 1024);

    // Test analyzeStorage
    StorageAnalysisReport report = manager.analyzeStorage(0);
    assert(report.device_info.vendor == "TestVendor");
    assert(report.trim_status == TRIMStatus::ENABLED);
    assert(report.gc_aggressiveness == GarbageCollectionAggressiveness::MEDIUM);
    assert(report.wear_leveling == WearLevelingQuality::GOOD);
    assert(double_equal(report.recoverability_estimate, 75.0, 0.1));

    std::cout << "����������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������✓ StorageManager basic functionality test passed" << std::endl;
}

// Test storage utilities
void test_storage_utils() {
    assert(StorageUtils::formatBytes(0) == "0.0 B");
    assert(StorageUtils::formatBytes(1024) == "1.0 KB");
    assert(StorageUtils::formatBytes(1024*1024) == "1.0 MB");
    assert(StorageUtils::formatBytes(1024*1024*1024) == "1.0 GB");

    std::cout << "����������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������✓ StorageUtils formatBytes test passed" << std::endl;
}

// Test temperature conversion
void test_temperature_conversion() {
    // 0°C = 273.15K
    assert(double_equal(StorageUtils::kelvinToCelsius(273), 0.0, 1.0));
    assert(double_equal(StorageUtils::celsiusToKelvin(0.0), 273.0, 1.0));

    // 100°C = 373.15K
    assert(double_equal(StorageUtils::kelvinToCelsius(373), 100.0, 1.0));
    assert(double_equal(StorageUtils::celsiusToKelvin(100.0), 373.0, 1.0));

    std::cout << "����������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������✓ StorageUtils temperature conversion test passed" << std::endl;
}

// Test write amplification calculation
void test_write_amplification() {
    assert(double_equal(StorageUtils::calculateWriteAmplification(1000.0, 500.0), 2.0));
    assert(double_equal(StorageUtils::calculateWriteAmplification(500.0, 1000.0), 0.5));
    assert(double_equal(StorageUtils::calculateWriteAmplification(0.0, 1000.0), 0.0));

    std::cout << "��������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������✓ StorageUtils write amplification test passed" << std::endl;
}

// Test over-provisioning estimation
void test_overprovisioning_pct() {
    // 25% over-provisioning: 128GB user, 160GB raw
    assert(double_equal(StorageUtils::estimateOverprovisioningPct(128ULL*1024*1024*1024,
                                                                160ULL*1024*1024*1024), 20.0, 0.1));

    // 0% over-provisioning: equal capacities
    assert(double_equal(StorageUtils::estimateOverprovisioningPct(1000ULL*1024*1024*1024,
                                                                 1000ULL*1024*1024*1024), 0.0));

    // Invalid case: raw capacity <= user capacity
    assert(double_equal(StorageUtils::estimateOverprovisioningPct(1000ULL*1024*1024*1024,
                                                                 500ULL*1024*1024*1024), 0.0));

    std::cout << "��������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������������✓ StorageUtils over-provisioning test passed" << std::endl;
}

int main() {
    std::cout << "Testing RecoverySuite Storage Module interface..." << std::endl;

    try {
        test_basic_functionality();
        test_storage_utils();
        test_temperature_conversion();
        test_write_amplification();
        test_overprovisioning_pct();

        std::cout << "All Storage module tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}