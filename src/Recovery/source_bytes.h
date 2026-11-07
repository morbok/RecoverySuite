#pragma once
#include "Disk/IDiskReader.hpp"
#include <algorithm>
#include <limits>
#include <functional>
#include <vector>
namespace recoverysuite::recovery {
// Bounded byte extraction within the caller's selected sector range. Never trust a short read.
inline bool readSourceBytes(disk::IDiskReader* reader, uint64_t start, uint64_t count,
                            uint64_t offset, uint64_t length, std::vector<uint8_t>& output,
                            const std::function<bool()>& cancelled = {}) {
    output.clear();
    constexpr uint64_t maximumBytes = 16 * 1024 * 1024;
    if (!reader || !reader->isOpen() || !count || !length || length > maximumBytes) return false;
    const uint64_t sectorSize = reader->getSectorSize(), total = reader->getTotalSectors();
    if (sectorSize < 512 || sectorSize > 4096 || (sectorSize & (sectorSize - 1)) ||
        start >= total || count > total - start || count > std::numeric_limits<uint64_t>::max() / sectorSize)
        return false;
    const uint64_t rangeBytes = count * sectorSize;
    if (offset >= rangeBytes || length > rangeBytes - offset) return false;
    const uint64_t first = offset / sectorSize, skip = offset % sectorSize;
    const uint64_t sectors = (skip + length + sectorSize - 1) / sectorSize;
    std::vector<uint8_t> result;
    result.reserve(static_cast<size_t>(length));
    try {
        for (uint64_t consumed = 0; consumed < sectors;) {
            if (cancelled && cancelled()) return false;
            const uint64_t batch = std::min<uint64_t>(sectors - consumed, 65536 / sectorSize);
            std::vector<std::byte> bytes;
            if (!reader->readSectors(start + first + consumed, batch, bytes) || bytes.size() != batch * sectorSize)
                return false;
            const auto begin = static_cast<size_t>(consumed ? 0 : skip);
            const auto size = std::min<size_t>(bytes.size() - begin, static_cast<size_t>(length) - result.size());
            for (size_t i = 0; i < size; ++i) result.push_back(std::to_integer<uint8_t>(bytes[begin + i]));
            consumed += batch;
        }
    } catch (const std::exception&) { return false; }
    if (cancelled && cancelled()) return false;
    output = std::move(result);
    return true;
}
inline uint64_t sourceLe16(const std::vector<uint8_t>& bytes, size_t offset) {
    return bytes.at(offset) | (uint64_t{bytes.at(offset + 1)} << 8);
}
inline uint64_t sourceLe32(const std::vector<uint8_t>& bytes, size_t offset) {
    return sourceLe16(bytes, offset) | (sourceLe16(bytes, offset + 2) << 16);
}
}
