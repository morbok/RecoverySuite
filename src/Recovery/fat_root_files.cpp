#include "fat_root_files.h"
#include "BootSectorDetection.h"
#include "fat_long_name.h"
#include "source_bytes.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace recoverysuite::recovery {
namespace {
struct Cancelled {};
struct InvalidFile : std::runtime_error {
  using std::runtime_error::runtime_error;
};
std::string shortPart(const std::vector<uint8_t> &root, size_t pos,
                      size_t width) {
  while (width && root[pos + width - 1] == ' ')
    --width;
  std::string name;
  for (size_t i = 0; i < width; ++i) {
    const auto c = root[pos + i];
    const bool valid =
        (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        std::string("!#$%&'()-@^_`{}~").find(static_cast<char>(c)) !=
            std::string::npos;
    if (!valid)
      throw InvalidFile("Unsupported short filename");
    name.push_back(static_cast<char>(c));
  }
  return name;
}
} // namespace
FatRootSummary recoverFatRootFiles(
    disk::IDiskReader &reader, uint64_t start, uint64_t count,
    const std::function<void(const std::string &, const std::vector<uint8_t> &)>
        &item,
    const std::function<void(uint64_t)> &progress,
    const std::function<bool()> &cancelled, bool recursive, bool deletedMode,
    const std::function<void(const FatFileSource &)> &streamItem) {
  FatRootSummary summary;
  const uint64_t fileLimit =
      streamItem ? 2ULL * 1024 * 1024 * 1024 : 16ULL * 1024 * 1024;
  const auto check = [&] {
    if (cancelled && cancelled())
      throw Cancelled{};
  };
  uint64_t lastProgress = 0;
  const auto read = [&](uint64_t offset, uint64_t length) {
    check();
    std::vector<uint8_t> bytes;
    if (!readSourceBytes(&reader, start, count, offset, length, bytes,
                         cancelled)) {
      check();
      throw std::runtime_error(
          "FAT source read failed or exceeded the selected range");
    }
    summary.processedBytes += length;
    if (progress &&
        (!streamItem || summary.processedBytes - lastProgress >= 65536)) {
      lastProgress = summary.processedBytes;
      progress(summary.processedBytes);
    }
    return bytes;
  };
  try {
    const auto boot = read(0, reader.getSectorSize());
    const auto type = detectBootSectorType(boot);
    const bool fat32 = type == "FAT32";
    if (type != "FAT12" && type != "FAT16" && !fat32)
      throw std::runtime_error(
          "Root-file recovery supports FAT12, FAT16 and FAT32 only");
    const uint64_t sectorSize = sourceLe16(boot, 11), clusterSectors = boot[13],
                   reserved = sourceLe16(boot, 14), copies = boot[16];
    uint64_t entries = sourceLe16(boot, 17);
    const uint64_t fatSectors =
        fat32 ? sourceLe32(boot, 36) : sourceLe16(boot, 22);
    const uint64_t total =
        sourceLe16(boot, 19) ? sourceLe16(boot, 19) : sourceLe32(boot, 32);
    const uint64_t rootSectors = (entries * 32 + sectorSize - 1) / sectorSize,
                   rootStart = reserved + copies * fatSectors;
    const uint64_t dataStart = rootStart + rootSectors;
    if (sectorSize != reader.getSectorSize() || !fatSectors || !copies ||
        copies > 2 || total > count || dataStart >= total ||
        (fat32 ? (entries != 0 || sourceLe16(boot, 22) != 0 ||
                  sourceLe16(boot, 42) != 0)
               : !entries))
      throw std::runtime_error("Invalid or truncated FAT volume geometry");
    const uint64_t clusters = (total - dataStart) / clusterSectors;
    const auto flags = fat32 ? sourceLe16(boot, 40) : 0;
    if (flags & ~uint64_t{0x8f})
      throw std::runtime_error("Reserved FAT32 flags are set");
    const bool mirrored = !(flags & 0x80);
    const uint64_t active = mirrored ? 0 : flags & 15;
    if (active >= copies)
      throw std::runtime_error(
          "Active FAT index exceeds the number of FAT copies");
    const auto fat = read((reserved + active * fatSectors) * sectorSize,
                          fatSectors * sectorSize);
    const uint64_t capacity = type == "FAT12"
                                  ? ((clusters + 2) * 3 + 1) / 2
                                  : (clusters + 2) * (fat32 ? 4 : 2);
    if (capacity > fat.size())
      throw std::runtime_error(
          "FAT is too short for the declared data clusters");
    if (mirrored && copies == 2) {
      const auto other =
          read((reserved + fatSectors) * sectorSize, fatSectors * sectorSize);
      bool equal = true;
      if (fat32) {
        // The upper four bits are reserved and do not belong to a FAT32 cluster
        // value.
        for (size_t cluster = 0; cluster < clusters + 2; ++cluster) {
          if (cluster % 4096 == 0)
            check();
          if ((sourceLe32(fat, cluster * 4) & 0x0fffffff) !=
              (sourceLe32(other, cluster * 4) & 0x0fffffff)) {
            equal = false;
            break;
          }
        }
      } else
        equal = other == fat;
      if (!equal)
        throw std::runtime_error(
            "FAT copies disagree; refusing to guess cluster chains");
    }
    const auto next = [&](uint64_t cluster) {
      if (fat32)
        return sourceLe32(fat, static_cast<size_t>(cluster * 4)) &
               uint64_t{0x0fffffff};
      if (type == "FAT16")
        return sourceLe16(fat, static_cast<size_t>(cluster * 2));
      const auto value =
          sourceLe16(fat, static_cast<size_t>(cluster + cluster / 2));
      return (cluster & 1) ? value >> 4 : value & 0xfff;
    };
    const uint64_t eoc =
        fat32 ? 0x0ffffff8 : (type == "FAT12" ? 0xff8 : 0xfff8);
    const uint64_t reservedCluster =
        fat32 ? 0x0ffffff0 : (type == "FAT12" ? 0xff0 : 0xfff0);
    const bool validSecond = type == "FAT12"
                                 ? next(1) >= eoc
                                 : (next(1) & (fat32 ? 0x03ffffff : 0x3fff)) ==
                                       (fat32 ? 0x03ffffff : 0x3fff);
    if ((next(0) & 255) != boot[21] ||
        (next(0) & ~uint64_t{255}) !=
            (fat32 ? 0x0fffff00 : (type == "FAT12" ? 0xf00 : 0xff00)) ||
        !validSecond)
      throw std::runtime_error("Invalid reserved FAT entries");
    std::vector<uint8_t> root;
    std::vector<bool> usedClusters(static_cast<size_t>(clusters + 2), false);
    std::vector<bool> directoryClaims(
        recursive ? static_cast<size_t>(clusters + 2) : 0, false);
    if (fat32) {
      uint64_t cluster = sourceLe32(boot, 44);
      while (cluster < eoc) {
        check();
        if (cluster < 2 || cluster >= clusters + 2 ||
            cluster >= reservedCluster || usedClusters[cluster])
          throw std::runtime_error(
              "Invalid or cyclic FAT32 root directory chain");
        usedClusters[cluster] = true;
        const auto length = clusterSectors * sectorSize;
        if (root.size() + length > 16 * 1024 * 1024)
          throw std::runtime_error(
              "FAT32 root directory exceeds the 16 MiB limit");
        const auto bytes = read(
            (dataStart + (cluster - 2) * clusterSectors) * sectorSize, length);
        root.insert(root.end(), bytes.begin(), bytes.end());
        cluster = next(cluster);
      }
      if (root.empty())
        throw std::runtime_error(
            "FAT32 root directory has no allocated cluster");
      entries = root.size() / 32;
    } else
      root = read(rootStart * sectorSize, rootSectors * sectorSize);
    // Reserve known child-directory chains before exporting any root file.
    // Directory entries can occur after files, so a single output pass would
    // otherwise miss these cross-links and export directory metadata as if it
    // were file contents.
    if (!recursive)
      for (size_t index = 0; index < entries; ++index) {
        check();
        const size_t pos = index * 32;
        if (root[pos] == 0)
          break;
        if (root[pos] == 0xe5 || (root[pos + 11] & 0x18) != 0x10)
          continue;
        uint64_t cluster = sourceLe16(root, pos + 26) |
                           (fat32 ? sourceLe16(root, pos + 20) << 16 : 0);
        while (cluster >= 2 && cluster < clusters + 2 &&
               cluster < reservedCluster) {
          check();
          if (usedClusters[cluster])
            break;
          usedClusters[cluster] = true;
          cluster = next(cluster);
        }
      }
    struct Directory {
      std::vector<uint8_t> bytes;
      std::string path;
      size_t depth;
    };
    struct File {
      std::string name;
      uint64_t size, first;
    };
    std::vector<Directory> directories;
    directories.push_back({std::move(root), {}, 0});
    std::vector<File> files, deletedFiles, liveFiles;
    std::set<std::string> directoryPaths;
    uint64_t directoryBytes = directories.front().bytes.size();
    for (size_t d = 0; d < directories.size(); ++d) {
      // Move the current directory out: appending children may reallocate the
      // queue.
      auto current = std::move(directories[d]);
      const auto &bytes = current.bytes;
      FatLongName longName;
      const size_t limit = d == 0 ? entries : bytes.size() / 32;
      for (size_t index = 0; index < limit; ++index) {
        check();
        const size_t pos = index * 32;
        if (bytes[pos] == 0)
          break;
        if (bytes[pos] == 0xe5) {
          longName.reset();
          ++summary.deletedEntries;
          if (!deletedMode || (bytes[pos + 11] & 0x18) ||
              (bytes[pos + 11] & 0x0f) == 0x0f)
            continue;
          try {
            if (deletedFiles.size() + summary.invalidFiles >= 10000)
              throw std::runtime_error("Deleted candidate limit");
            if (bytes[pos + 11] & 0xc0)
              throw InvalidFile("Invalid deleted attributes");
            std::vector<uint8_t> alias(bytes.begin() + pos,
                                       bytes.begin() + pos + 32);
            alias[0] = '_';
            auto name = shortPart(alias, 0, 8);
            const auto extension = shortPart(alias, 8, 3);
            if (!extension.empty())
              name += '.' + extension;
            name = "deleted-" + std::to_string(d) + "-" +
                   std::to_string(index) + "-" + name;
            if (!current.path.empty())
              name = current.path + '/' + name;
            const auto size = sourceLe32(bytes, pos + 28),
                       high = sourceLe16(bytes, pos + 20),
                       first = sourceLe16(bytes, pos + 26) |
                               (fat32 ? high << 16 : 0);
            if (name.size() > 4095 || size > fileLimit ||
                (fat32 ? (high & 0xf000) != 0 : high != 0) || (!size && first))
              throw InvalidFile("Invalid deleted file size or cluster");
            deletedFiles.push_back({std::move(name), size, first});
          } catch (const InvalidFile &) {
            ++summary.invalidFiles;
          }
          continue;
        }
        const auto attributes = bytes[pos + 11];
        if ((attributes & 0x0f) == 0x0f) {
          longName.consume(bytes, pos);
          continue;
        }
        if (attributes & 8) {
          longName.reset();
          continue;
        }
        const bool directory = attributes & 16;
        // Dot links describe parent relationships, never traversal targets.
        if (directory && bytes[pos] == '.' &&
            ((bytes[pos + 1] == ' ' &&
              std::all_of(bytes.begin() + pos + 2, bytes.begin() + pos + 11,
                          [](uint8_t c) { return c == ' '; })) ||
             (bytes[pos + 1] == '.' &&
              std::all_of(bytes.begin() + pos + 2, bytes.begin() + pos + 11,
                          [](uint8_t c) { return c == ' '; })))) {
          longName.reset();
          continue;
        }
        if (directory) {
          ++summary.directories;
          if (!recursive) {
            longName.reset();
            continue;
          }
          if (summary.directories > 1023)
            throw std::runtime_error("FAT directory item limit reached");
          // Even an invalid name or an oversized directory still claims
          // metadata clusters. Reserve the entire reachable chain without
          // reading its data.
          uint64_t claimed = sourceLe16(bytes, pos + 26) |
                             (fat32 ? sourceLe16(bytes, pos + 20) << 16 : 0);
          while (claimed >= 2 && claimed < clusters + 2 &&
                 claimed < reservedCluster && !directoryClaims[claimed]) {
            check();
            directoryClaims[claimed] = true;
            claimed = next(claimed);
          }
        } else if (files.size() + summary.invalidFiles >= 10000)
          throw std::runtime_error("FAT output item limit reached");
        if (deletedMode && !directory) {
          const auto first =
              sourceLe16(bytes, pos + 26) |
              (fat32 ? (sourceLe16(bytes, pos + 20) & 0x0fff) << 16 : 0);
          liveFiles.push_back({{}, sourceLe32(bytes, pos + 28), first});
        }
        bool fallback = false;
        auto name = longName.take(bytes, pos, fallback);
        if (fallback)
          ++summary.longNameFallbacks;
        try {
          if (attributes & 0xc0)
            throw InvalidFile("Reserved directory attributes are set");
          if (name.empty()) {
            name = shortPart(bytes, pos, 8);
            const auto extension = shortPart(bytes, pos + 8, 3);
            if (name.empty())
              throw InvalidFile("Empty filename");
            if (!extension.empty())
              name += '.' + extension;
          }
          if (!current.path.empty())
            name = current.path + '/' + name;
          if (name.size() > 4095)
            throw InvalidFile("Recovered path exceeds the output path limit");
          const uint64_t size = sourceLe32(bytes, pos + 28),
                         high = sourceLe16(bytes, pos + 20);
          const uint64_t first =
              sourceLe16(bytes, pos + 26) | (fat32 ? high << 16 : 0);
          if ((fat32 ? (high & 0xf000) != 0 : high != 0))
            throw InvalidFile("Unsupported first cluster");
          if (directory) {
            if (size || current.depth >= 32 || directories.size() >= 1024 ||
                !directoryPaths.insert(name).second)
              throw InvalidFile(
                  "Invalid directory size, depth, count or duplicate path");
            std::vector<uint8_t> contents;
            std::set<uint64_t> chain;
            uint64_t cluster = first;
            do {
              check();
              if (cluster < 2 || cluster >= clusters + 2 ||
                  cluster >= reservedCluster || !chain.insert(cluster).second ||
                  usedClusters[cluster])
                throw InvalidFile(
                    "Invalid, cyclic or cross-linked directory chain");
              usedClusters[cluster] = true;
              const auto length = clusterSectors * sectorSize;
              if (contents.size() + length > 16 * 1024 * 1024 ||
                  directoryBytes + length > 64 * 1024 * 1024)
                throw InvalidFile("Directory read limit reached");
              const auto part = read(
                  (dataStart + (cluster - 2) * clusterSectors) * sectorSize,
                  length);
              directoryBytes += length;
              contents.insert(contents.end(), part.begin(), part.end());
              cluster = next(cluster);
            } while (cluster < eoc);
            directories.push_back(
                {std::move(contents), std::move(name), current.depth + 1});
            ++summary.traversedDirectories;
          } else {
            if (size > fileLimit || (!size && first))
              throw InvalidFile("Unsupported file size or first cluster");
            files.push_back({std::move(name), size, first});
          }
        } catch (const InvalidFile &) {
          if (directory)
            ++summary.invalidDirectories;
          else
            ++summary.invalidFiles;
        }
      }
    }
    if (deletedMode) {
      std::vector<bool> liveClaims(static_cast<size_t>(clusters + 2), false);
      uint64_t claimWork = 0;
      const auto work = [&] {
        check();
        if (++claimWork > 16 * 1024 * 1024)
          throw std::runtime_error("Deleted cluster-analysis work limit");
      };
      // Reserve every reachable live chain, including inconsistent entries
      // whose FAT was cleared.
      for (const auto &file : liveFiles) {
        const auto contiguous = (file.size + clusterSectors * sectorSize - 1) /
                                (clusterSectors * sectorSize);
        if (file.first >= 2 && file.first < clusters + 2 &&
            contiguous <= clusters + 2 - file.first)
          for (uint64_t i = 0; i < contiguous; ++i) {
            work();
            usedClusters[file.first + i] = true;
          }
        auto cluster = file.first;
        while (cluster >= 2 && cluster < clusters + 2 &&
               cluster < reservedCluster && !liveClaims[cluster]) {
          work();
          liveClaims[cluster] = true;
          usedClusters[cluster] = true;
          cluster = next(cluster);
        }
      }
      for (const auto &file : deletedFiles) {
        try {
          const auto clusterBytes = clusterSectors * sectorSize,
                     needed = (file.size + clusterBytes - 1) / clusterBytes;
          if (streamItem && needed > 1048576)
            throw InvalidFile("Deleted FAT cluster-chain limit reached");
          if (needed && (file.first < 2 || file.first >= clusters + 2 ||
                         needed > clusters + 2 - file.first))
            throw InvalidFile("Deleted extent outside volume");
          for (uint64_t i = 0; i < needed; ++i) {
            work();
            const auto c = file.first + i;
            if (c >= reservedCluster || next(c) != 0 || usedClusters[c] ||
                (recursive && directoryClaims[c]))
              throw InvalidFile("Deleted cluster is allocated or claimed");
          }
          check();
          if (streamItem) {
            FatFileSource source{
                file.name, file.size, [&](uint64_t offset, size_t n) {
                  if (offset > file.size || n > file.size - offset || n > 65536)
                    throw InvalidFile("Invalid FAT preview/export read");
                  return n ? read((dataStart +
                                   (file.first - 2) * clusterSectors) *
                                          sectorSize +
                                      offset,
                                  n)
                           : std::vector<uint8_t>{};
                }};
            streamItem(source);
          } else {
            std::vector<uint8_t> data;
            if (file.size)
              data = read((dataStart + (file.first - 2) * clusterSectors) *
                              sectorSize,
                          file.size);
            check();
            item(file.name, data);
          }
          for (uint64_t i = 0; i < needed; ++i)
            usedClusters[file.first + i] = true;
          ++summary.foundItems;
        } catch (const InvalidFile &) {
          ++summary.invalidFiles;
        }
      }
      return summary;
    }
    if (streamItem) {
      struct Candidate {
        std::vector<uint64_t> chain;
        bool valid{};
      };
      std::vector<Candidate> candidates(files.size());
      std::vector<int32_t> owners(size_t(clusters + 2), -1);
      std::set<size_t> conflicts;
      const auto clusterBytes = clusterSectors * sectorSize;
      uint64_t references = 0;
      for (size_t index = 0; index < files.size(); ++index) {
        const auto &file = files[index];
        auto &candidate = candidates[index];
        try {
          const auto needed = (file.size + clusterBytes - 1) / clusterBytes;
          if (needed > 1048576)
            throw InvalidFile("FAT cluster-chain limit reached");
          auto cluster = file.first;
          for (uint64_t i = 0; i < needed; ++i) {
            check();
            if (++references > 4 * 1024 * 1024)
              throw std::runtime_error(
                  "FAT aggregate cluster-analysis limit reached");
            if (cluster < 2 || cluster >= clusters + 2 ||
                cluster >= reservedCluster || usedClusters[cluster] ||
                (recursive && directoryClaims[cluster]) ||
                owners[cluster] == int32_t(index))
              throw InvalidFile(
                  "Invalid, cyclic or metadata-aliased FAT chain");
            if (owners[cluster] >= 0) {
              conflicts.insert(index);
              conflicts.insert(size_t(owners[cluster]));
            }
            owners[cluster] = int32_t(index);
            candidate.chain.push_back(cluster);
            const auto following = next(cluster);
            if ((i + 1 == needed && following < eoc) ||
                (i + 1 < needed && following >= eoc))
              throw InvalidFile("FAT chain length does not match file size");
            cluster = following;
          }
          candidate.valid = true;
        } catch (const InvalidFile &) {
          ++summary.invalidFiles;
        }
      }
      for (size_t index = 0; index < files.size(); ++index) {
        check();
        const auto &file = files[index];
        const auto &candidate = candidates[index];
        if (!candidate.valid)
          continue;
        if (conflicts.contains(index)) {
          ++summary.invalidFiles;
          continue;
        }
        FatFileSource source{
            file.name, file.size, [&](uint64_t at, size_t n) {
              check();
              if (at > file.size || n > file.size - at || n > 65536)
                throw InvalidFile("Invalid FAT preview/export read");
              std::vector<uint8_t> result;
              result.reserve(n);
              while (result.size() < n) {
                const auto logical = at / clusterBytes,
                           within = at % clusterBytes;
                const auto length = std::min<uint64_t>(n - result.size(),
                                                       clusterBytes - within);
                const auto part = read(
                    (dataStart + (candidate.chain.at(size_t(logical)) - 2) *
                                     clusterSectors) *
                            sectorSize +
                        within,
                    length);
                result.insert(result.end(), part.begin(), part.end());
                at += length;
              }
              return result;
            }};
        streamItem(source);
        ++summary.foundItems;
      }
      return summary;
    }
    // Discover/reserve all directory chains before reading any file payload.
    for (const auto &file : files) {
      try {
        std::vector<uint8_t> data;
        data.reserve(static_cast<size_t>(file.size));
        std::set<uint64_t> chain;
        uint64_t cluster = file.first;
        while (data.size() < file.size) {
          check();
          if (cluster < 2 || cluster >= clusters + 2 ||
              cluster >= reservedCluster || !chain.insert(cluster).second ||
              usedClusters[cluster] || (recursive && directoryClaims[cluster]))
            throw InvalidFile("Invalid, cyclic or cross-linked cluster chain");
          const auto length = std::min<uint64_t>(file.size - data.size(),
                                                 clusterSectors * sectorSize);
          const auto part =
              read((dataStart + (cluster - 2) * clusterSectors) * sectorSize,
                   length);
          data.insert(data.end(), part.begin(), part.end());
          const auto following = next(cluster);
          if (data.size() == file.size) {
            if (following < eoc)
              throw InvalidFile("Chain extends beyond declared file length");
          } else if (following >= eoc)
            throw InvalidFile("Chain ends before declared file length");
          cluster = following;
        }
        check();
        item(file.name, data);
        for (const auto allocated : chain)
          usedClusters[allocated] = true;
        ++summary.foundItems;
      } catch (const InvalidFile &) {
        ++summary.invalidFiles;
      }
    }
  } catch (const Cancelled &) {
    summary.cancelled = true;
  }
  return summary;
}
} // namespace recoverysuite::recovery
