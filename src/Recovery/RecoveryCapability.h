#ifndef RECOVERYSUITE_RECOVERY_RECOVERYCAPABILITY_H
#define RECOVERYSUITE_RECOVERY_RECOVERYCAPABILITY_H

#include <cstdint>
#include <string>

namespace recoverysuite {
namespace recovery {

/**
 * RecoveryCapability
 * Defines the types of recovery capabilities that can be supported
 */
enum class RecoveryCapability : uint32_t {
    NONE = 0x00000000,

    // Filesystem detection capabilities
    FILESYSTEM_DETECTION = 0x00000001,

    // Filesystem analysis capabilities
    FILESYSTEM_ANALYSIS = 0x00000002,

    // Metadata recovery capabilities
    METADATA_RECOVERY = 0x00000004,

    // File recovery capabilities
    FILE_RECOVERY = 0x00000008,

    // Carving capabilities
    CARVING = 0x00000010,

    // Output/export capabilities
    OUTPUT_EXPORT = 0x00000020
};

// Enable bitwise operations on RecoveryCapability
inline RecoveryCapability operator|(RecoveryCapability a, RecoveryCapability b) {
    return static_cast<RecoveryCapability>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

inline RecoveryCapability operator&(RecoveryCapability a, RecoveryCapability b) {
    return static_cast<RecoveryCapability>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
}

inline RecoveryCapability operator^(RecoveryCapability a, RecoveryCapability b) {
    return static_cast<RecoveryCapability>(
        static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b)
    );
}

inline RecoveryCapability operator~(RecoveryCapability a) {
    return static_cast<RecoveryCapability>(~static_cast<uint32_t>(a));
}

inline RecoveryCapability& operator|=(RecoveryCapability& a, RecoveryCapability b) {
    a = a | b;
    return a;
}

inline RecoveryCapability& operator&=(RecoveryCapability& a, RecoveryCapability b) {
    a = a & b;
    return a;
}

inline RecoveryCapability& operator^=(RecoveryCapability& a, RecoveryCapability b) {
    a = a ^ b;
    return a;
}

/**
 * Convert RecoveryCapability to string for logging/debugging
 */
std::string recoveryCapabilityToString(RecoveryCapability capability);

/**
 * RecoveryCapabilityRegistry
 * Registry for tracking available recovery capabilities
 */
class RecoveryCapabilityRegistry {
public:
    // Constructor
    RecoveryCapabilityRegistry();

    // Destructor
    ~RecoveryCapabilityRegistry() = default;

    // Register a capability
    void registerCapability(RecoveryCapability capability);

    // Unregister a capability
    void unregisterCapability(RecoveryCapability capability);

    // Check if a capability is registered
    bool isCapabilityRegistered(RecoveryCapability capability) const;

    // Get all registered capabilities
    RecoveryCapability getRegisteredCapabilities() const;

    // Clear all registered capabilities
    void clearCapabilities();

private:
    // Currently registered capabilities
    RecoveryCapability registeredCapabilities_ = RecoveryCapability::NONE;
};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_RECOVERYCAPABILITY_H