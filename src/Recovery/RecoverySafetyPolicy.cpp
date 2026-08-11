#include "RecoverySafetyPolicy.h"
#include <sstream>

namespace recoverysuite {
namespace recovery {

RecoverySafetyPolicy::RecoverySafetyPolicy() {
    // Default constructor
}

bool RecoverySafetyPolicy::isCapabilitySupported(
    RecoveryCapability requestedCapability,
    const RecoveryCapabilityRegistry& capabilityRegistry
) const {
    return capabilityRegistry.isCapabilityRegistered(requestedCapability);
}

bool RecoverySafetyPolicy::validateSafety(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    RecoveryCapability requestedCapability,
    const RecoveryCapabilityRegistry& capabilityRegistry,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid
) const {
    // Check all safety conditions
    bool capabilitySupported = isCapabilitySupported(requestedCapability, capabilityRegistry);

    return sourceIsReadOnly &&
           destinationDiffersFromSource &&
           capabilitySupported &&
           sourceRangesAreValid &&
           outputPathIsValid &&
           arithmeticCannotOverflow &&
           cancellationStateIsValid;
}

std::string RecoverySafetyPolicy::getSafetyCheckDescription(const SafetyChecks& checks) const {
    std::stringstream ss;
    ss << "Safety Checks: "
       << "sourceIsReadOnly=" << (checks.sourceIsReadOnly ? "PASS" : "FAIL") << ", "
       << "destinationDiffersFromSource=" << (checks.destinationDiffersFromSource ? "PASS" : "FAIL") << ", "
       << "capabilityIsSupported=" << (checks.capabilityIsSupported ? "PASS" : "FAIL") << ", "
       << "sourceRangesAreValid=" << (checks.sourceRangesAreValid ? "PASS" : "FAIL") << ", "
       << "outputPathIsValid=" << (checks.outputPathIsValid ? "PASS" : "FAIL") << ", "
       << "arithmeticCannotOverflow=" << (checks.arithmeticCannotOverflow ? "PASS" : "FAIL") << ", "
       << "cancellationStateIsValid=" << (checks.cancellationStateIsValid ? "PASS" : "FAIL");
    return ss.str();
}

} // namespace recovery
} // namespace recoverysuite