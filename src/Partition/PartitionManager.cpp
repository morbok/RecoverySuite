#include "RecoverySuite/Partition/PartitionManager.hpp"
#include "RecoverySuite/Partition/MBParser.hpp"
#include "RecoverySuite/Partition/PartitionValidator.hpp"
#include <RecoverySuite/Partition/PartitionException.hpp>
#include <stdexcept>

namespace recoverysuite {
namespace partition {

PartitionManager::PartitionManager(std::shared_ptr<recoverysuite::disk::IDiskReader> reader)
    : m_reader(std::move(reader)) {}

std::pair<MBRHeader, PartitionTable> PartitionManager::readMBR() const {
    if (!m_reader) {
        throw PartitionException("Disk reader is null");
    }

    MBParser parser(m_reader);
    return parser.parseMBR();
}

void PartitionManager::validatePartitions() const {
    if (!m_reader) {
        throw PartitionException("Disk reader is null");
    }

    // Read MBR first
    auto [header, table] = readMBR();

    // Validate the partition table
    PartitionValidator validator;
    validator.validate(table);
}

std::vector<PartitionGeometry> PartitionManager::getPartitionGeometries() const {
    if (!m_reader) {
        throw PartitionException("Disk reader is null");
    }

    // Read MBR first
    auto [header, table] = readMBR();

    std::vector<PartitionGeometry> geometries;
    geometries.reserve(table.getPartitionCount());

    // Extract geometry from each non-empty partition entry
    for (size_t i = 0; i < table.getPartitionCount(); ++i) {
        const auto& entry = table.getEntry(i);

        // Skip empty entries
        if (entry.isEmpty()) {
            continue;
        }

        PartitionGeometry geometry(
            entry.getStartLBA(),
            entry.getSectorCount(),
            entry.getPartitionType(),
            entry.isBootable()
        );

        geometries.push_back(geometry);
    }

    return geometries;
}

} // namespace partition
} // namespace recoverysuite