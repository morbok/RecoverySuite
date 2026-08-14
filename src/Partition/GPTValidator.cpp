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

    // Compute CRC32 of the header, setting the headerCRC32 field to 0 during computation
    std::array<std::byte, sizeof(GPTHeader)> headerBytes{};
    std::memcpy(headerBytes.data(), &header, sizeof(GPTHeader));

    // Debug: print the partitionEntryArrayCRC32 field (offset 88-91) after copying
    std::cout << "Validator: partitionEntryArrayCRC32 field (offset 88-91) after copy: ";
    for (size_t i = 0; i < 4; ++i) {
        std::cout << std::hex << static_cast<int>(static_cast<unsigned char>(headerBytes[88 + i])) << " ";
    }
    std::cout << std::dec << std::endl;

    // Print the headerBytes (first 32 bytes) after copying, before zeroing
    std::cout << "Validator: headerBytes after copy (first 32 bytes): ";
    for (size_t i = 0; i < 32 && i < headerBytes.size(); ++i) {
        std::cout << std::hex << static_cast<int>(static_cast<unsigned char>(headerBytes[i])) << " ";
    }
    std::cout << std::dec << std::endl;

    // Zero out the headerCRC32 field (offset 0x10, 4 bytes)
    // In the GPTHeader struct, headerCRC32 is at offset 16 (after signature 8, revision 4, headerSize 4)
    // So we set bytes 16-19 to 0.
    for (size_t i = 16; i < 20; ++i) {
        headerBytes[i] = std::byte{0};
    }

    // Print the headerBytes (first 32 bytes) after zeroing
    std::cout << "Validator: headerBytes after zeroing (first 32 bytes): ";
    for (size_t i = 0; i < 32 && i < headerBytes.size(); ++i) {
        std::cout << std::hex << static_cast<int>(static_cast<unsigned char>(headerBytes[i])) << " ";
    }
    std::cout << std::dec << std::endl;
    // Compute simple sum of all bytes for debugging
    uint64_t sum = 0;
    for (size_t i = 0; i < headerBytes.size(); ++i) {
        sum += static_cast<uint64_t>(static_cast<unsigned char>(headerBytes[i]));
    }
    std::cout << "Validator headerBytes sum: " << sum << std::endl;
    // Print sums in 64-byte chunks
    for (size_t chunk = 0; chunk < headerBytes.size() / 64; ++chunk) {
        uint64_t chunkSum = 0;
        for (size_t i = 0; i < 64; ++i) {
            chunkSum += static_cast<uint64_t>(static_cast<unsigned char>(headerBytes[chunk * 64 + i]));
        }
        std::cout << "  Chunk " << chunk << " (offset " << chunk * 64 << "): " << chunkSum << std::endl;
    }
    if (headerBytes.size() % 64 != 0) {
        uint64_t chunkSum = 0;
        size_t start = (headerBytes.size() / 64) * 64;
        for (size_t i = start; i < headerBytes.size(); ++i) {
            chunkSum += static_cast<uint64_t>(static_cast<unsigned char>(headerBytes[i]));
        }
        std::cout << "  Last chunk (offset " << start << "): " << chunkSum << std::endl;
    }

    uint32_t crc = computeCRC32(headerBytes.data(), headerBytes.size());
    std::cout << "Validator computed header CRC32: 0x" << std::hex << crc << std::dec << std::endl;
    std::cout << "Header's headerCRC32: 0x" << std::hex << static_cast<unsigned int>(header.headerCRC32) << std::dec << std::endl;
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

    if (entrySize < sizeof(GPTPartitionEntry::SIZE)) {
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
        std::memcpy(entryData.data(), entryArrayData.data() + offset, entrySize);
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
    static uint32_t crc32_table[256];
    static bool table_initialized = false;
    if (!table_initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                crc = (crc >> 1) ^ (0xEDB88320 & ~((crc & 1) - 1));
            }
            crc32_table[i] = crc;
        }
        table_initialized = true;
    }

    // Debug: print first 10 bytes of data
    std::cout << "computeCRC32: data=" << data << ", length=" << length << std::endl;
    const std::byte* ptr = static_cast<const std::byte*>(data);
    std::cout << "First 10 bytes: ";
    for (size_t i = 0; i < 10 && i < length; ++i) {
        std::cout << std::hex << static_cast<int>(static_cast<unsigned char>(ptr[i])) << " ";
    }
    std::cout << std::dec << std::endl;

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
    std::cout << "computeCRC32: result=0x" << std::hex << result << std::dec << std::endl;
    return result;
}

} // namespace partition
} // namespace recoverysuite