#ifndef RECOVERYSUITE_RECOVERY_RECOVERYOPERATIONVALIDATOR_H
#define RECOVERYSUITE_RECOVERY_RECOVERYOPERATIONVALIDATOR_H

#include <cstdint>
#include <string>
#include "RecoveryCapability.h"
#include "RecoverySafetyPolicy.h"
#include "RecoveryValidationReport.h"

namespace recoverysuite {
namespace recovery {

/**
 * RecoveryOperationValidator
 * Validates recovery operations before they are executed
 */
class RecoveryOperationValidator {
public:
    // Constructor
    RecoveryOperationValidator(
        const RecoveryCapabilityRegistry& capabilityRegistry,
        const RecoverySafetyPolicy& safetyPolicy = RecoverySafetyPolicy()
    );

    // Destructor
    ~RecoveryOperationValidator() = default;

    // Validate a recovery operation
    // Returns a validation report indicating success or failure
    RecoveryValidationReport validateOperation(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        RecoveryCapability requestedCapability,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid
    ) const;

    // Validate and throw exception if validation fails
    void validateOperationOrThrow(
        bool sourceIsReadOnly,
        bool destinationDiffersFromSource,
        RecoveryCapability requestedCapability,
        bool sourceRangesAreValid,
        bool outputPathIsValid,
        bool arithmeticCannotOverflow,
        bool cancellationStateIsValid
    ) const;

private:
    // The capability registry to check against
    const RecoveryCapabilityRegistry& capabilityRegistry_;

    // The safety policy to use for validation
    const RecoverySafetyPolicy& safetyPolicy_;
};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_RECOVERYOPERATIONVALIDATOR_H