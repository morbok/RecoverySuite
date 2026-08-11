#ifndef RECOVERYSUITE_RECOVERY_RECOVERYVALIDATIONREPORT_H
#define RECOVERYSUITE_RECOVERY_RECOVERYVALIDATIONREPORT_H

#include <cstdint>
#include <string>
#include <vector>
#include "RecoveryCapability.h"

namespace recoverysuite {
namespace recovery {

/**
 * RecoveryValidationReport
 * Report of validation results for recovery operations
 */
class RecoveryValidationReport {
public:
    // Constructor
    RecoveryValidationReport();

    // Destructor
    ~RecoveryValidationReport() = default;

    // Validation error types
    struct ValidationError {
        enum class Type {
            SOURCE_NOT_READONLY,
            DESTINATION_SAME_AS_SOURCE,
            UNSUPPORTED_CAPABILITY,
            INVALID_SOURCE_RANGES,
            INVALID_OUTPUT_PATH,
            ARITHMETIC_OVERFLOW_RISK,
            INVALID_CANCELLATION_STATE,
            CAPABILITY_NOT_INITIALIZED
        };

        Type type;
        std::string description;
        RecoveryCapability relatedCapability;
    };

    // Add a validation error
    void addValidationError(ValidationError::Type type, const std::string& description, RecoveryCapability relatedCapability = RecoveryCapability::NONE);

    // Check if validation passed (no errors)
    bool validationPassed() const;

    // Get all validation errors
    const std::vector<ValidationError>& getValidationErrors() const;

    // Get validation errors for a specific capability
    std::vector<ValidationError> getValidationErrorsForCapability(RecoveryCapability capability) const;

    // Clear all validation errors
    void clearErrors();

    // Get a summary string of the validation report
    std::string getSummary() const;

private:
    // Validation errors
    std::vector<ValidationError> validationErrors_;
};

} // namespace recovery
} // namespace recoverysuite

#endif // RECOVERYSUITE_RECOVERY_RECOVERYVALIDATIONREPORT_H