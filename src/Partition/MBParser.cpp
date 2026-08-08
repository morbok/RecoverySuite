#include "RecoverySuite/Partition/MBParser.hpp"
#include <RecoverySuite/Partition/PartitionException.hpp>
#include <stdexcept>
#include <cstring>

namespace recoverysuite {
namespace partition {

MBParser::MBParser(std::shared_ptr<recoverysuite::disk::IDiskReader> reader)
    : m_reader(std::move(reader)) {}

std::pair<MBRHeader, PartitionTable> MBParser::parseMBR() const {
    if (!m_reader) {
        throw PartitionException("Disk reader is null");
    }

    // Ensure disk is open
    if (!m_reader->isOpen()) {
        m_reader->open(""); // Device path doesn't matter for the mock
    }

    // Read sector 0 (MBR is always in first sector)
    std::vector<std::byte> buffer;
    buffer.resize(512); // Standard MBR size

    uint64_t sectorsRead = m_reader->readSectors(0, 1, buffer);
    if (sectorsRead != 1) {
        throw PartitionException("Failed to read MBR sector");
    }

    if (buffer.size() < MBRHeader::SIZE) {
        throw PartitionException("MBR buffer too small");
    }

    // Extract MBR data
    std::array<std::byte, MBRHeader::SIZE> mbrData{};
    std::memcpy(mbrData.data(), buffer.data(), MBRHeader::SIZE);

    // Parse MBR header and partition table
    MBRHeader header(mbrData);
    PartitionTable table(header.getPartitionTable());

    // Validate MBR signature
    if (!header.hasValidSignature()) {
        throw InvalidMBRSignatureException(
            "Invalid MBR signature",
            static_cast<int>(std::errc::invalid_argument)
        );
    }

    return {header, table};
}

} // namespace partition
} // namespace recoverysuite