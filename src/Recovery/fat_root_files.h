#pragma once
#include "Disk/IDiskReader.hpp"
#include <functional>
namespace recoverysuite::recovery {
struct FatFileSource {
  std::string path;
  uint64_t size{};
  std::function<std::vector<uint8_t>(uint64_t, size_t)> read;
};
struct FatRootSummary {
  uint64_t processedBytes{}, foundItems{}, invalidFiles{}, deletedEntries{},
      directories{}, longNameFallbacks{}, traversedDirectories{},
      invalidDirectories{};
  bool cancelled{};
};
// Copies allocated FAT12/16/32 root files using verified Unicode long names
// or 8.3 aliases and exact cluster chains. Recursive mode emits relative file
// paths; otherwise only root files are returned. Deleted mode infers contiguous
// free-cluster candidates from deleted short entries; original content is
// unproven. Optional streamItem avoids payload reads during discovery, up to 2
// GiB/file, with bounded 64 KiB reads and one million clusters/file. Its read
// callback is synchronous-only. The original buffered callback retains the 16
// MiB limit. Does not reconstruct deleted directories or lost long names.
// Files/directories: 16 MiB each, directory reads: 64 MiB total.
FatRootSummary recoverFatRootFiles(
    disk::IDiskReader &reader, uint64_t start, uint64_t count,
    const std::function<void(const std::string &, const std::vector<uint8_t> &)>
        &item,
    const std::function<void(uint64_t)> &progress = {},
    const std::function<bool()> &cancelled = {}, bool recursive = false,
    bool deletedMode = false,
    const std::function<void(const FatFileSource &)> &streamItem = {});
} // namespace recoverysuite::recovery
