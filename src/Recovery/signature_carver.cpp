#include "signature_carver.h"
#include <algorithm>
#include <array>
#include <limits>
namespace recoverysuite::recovery {
CarvingSummary carveSignatures(disk::IDiskReader& reader, uint64_t startSector, uint64_t numSectors,
    const std::function<void(const std::string&, const std::vector<uint8_t>&)>& onItem,
    const std::function<void(uint64_t)>& onProgress, const std::function<bool()>& cancelled) {
    const uint64_t sectorSize = reader.getSectorSize(), total = reader.getTotalSectors();
    if (!reader.isOpen() || !sectorSize || sectorSize > 65536 || !numSectors ||
        startSector >= total || numSectors > total - startSector ||
        numSectors > std::numeric_limits<uint64_t>::max() / sectorSize)
        throw std::invalid_argument("Invalid carving range");
    constexpr size_t kChunkBytes = 64 * 1024, kMaximumCandidate = 16 * 1024 * 1024;
    const std::array<std::vector<uint8_t>, 3> headers{{{0xFF,0xD8,0xFF},{'%','P','D','F','-'},{'P','K',3,4}}};
    const std::array<std::vector<uint8_t>, 3> endings{{{0xFF,0xD9},{'%','%','E','O','F'},{'P','K',5,6}}};
    const std::array<const char*, 3> extensions{{"jpg","pdf","zip"}};
    CarvingSummary summary;
    std::vector<uint8_t> pending;
    size_t activeKind = headers.size(), endSearchFrom = 0;
    const auto shouldCancel = [&] { return cancelled && cancelled(); };
    for (uint64_t sector = 0; sector < numSectors;) {
        if (shouldCancel()) { summary.cancelled = true; return summary; }
        const uint64_t count = std::min<uint64_t>(numSectors - sector, kChunkBytes / sectorSize);
        std::vector<std::byte> bytes;
        if (!reader.readSectors(startSector + sector, count, bytes) || bytes.size() != count * sectorSize)
            throw disk::DiskException("Short read while carving image");
        for (const auto byte : bytes) pending.push_back(std::to_integer<uint8_t>(byte));
        sector += count;
        summary.processedBytes += bytes.size();
        while (!pending.empty()) {
            if (shouldCancel()) { summary.cancelled = true; return summary; }
            if (activeKind == headers.size()) {
                size_t first = pending.size();
                for (size_t candidate = 0; candidate < headers.size(); ++candidate) {
                    const auto it = std::search(pending.begin(), pending.end(), headers[candidate].begin(), headers[candidate].end());
                    const auto position = static_cast<size_t>(it - pending.begin());
                    if (position < first) { first = position; activeKind = candidate; }
                }
                if (first == pending.size()) {
                    if (pending.size() > 4) pending.erase(pending.begin(), pending.end() - 4);
                    break;
                }
                pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(first));
                endSearchFrom = headers[activeKind].size();
            }
            const size_t kind = activeKind;
            auto end = std::search(pending.begin() + static_cast<std::ptrdiff_t>(endSearchFrom), pending.end(),
                endings[kind].begin(), endings[kind].end());
            endSearchFrom = end == pending.end() ? std::max(headers[kind].size(), pending.size() - (endings[kind].size() - 1))
                : static_cast<size_t>(end - pending.begin());
            size_t length = end == pending.end() ? 0 : static_cast<size_t>(end - pending.begin()) + endings[kind].size();
            if (length && kind == 2) {
                const size_t record = length - 4;
                if (pending.size() < record + 22) length = 0;
                else {
                    const size_t comment = pending[record + 20] | (size_t{pending[record + 21]} << 8);
                    length = record + 22 + comment;
                    if (length > pending.size()) length = 0;
                }
            }
            if (length && length <= kMaximumCandidate) {
                const std::vector<uint8_t> data(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(length));
                onItem("carved_" + std::to_string(summary.foundItems) + "." + extensions[kind], data);
                ++summary.foundItems;
                pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(length));
                activeKind = headers.size(); endSearchFrom = 0;
            } else if (pending.size() > kMaximumCandidate) {
                ++summary.skippedCandidates;
                pending.erase(pending.begin());
                activeKind = headers.size(); endSearchFrom = 0;
            } else break;
        }
        if (onProgress) onProgress(summary.processedBytes);
    }
    if (shouldCancel()) summary.cancelled = true;
    if (activeKind != headers.size()) ++summary.skippedCandidates;
    return summary;
}
}
