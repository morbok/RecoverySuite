#pragma once
#include "Disk/IDiskReader.hpp"
#include <functional>
namespace recoverysuite::recovery {
struct FatRootSummary {
    uint64_t processedBytes{}, foundItems{}, invalidFiles{}, deletedEntries{}, directories{}, longNameFallbacks{}, traversedDirectories{}, invalidDirectories{};
    bool cancelled{};
};
// Copies allocated FAT12/16/32 root files using verified Unicode long names or 8.3 aliases and exact cluster chains.
// Recursive mode emits relative file paths; otherwise only root files are returned.
// No deleted-chain inference or NTFS. Files/directories: 16 MiB each, directory reads: 64 MiB total.
FatRootSummary recoverFatRootFiles(disk::IDiskReader& reader, uint64_t start, uint64_t count,
    const std::function<void(const std::string&,const std::vector<uint8_t>&)>& item,
    const std::function<void(uint64_t)>& progress = {}, const std::function<bool()>& cancelled = {},
    bool recursive = false);
}
