#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <vector>
#include "RecoverySuite/Partition/PartitionException.hpp"

namespace recoverysuite {
namespace partition {

/**
 * @brief GPT Partition Entry structure (as defined by UEFI specification)
 *
 * Each partition entry is typically 128 bytes.
 */
class GPTPartitionEntry {
public:
    static constexpr size_t SIZE = 128;
    static constexpr size_t NAME_LENGTH = 72; // UTF-16 characters, 72*2=144 bytes

    GPTPartitionEntry() = default;
    explicit GPTPartitionEntry(const std::array<std::byte, SIZE>& data);

    // Returns true if the entry is unused (all zeros or type GUID is zero)
    bool isEmpty() const noexcept;

    // Get the partition type GUID (16 bytes)
    std::array<uint8_t, 16> getPartitionTypeGUID() const noexcept;

    // Get the unique partition GUID (16 bytes)
    std::array<uint8_t, 16> getUniquePartitionGUID() const noexcept;

    // Get the starting LBA (little-endian)
    uint64_t getStartingLBA() const noexcept;

    // Get the ending LBA (inclusive, little-endian)
    uint64_t getEndingLBA() const noexcept;

    // Get the attributes (little-endian)
    uint64_t getAttributes() const noexcept;

    // Get the partition name (UTF-16 string)
    std::u16string getName() const noexcept;

private:
    std::array<std::byte, SIZE> m_data{};
};

} // namespace partition
} // namespace recoverysuite