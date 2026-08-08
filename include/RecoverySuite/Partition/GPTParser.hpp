#pragma once

#include <memory>
#include <vector>
#include <array>
#include "RecoverySuite/Partition/GPTHeader.hpp"
#include "RecoverySuite/Partition/GPTPartitionTable.hpp"
#include "IDiskReader.hpp"

namespace recoverysuite {
namespace partition {

/**
 * @brief Parser for GPT header and partition entry array
 */
class GPTParser {
public:
    explicit GPTParser(std::shared_ptr<recoverysuite::disk::IDiskReader> reader);

    /**
     * @brief Parse the GPT from disk
     * @return Pair of (GPTHeader, GPTPartitionTable)
     * @throws PartitionException on read or parsing errors
     */
    std::pair<GPTHeader, GPTPartitionTable> parseGPT() const;

    /**
     * @brief Check if the disk has a protective MBR (indicating GPT)
     * @return True if protective MBR is found
     */
    bool hasProtectiveMBR() const;

private:
    std::shared_ptr<recoverysuite::disk::IDiskReader> m_reader;

    /**
     * @brief Read a sector from disk into a buffer
     * @param lba Logical block address to read
     * @param buffer Buffer to receive the data (must be at least sectorSize bytes)
     */
    void readSector(uint64_t lba, std::vector<std::byte>& buffer) const;

    /**
     * @brief Read multiple sectors from disk
     * @param startLba Starting LBA
     * @param sectorCount Number of sectors to read
     * @param buffer Buffer to receive the data
     */
    void readSectors(uint64_t startLba, uint32_t sectorCount, std::vector<std::byte>& buffer) const;
};

} // namespace partition
} // namespace recoverysuite