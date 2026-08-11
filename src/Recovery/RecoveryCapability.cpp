#include "RecoveryCapability.h"
#include <sstream>

namespace recoverysuite {
namespace recovery {

std::string recoveryCapabilityToString(RecoveryCapability capability) {
    std::stringstream ss;
    bool first = true;

    if ((capability & RecoveryCapability::FILESYSTEM_DETECTION) != RecoveryCapability::NONE) {
        if (!first) ss << " | ";
        ss << "FILESYSTEM_DETECTION";
        first = false;
    }
    if ((capability & RecoveryCapability::FILESYSTEM_ANALYSIS) != RecoveryCapability::NONE) {
        if (!first) ss << " | ";
        ss << "FILESYSTEM_ANALYSIS";
        first = false;
    }
    if ((capability & RecoveryCapability::METADATA_RECOVERY) != RecoveryCapability::NONE) {
        if (!first) ss << " | ";
        ss << "METADATA_RECOVERY";
        first = false;
    }
    if ((capability & RecoveryCapability::FILE_RECOVERY) != RecoveryCapability::NONE) {
        if (!first) ss << " | ";
        ss << "FILE_RECOVERY";
        first = false;
    }
    if ((capability & RecoveryCapability::CARVING) != RecoveryCapability::NONE) {
        if (!first) ss << " | ";
        ss << "CARVING";
        first = false;
    }
    if ((capability & RecoveryCapability::OUTPUT_EXPORT) != RecoveryCapability::NONE) {
        if (!first) ss << " | ";
        ss << "OUTPUT_EXPORT";
        first = false;
    }
    if (first) {
        ss << "NONE";
    }

    return ss.str();
}

RecoveryCapabilityRegistry::RecoveryCapabilityRegistry()
    : registeredCapabilities_(RecoveryCapability::NONE) {
    // Initially, no capabilities are registered
}

void RecoveryCapabilityRegistry::registerCapability(RecoveryCapability capability) {
    registeredCapabilities_ |= capability;
}

void RecoveryCapabilityRegistry::unregisterCapability(RecoveryCapability capability) {
    registeredCapabilities_ &= ~capability;
}

bool RecoveryCapabilityRegistry::isCapabilityRegistered(RecoveryCapability capability) const {
    return (registeredCapabilities_ & capability) != RecoveryCapability::NONE;
}

RecoveryCapability RecoveryCapabilityRegistry::getRegisteredCapabilities() const {
    return registeredCapabilities_;
}

void RecoveryCapabilityRegistry::clearCapabilities() {
    registeredCapabilities_ = RecoveryCapability::NONE;
}

} // namespace recovery
} // namespace recoverysuite