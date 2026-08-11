#ifndef RECOVERYSUITE_RECOVERY_RECOVERYSAFETYPOLICY_H
#define RECOVERYSUITE_RECOVERY_RECOVERYSAFETYPOLICY_H

#include <cstdint>
#include <string>
#include "RecoveryCapability.h"

namespace recoverysuite {
namespace recovery {

/**
 * RecoverySafetyPolicy
 * Defines safety policies that must be checked before recovery operations
 */
class RecoverySafetyPolicy {
public:
    // Constructor
    RecoverySafetyPolicy();

    // Destructor
    ~RecoverySafetyPolicy() = default;

    // Safety validation checks
    struct SafetyChecks {
        bool sourceIsReadOnly = false;
        bool destinationDiffersFromSource = false;
        bool capabilityIsSupported = false;
        bool sourceRangesAreValid = false;
        bool outputPathIsValid = false;
        bool arithmeticCannotOverflow = false;
        bool cancellationStateIsValid = false;
    };

    // Validate safety policies before a recovery operation
    // Returns true if all safety checks pass
    bool validateSafety(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        RecoveryCapability requestedCapability,
        const RecoveryCapabilityRegistry& capabilityRegistry,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid
    ) const;

    // Get a description of which safety checks failed
    std::string getSafetyCheckDescription(const SafetyChecks& checks) const;

public:
    // Helper to check if a capability is supported by the registry
    bool isCapabilitySupported(
        RecoveryCapability requestedCapability,
        const RecoveryCapabilityRegistry& capabilityRegistry
    ) const;
};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_RECOVERYSAFETYPOLICY_H