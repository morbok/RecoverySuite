#include "RecoverySuite/Partition/GPTValidator.hpp"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <iostream>

namespace recoverysuite {
namespace partition {

void GPTValidator::validateHeader(const GPTHeader& header) const {
    if (!header.hasValidSignature()) {
        throw InvalidGPTSignatureException("Invalid GPT signature");
    }

    constexpr uint32_t kMinimumHeaderSize = 92;
    if (header.headerSize < kMinimumHeaderSize || header.headerSize > sizeof(GPTHeader)) {
        throw InvalidGPTCRCException("Unsupported GPT header size");
    }
    std::array<std::byte, sizeof(GPTHeader)> headerBytes{};
    std::memcpy(headerBytes.data(), &header, sizeof(GPTHeader));
    std::fill(headerBytes.begin() + 16, headerBytes.begin() + 20, std::byte{0});
    const uint32_t crc = computeCRC32(headerBytes.data(), header.headerSize);
    if (crc != header.headerCRC32) {
        throw InvalidGPTCRCException("Invalid GPT header CRC");
    }
}

void GPTValidator::validateEntryArray(const GPTHeader& header,
                                      const std::vector<std::byte>& entryArrayData) const {
    // Compute CRC32 of the entry array
    uint32_t crc = computeCRC32(entryArrayData.data(), entryArrayData.size());
    if (crc != header.partitionEntryArrayCRC32) {
        throw InvalidGPTEntryArrayCRCException("Invalid GPT partition entry array CRC");
    }

    // Additional validation: check LBA ranges and overlapping partitions
    // We'll parse the entries from the raw data
    size_t entrySize = header.sizeofPartitionEntry;
    size_t numEntries = header.numPartitionEntries;

    if (entrySize < GPTPartitionEntry::SIZE || entrySize % GPTPartitionEntry::SIZE != 0 ||
        numEntries == 0 || numEntries > entryArrayData.size() / entrySize) {
        throw InvalidGPTLBAException("GPT partition entry size too small");
    }

    // We'll create a vector of GPTPartitionEntry from the raw data
    std::vector<GPTPartitionEntry> entries;
    entries.reserve(numEntries);

    for (size_t i = 0; i < numEntries; ++i) {
        size_t offset = i * entrySize;
        if (offset + entrySize > entryArrayData.size()) {
            throw InvalidGPTLBAException("GPT entry array data too small");
        }
        std::array<std::byte, GPTPartitionEntry::SIZE> entryData{};
        std::memcpy(entryData.data(), entryArrayData.data() + offset, GPTPartitionEntry::SIZE);
        entries.emplace_back(entryData);
    }

    // Check each entry for valid LBA ranges (starting LBA <= ending LBA, within disk limits?)
    // We don't have total disk size here, so we can only check that starting <= ending.
    // Also, first usable and last usable LBA from header should be respected.
    uint64_t firstUsable = header.firstUsableLBA;
    uint64_t lastUsable = header.lastUsableLBA;

    for (size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].isEmpty()) {
            uint64_t start = entries[i].getStartingLBA();
            uint64_t end = entries[i].getEndingLBA();
            if (start > end) {
                throw InvalidGPTLBAException("GPT partition start LBA greater than end LBA");
            }
            if (start < firstUsable || end > lastUsable) {
                throw InvalidGPTLBAException("GPT partition LBA out of usable range");
            }
        }
    }

    // Check for overlapping partitions (only consider non-empty entries)
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].isEmpty()) continue;
        uint64_t start1 = entries[i].getStartingLBA();
        uint64_t end1 = entries[i].getEndingLBA();
        for (size_t j = i + 1; j < entries.size(); ++j) {
            if (entries[j].isEmpty()) continue;
            uint64_t start2 = entries[j].getStartingLBA();
            uint64_t end2 = entries[j].getEndingLBA();
            // Check if ranges overlap: not (end1 < start2 || end2 < start1)
            if (!(end1 < start2 || end2 < start1)) {
                throw OverlappingGPTPartitionsException("GPT partitions overlap");
            }
        }
    }
}

uint32_t GPTValidator::computeCRC32(const void* data, size_t length) {
    // Simple CRC32 implementation (IEEE 802.3)
    // Polynomial: 0xEDB88320
    static constexpr auto crc32_table = [] {
        std::array<uint32_t, 256> table{};
        for (uint32_t i = 0; i < table.size(); ++i) {
            uint32_t crc = i;
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            table[i] = crc;
        }
        return table;
    }();
    const auto* ptr = static_cast<const std::byte*>(data);

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        uint8_t index = static_cast<uint8_t>(crc ^ static_cast<uint8_t>(ptr[i]));
        crc = (crc >> 8) ^ crc32_table[index];
        // Uncomment to see progress
        // if (i < 10) {
        //     std::cout << "i=" << i << ", byte=" << static_cast<int>(static_cast<unsigned char>(ptr[i]))
        //               << ", index=" << static_cast<int>(index) << ", crc=" << std::hex << crc << std::dec << std::endl;
        // }
    }
    uint32_t result = ~crc;
    return result;
}

} // namespace partition
} // namespace recoverysuite