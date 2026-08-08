#pragma once

#include <memory>
#include <utility>
#include "MBRHeader.hpp"
#include "PartitionTable.hpp"
#include <IDiskReader.hpp>

namespace recoverysuite {
namespace partition {

/**
 * @brief MBR parser interface
 *
 * Parses the Master Boot Record from a disk sector.
 */
class MBParser {
public:
    explicit MBParser(std::shared_ptr<recoverysuite::disk::IDiskReader> reader);
    virtual ~MBParser() = default;

    /**
     * Parse MBR from disk sector 0
     * @return Pair of (MBRHeader, PartitionTable)
     * @throws PartitionException on parsing errors
     */
    virtual std::pair<MBRHeader, PartitionTable> parseMBR() const;

protected:
    std::shared_ptr<recoverysuite::disk::IDiskReader> m_reader;
};

} // namespace partition
} // namespace recoverysuite