#pragma once
#include "Disk/IDiskReader.hpp"
#include <functional>
namespace recoverysuite::recovery {
// Earliest unconsumed byte and stable naming counter. Pending data is reread
// from this bounded position rather than serializing untrusted parser memory.
struct CarvingCheckpoint {
  uint64_t offset{}, foundItems{};
};
struct CarvingSummary {
  uint64_t processedBytes{};
  uint64_t foundItems{};
  uint64_t skippedCandidates{};
  bool cancelled{};
};
/** Bounded streaming signature carving. This does not verify complete file
 * format semantics. JPEG/PDF terminators and ZIP end records may span read
 * boundaries. Maximum candidate: 16 MiB. onLocated receives the exact byte
 * offset relative to the selected start sector. Callbacks run synchronously on
 * the calling thread; reader must remain open and immutable.
 */
CarvingSummary carveSignatures(
    disk::IDiskReader &reader, uint64_t startSector, uint64_t numSectors,
    const std::function<void(const std::string &, const std::vector<uint8_t> &)>
        &onItem,
    const std::function<void(uint64_t)> &onProgress = {},
    const std::function<bool()> &cancelled = {},
    const std::function<void(uint64_t, const std::string &,
                             const std::vector<uint8_t> &)> &onLocated = {},
    CarvingCheckpoint *checkpoint = nullptr);
} // namespace recoverysuite::recovery
