#pragma once
#include "Disk/IDiskReader.hpp"
#include <functional>
#include <vector>
namespace recoverysuite::recovery {
struct NtfsMftSnapshot {
    std::vector<uint8_t> bootSector, initializedRecords;
    uint64_t processedBytes{}, recordSize{}, inUseRecords{}, unusedRecords{}, blankRecords{};
};
// Read the initialized $MFT stream (<=16 MiB), preserving its on-disk bytes.
// Validates bootstrap attributes, nonsparse mapping pairs, record headers and 512-byte USA fixups.
// Attribute-list extensions, compressed/encrypted/sparse MFTs and mirror fallback are unsupported.
// Throws on invalid/unsupported metadata, failed/short reads, resource limits or cancellation.
NtfsMftSnapshot readNtfsMft(disk::IDiskReader& reader,uint64_t start,uint64_t count,
    const std::function<void(uint64_t)>& progress={},const std::function<bool()>& cancelled={});
}
