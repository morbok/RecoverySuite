#pragma once
#include "Disk/IDiskReader.hpp"
#include <functional>
#include <vector>
#include <string>
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
struct NtfsResidentSummary {
    uint64_t processedBytes{},foundItems{},invalidFiles{},nonresidentFiles{},unsupportedFiles{};
    uint64_t unusedRecords{},directoryRecords{},extensionRecords{},nonRootFiles{},otherRootNames{},recoveredNonresidentFiles{};
};
// Copy unnamed resident DATA from in-use base records with a sequence-matching root parent.
// Exports one safe preferred root name per record. Does not traverse indexes/subdirectories,
// reconstruct deleted/nonresident/ADS data, or cross-check allocation bitmaps.
NtfsResidentSummary recoverNtfsResidentRootFiles(disk::IDiskReader& reader,uint64_t start,uint64_t count,
    const std::function<void(const std::string&,const std::vector<uint8_t>&)>& item,
    const std::function<void(uint64_t)>& progress={},const std::function<bool()>& cancelled={});

// Also copy nonsparse, uncompressed, unencrypted nonresident unnamed DATA, <=16 MiB/file.
// Requires complete mapping pairs in the base record. Zero-fills uninitialized logical bytes.
// Rejects overlaps with the MFT/previous file runs; no allocation bitmap/index validation.
NtfsResidentSummary recoverNtfsRootFiles(disk::IDiskReader& reader,uint64_t start,uint64_t count,
    const std::function<void(const std::string&,const std::vector<uint8_t>&)>& item,
    const std::function<void(uint64_t)>& progress={},const std::function<bool()>& cancelled={});

}
