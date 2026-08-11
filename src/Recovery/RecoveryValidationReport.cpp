#include "RecoveryValidationReport.h"
#include <sstream>

namespace recoverysuite {
namespace recovery {

RecoveryValidationReport::RecoveryValidationReport() {
    // Default constructor
}

void RecoveryValidationReport::addValidationError(
    ValidationError::Type type,
    const std::string& description,
    RecoveryCapability relatedCapability
) {
    validationErrors_.push_back({type, description, relatedCapability});
}

bool RecoveryValidationReport::validationPassed() const {
    return validationErrors_.empty();
}

const std::vector<RecoveryValidationReport::ValidationError>& RecoveryValidationReport::getValidationErrors() const {
    return validationErrors_;
}

std::vector<RecoveryValidationReport::ValidationError> RecoveryValidationReport::getValidationErrorsForCapability(
    RecoveryCapability capability
) const {
    std::vector<ValidationError> result;
    for (const auto& error : validationErrors_) {
        if (error.relatedCapability == capability ||
            error.relatedCapability == RecoveryCapability::NONE ||
            capability == RecoveryCapability::NONE) {
            result.push_back(error);
        }
    }
    return result;
}

void RecoveryValidationReport::clearErrors() {
    validationErrors_.clear();
}

std::string RecoveryValidationReport::getSummary() const {
    if (validationPassed()) {
        return "Validation PASSED";
    }

    std::stringstream ss;
    ss << "Validation FAILED (" << validationErrors_.size() << " error(s)):\n";

    for (size_t i = 0; i < validationErrors_.size(); ++i) {
        const auto& error = validationErrors_[i];
        ss << "  " << (i + 1) << ". ";

        switch (error.type) {
            case ValidationError::Type::SOURCE_NOT_READONLY:
                ss << "Source is not read-only";
                break;
            case ValidationError::Type::DESTINATION_SAME_AS_SOURCE:
                ss << "Destination is the same as source";
                break;
            case ValidationError::Type::UNSUPPORTED_CAPABILITY:
                ss << "Unsupported capability requested";
                break;
            case ValidationError::Type::INVALID_SOURCE_RANGES:
                ss << "Invalid source ranges";
                break;
            case ValidationError::Type::INVALID_OUTPUT_PATH:
                ss << "Invalid output path";
                break;
            case ValidationError::Type::ARITHMETIC_OVERFLOW_RISK:
                ss << "Arithmetic overflow risk";
                break;
            case ValidationError::Type::INVALID_CANCELLATION_STATE:
                ss << "Invalid cancellation state";
                break;
            case ValidationError::Type::CAPABILITY_NOT_INITIALIZED:
                ss << "Capability not initialized";
                break;
        }

        if (!error.description.empty()) {
            ss << ": " << error.description;
        }
        ss << "\n";
    }

    return ss.str();
}

} // namespace recovery
} // namespace recoverysuite