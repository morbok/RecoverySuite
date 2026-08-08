#include "RecoverySuite/Partition/GPTParser.hpp"
#include "RecoverySuite/Partition/GPTValidator.hpp"
#include "RecoverySuite/Partition/PartitionException.hpp"
#include "DiskExceptions.h"
#include <cstring>
#include <vector>
#include <stdexcept>

namespace recoverysuite {
namespace partition {

GPTParser::GPTParser(std::shared_ptr<recoverysuite::disk::IDiskReader> reader)
    : m_reader(std::move(reader))
{
}

std::pair<GPTHeader, GPTPartitionTable> GPTParser::parseGPT() const {
    if (!m_reader) {
        throw PartitionException("Disk reader is null");
    }

    // Ensure the disk is open
    if (!m_reader->isOpen()) {
        throw PartitionException("Disk is not open");
    }

    // Get sector size
    uint32_t sectorSize = m_reader->getSectorSize();
    if (sectorSize < 512) {
        throw PartitionException("Sector size too small for GPT");
    }

    // Step 1: Check for protective MBR (optional, but we can do it)
    (void)hasProtectiveMBR(); // suppress unused warning

    // Step 2: Read primary GPT header from LBA 1
    std::vector<std::byte> primaryHeaderBuffer(sectorSize);
    readSector(1, primaryHeaderBuffer);

    // Copy the buffer into a GPTHeader structure (assuming the header is at the start of the sector)
    GPTHeader primaryHeader{};
    std::memcpy(&primaryHeader, primaryHeaderBuffer.data(), sizeof(GPTHeader));

    // Try to validate the primary header
    GPTValidator validator;
    bool primaryValid = false;
    try {
        validator.validateHeader(primaryHeader);
        primaryValid = true;
    } catch (const PartitionException&) {
        // Primary header invalid, we'll try backup
    }

    GPTHeader headerToUse = primaryHeader;

    if (!primaryValid) {
        // Try to read backup GPT header from the last LBA
        uint64_t totalSectors = m_reader->getTotalSectors();
        if (totalSectors < 2) {
            throw PartitionException("Disk too small for GPT");
        }
        uint64_t backupLBA = totalSectors - 1;

        std::vector<std::byte> backupHeaderBuffer(sectorSize);
        readSector(backupLBA, backupHeaderBuffer);

        GPTHeader backupHeader{};
        std::memcpy(&backupHeader, backupHeaderBuffer.data(), sizeof(GPTHeader));

        try {
            validator.validateHeader(backupHeader);
            headerToUse = backupHeader;
        } catch (const PartitionException&) {
            throw PartitionException("Both primary and backup GPT headers are invalid");
        }
    }

    // Now validate the entry array using the header we selected
    uint64_t entryArrayLBA = headerToUse.partitionEntryLBA;
    uint32_t numEntries = headerToUse.numPartitionEntries;
    uint32_t entrySize = headerToUse.sizeofPartitionEntry;

    // Calculate total bytes needed for entry array
    uint64_t entryArrayBytes = static_cast<uint64_t>(numEntries) * entrySize;
    if (entryArrayBytes == 0 || numEntries == 0) {
        throw PartitionException("GPT has zero partition entries");
    }

    // Read the entry array
    std::vector<std::byte> entryArrayBuffer;
    entryArrayBuffer.resize(static_cast<size_t>(entryArrayBytes));
    readSectors(entryArrayLBA, static_cast<uint32_t>((entryArrayBytes + sectorSize - 1) / sectorSize),
                entryArrayBuffer);

    // Trim buffer to exact size (in case we read extra sector)
    entryArrayBuffer.resize(static_cast<size_t>(entryArrayBytes));

    // Validate the entry array
    validator.validateEntryArray(headerToUse, entryArrayBuffer);

    // Build the partition table from the entry array data
    std::vector<GPTPartitionEntry> entries;
    entries.reserve(numEntries);
    for (uint32_t i = 0; i < numEntries; ++i) {
        size_t offset = i * entrySize;
        std::array<std::byte, GPTPartitionEntry::SIZE> entryData{};
        std::memcpy(entryData.data(), entryArrayBuffer.data() + offset, entrySize);
        entries.emplace_back(entryData);
    }

    GPTPartitionTable partitionTable(std::move(entries));

    return {headerToUse, partitionTable};
}

bool GPTParser::hasProtectiveMBR() const {
    if (!m_reader || !m_reader->isOpen()) {
        return false;
    }

    uint32_t sectorSize = m_reader->getSectorSize();
    if (sectorSize < 512) {
        return false;
    }

    // Read sector 0
    std::vector<std::byte> mbrBuffer(sectorSize);
    readSector(0, mbrBuffer);

    // Check for MBR signature (0x55AA) at offset 510
    if (mbrBuffer.size() < 512) {
        return false;
    }
    if (mbrBuffer[510] != std::byte{0xAA} || mbrBuffer[511] != std::byte{0x55}) {
        return false;
    }

    // Now check if there is a single partition entry of type 0xEE (protective MBR)
    // The partition table starts at offset 446, each entry is 16 bytes, there are 4 entries.
    // We'll look for an entry with boot indicator 0x80 or 0x00? Actually, protective MBR typically
    // has a single partition covering the entire disk with type 0xEE.
    // We'll just check if any of the four entries has type 0xEE.
    for (int i = 0; i < 4; ++i) {
        size_t offset = 446 + i * 16;
        if (offset + 16 > mbrBuffer.size()) break;
        if (mbrBuffer[offset + 4] == std::byte{0xEE}) { // partition type byte
            return true;
        }
    }

    return false;
}

void GPTParser::readSector(uint64_t lba, std::vector<std::byte>& buffer) const {
    if (!m_reader) {
        throw PartitionException("Disk reader is null");
    }
    uint32_t sectorSize = m_reader->getSectorSize();
    buffer.resize(sectorSize);
    uint64_t sectorsRead = m_reader->readSectors(lba, 1, buffer);
    if (sectorsRead != 1) {
        throw PartitionException("Failed to read sector");
    }
}

void GPTParser::readSectors(uint64_t startLba, uint32_t sectorCount, std::vector<std::byte>& buffer) const {
    if (!m_reader) {
        throw PartitionException("Disk reader is null");
    }
    buffer.resize(sectorCount * m_reader->getSectorSize());
    uint64_t sectorsRead = m_reader->readSectors(startLba, sectorCount, buffer);
    if (sectorsRead != sectorCount) {
        throw PartitionException("Failed to read sectors");
    }
}

} // namespace partition
} // namespace recoverysuite