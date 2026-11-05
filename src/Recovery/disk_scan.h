#pragma once
#include "Disk/IDiskReader.hpp"
#include "Filesystem/FAT/fat_boot_geometry.h"
#include "fat_root_files.h"
#include "ntfs_mft.h"
#include "signature_carver.h"
#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace recoverysuite::recovery {
struct FoundVolume {
  uint64_t start{}, count{};
  std::string filesystem;
};
using DiskScanProgress =
    std::function<void(const std::string &, uint64_t, uint64_t)>;
using DiskScanItem = std::function<void(
    const std::string &, uint64_t,
    const std::function<std::vector<uint8_t>(uint64_t, size_t)> &,
    const std::string &)>;

// Search actual boot sectors independently of partition-table entries. No
// source sectors are modified, and only complete, bounded FAT/NTFS ranges are
// accepted.
inline std::vector<FoundVolume>
findDiskVolumes(disk::IDiskReader &source, const DiskScanProgress &progress,
                const std::function<bool()> &cancelled) {
  std::vector<FoundVolume> result;
  const auto sector = source.getSectorSize();
  const auto total = source.getTotalSectors();
  if (!filesystem::fat::supportedFatSectorSize(sector) || !total ||
      total > uint64_t(std::numeric_limits<int64_t>::max()) / sector)
    throw std::runtime_error("Unsupported source geometry for disk search");
  for (uint64_t at = 0; at < total;) {
    if (cancelled && cancelled())
      break;
    const auto count = std::min<uint64_t>(65536 / sector, total - at);
    std::vector<std::byte> bytes;
    if (!count || !source.readSectors(at, count, bytes) ||
        bytes.size() != count * sector)
      throw std::runtime_error(
          "Disk search stopped on an incomplete source read");
    for (uint64_t i = 0; i < count; ++i) {
      const auto start = at + i;
      // A volume's backup boot sector is not a separate partition.
      if (std::any_of(result.begin(), result.end(), [&](const auto &v) {
            return start >= v.start && start - v.start < v.count;
          }))
        continue;
      const auto *p =
          reinterpret_cast<const uint8_t *>(bytes.data()) + i * sector;
      if (p[510] != 0x55 || p[511] != 0xaa)
        continue;
      uint64_t length = 0;
      std::string type;
      const auto le = [&](size_t offset, size_t n) {
        uint64_t value = 0;
        for (size_t j = 0; j < n; ++j)
          value |= uint64_t(p[offset + j]) << (8 * j);
        return value;
      };
      if (!std::memcmp(p + 3, "NTFS    ", 8)) {
        length = le(40, 8);
        const auto clusters = p[13];
        if (le(11, 2) != sector || !clusters || clusters > 128 ||
            (clusters & (clusters - 1)) || !length ||
            le(48, 8) >= length / clusters || le(56, 8) >= length / clusters)
          continue;
        type = "NTFS";
      } else {
        filesystem::fat::FATBootSectorCommon boot{};
        std::memcpy(&boot, p, sizeof(boot));
        try {
          type = filesystem::fat::fatTypeToString(
              filesystem::fat::validatedFatType(boot, sector));
          length =
              boot.totalSectors16 ? boot.totalSectors16 : boot.totalSectors32;
        } catch (const std::runtime_error &) {
          continue;
        }
      }
      if (!length || length > total - start)
        continue;
      if (result.size() >= 128)
        throw std::runtime_error("Volume candidate limit reached (128)");
      result.push_back({start, length, type});
    }
    at += count;
    if (progress)
      progress("Searching for surviving and lost volumes", at * sector,
               total * sector);
  }
  return result;
}

class DiskCandidateLimit : public std::runtime_error {
public:
  DiskCandidateLimit()
      : std::runtime_error(
            "Candidate limit reached (10,000); review the partial results") {}
};

// Callback data is valid synchronously only. Catalogs retain names/sizes, never
// dangling read callbacks. Recovery replays discovery and exports chosen names.
inline std::vector<std::string>
scanDiskFiles(disk::IDiskReader &source,
              const std::vector<FoundVolume> &volumes, const DiskScanItem &item,
              const DiskScanProgress &progress,
              const std::function<bool()> &cancelled) {
  std::vector<std::string> issues;
  for (const auto &volume : volumes) {
    if (cancelled && cancelled())
      return issues;
    const auto prefix =
        volume.filesystem + "_" + std::to_string(volume.start) + "/";
    const auto report = [&](uint64_t bytes) {
      if (progress)
        progress("Scanning " + volume.filesystem + " at sector " +
                     std::to_string(volume.start),
                 bytes, 0);
    };
    const auto buffered = [&](const std::string &group,
                              const std::string &confidence) {
      return [&, group, confidence](const std::string &name,
                                    const std::vector<uint8_t> &data) {
        item(
            prefix + group + name, data.size(),
            [&](uint64_t offset, size_t n) {
              return std::vector<uint8_t>(data.begin() + offset,
                                          data.begin() + offset + n);
            },
            confidence);
      };
    };
    for (bool deleted : {false, true}) {
      if (cancelled && cancelled())
        return issues;
      try {
        if (volume.filesystem == "NTFS") {
          if (deleted) {
            NtfsRecoveryOptions options;
            options.deleted = true;
            const auto summary = recoverNtfsChecked(
                source, volume.start, volume.count, options,
                [&](const NtfsFileSource &file) {
                  item(prefix + "Deleted/" + file.path, file.size, file.read,
                       file.confidence);
                },
                report, cancelled);
            if (summary.invalidFiles)
              issues.push_back(
                  prefix +
                  "Some deleted NTFS records could not be reconstructed");
          } else {
            // Prefer checked streaming reconstruction; salvage retained paths
            // when allocation/index metadata is damaged or unavailable.
            try {
              NtfsRecoveryOptions options;
              recoverNtfsChecked(
                  source, volume.start, volume.count, options,
                  [&](const NtfsFileSource &file) {
                    item(prefix + "Existing/" + file.path, file.size, file.read,
                         "Allocation and directory membership checked; file "
                         "contents still unproven");
                  },
                  report, cancelled);
            } catch (const DiskCandidateLimit &) {
              throw;
            } catch (const std::exception &error) {
              if (cancelled && cancelled())
                return issues;
              issues.push_back(prefix +
                               "Checked reconstruction unavailable; trying "
                               "retained metadata: " +
                               error.what());
            }
            const auto summary = recoverNtfsTreeFiles(
                source, volume.start, volume.count,
                buffered("Existing/", "Reconstructed from retained NTFS "
                                      "metadata; contents unproven"),
                report, cancelled);
            if (summary.invalidFiles || summary.unsupportedFiles ||
                summary.nonresidentFiles)
              issues.push_back(
                  prefix +
                  "Some NTFS files have damaged or unsupported metadata");
          }
        } else {
          const auto summary = recoverFatRootFiles(
              source, volume.start, volume.count,
              buffered(deleted ? "Deleted/" : "Existing/",
                       deleted ? "Low confidence: contiguous free-cluster "
                                 "assumption; bytes may be overwritten"
                               : "Reconstructed from allocated FAT chains; "
                                 "contents unproven"),
              report, cancelled, true, deleted);
          if (summary.invalidFiles || summary.invalidDirectories)
            issues.push_back(
                prefix +
                "Some FAT files or directories could not be reconstructed");
        }
      } catch (const DiskCandidateLimit &) {
        throw;
      } catch (const std::exception &error) {
        if (cancelled && cancelled())
          return issues;
        issues.push_back(prefix + (deleted ? "Deleted: " : "Existing: ") +
                         error.what());
      }
    }
  }
  if (cancelled && cancelled())
    return issues;
  const auto summary = carveSignatures(
      source, 0, source.getTotalSectors(),
      [&](const std::string &name, const std::vector<uint8_t> &data) {
        item(
            "Signature matches/" + name, data.size(),
            [&](uint64_t offset, size_t n) {
              return std::vector<uint8_t>(data.begin() + offset,
                                          data.begin() + offset + n);
            },
            "Signature match only; original name, fragmentation and file "
            "validity unknown");
      },
      [&](uint64_t bytes) {
        if (progress)
          progress("Deep search for file signatures", bytes,
                   source.getTotalSectors() * source.getSectorSize());
      },
      cancelled);
  if (summary.skippedCandidates)
    issues.push_back(
        "Some signature matches were incomplete or exceeded the 16 MiB limit");
  return issues;
}
} // namespace recoverysuite::recovery
