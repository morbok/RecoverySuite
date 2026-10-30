#pragma once
#include "Disk/IDiskReader.hpp"
#include <functional>
namespace recoverysuite::recovery {
struct FatRootSummary {
    uint64_t processedBytes{}, foundItems{}, invalidFiles{}, deletedEntries{}, directories{};
    bool cancelled{};
};
// Copies allocated FAT12/16 root files using their 8.3 aliases and exact cluster chains.
// Does not infer deleted chains, traverse subdirectories, or support FAT32/NTFS. Max file: 16 MiB.
FatRootSummary recoverFatRootFiles(disk::IDiskReader& reader, uint64_t start, uint64_t count,
    const std::function<void(const std::string&,const std::vector<uint8_t>&)>& item,
    const std::function<void(uint64_t)>& progress = {}, const std::function<bool()>& cancelled = {});
}
