#include "RecoveryOperationValidator.h"
#include <stdexcept>

namespace recoverysuite {
namespace recovery {

RecoveryOperationValidator::RecoveryOperationValidator(
    const RecoveryCapabilityRegistry& capabilityRegistry,
    const RecoverySafetyPolicy& safetyPolicy
)
    : capabilityRegistry_(capabilityRegistry),
      safetyPolicy_(safetyPolicy) {
}

RecoveryValidationReport RecoveryOperationValidator::validateOperation(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    RecoveryCapability requestedCapability,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid
) const {
    RecoveryValidationReport report;

    // Create SafetyChecks object with individual results for detailed reporting
    RecoverySafetyPolicy::SafetyChecks checks;
    checks.sourceIsReadOnly = sourceIsReadOnly;
    checks.destinationDiffersFromSource = destinationDiffersFromSource;
    checks.sourceRangesAreValid = sourceRangesAreValid;
    checks.outputPathIsValid = outputPathIsValid;
    checks.arithmeticCannotOverflow = arithmeticCannotOverflow;
    checks.cancellationStateIsValid = cancellationStateIsValid;
    checks.capabilityIsSupported = safetyPolicy_.isCapabilitySupported(
        requestedCapability, capabilityRegistry_);

    // Add validation errors for failed checks
    if (!checks.sourceIsReadOnly) {
        report.addValidationError(
            RecoveryValidationReport::ValidationError::Type::SOURCE_NOT_READONLY,
            "Source media must be opened in read-only mode to prevent accidental modification",
            requestedCapability
        );
    }

    if (!checks.destinationDiffersFromSource) {
        report.addValidationError(
            RecoveryValidationReport::ValidationError::Type::DESTINATION_SAME_AS_SOURCE,
            "Output destination must differ from source to prevent overwriting source data",
            requestedCapability
        );
    }

    if (!checks.capabilityIsSupported) {
        report.addValidationError(
            RecoveryValidationReport::ValidationError::Type::UNSUPPORTED_CAPABILITY,
            "Requested capability is not supported by the system",
            requestedCapability
        );
    }

    if (!checks.sourceRangesAreValid) {
        report.addValidationError(
            RecoveryValidationReport::ValidationError::Type::INVALID_SOURCE_RANGES,
            "Source ranges are invalid (out of bounds, negative, or malformed)",
            requestedCapability
        );
    }

    if (!checks.outputPathIsValid) {
        report.addValidationError(
            RecoveryValidationReport::ValidationError::Type::INVALID_OUTPUT_PATH,
            "Output path is invalid (inaccessible, read-only, or malformed)",
            requestedCapability
        );
    }

    if (!checks.arithmeticCannotOverflow) {
        report.addValidationError(
            RecoveryValidationReport::ValidationError::Type::ARITHMETIC_OVERFLOW_RISK,
            "Arithmetic overflow risk detected in operation parameters",
            requestedCapability
        );
    }

    if (!checks.cancellationStateIsValid) {
        report.addValidationError(
            RecoveryValidationReport::ValidationError::Type::INVALID_CANCELLATION_STATE,
            "Cancellation state is invalid (corrupted or inconsistent)",
            requestedCapability
        );
    }

    return report;
}

void RecoveryOperationValidator::validateOperationOrThrow(
    bool sourceIsReadOnly,
    bool destinationDiffersFromSource,
    RecoveryCapability requestedCapability,
    bool sourceRangesAreValid,
    bool outputPathIsValid,
    bool arithmeticCannotOverflow,
    bool cancellationStateIsValid
) const {
    RecoveryValidationReport report = validateOperation(
        sourceIsReadOnly,
        destinationDiffersFromSource,
        requestedCapability,
        sourceRangesAreValid,
        outputPathIsValid,
        arithmeticCannotOverflow,
        cancellationStateIsValid
    );

    if (!report.validationPassed()) {
        throw std::runtime_error("Recovery operation validation failed: " + report.getSummary());
    }
}

} // namespace recovery
} // namespace recoverysuite