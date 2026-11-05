#pragma once
#include "source_bytes.h"
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>

namespace recoverysuite::recovery {
// Read-only exFAT 1.x reconstruction. Format reference:
// https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification
namespace exfat {
using Bytes = std::vector<uint8_t>;
inline uint64_t le(const uint8_t *p, size_t n) {
  uint64_t v = 0;
  for (size_t i = 0; i < n; ++i)
    v |= uint64_t(p[i]) << (8 * i);
  return v;
}
struct Geometry {
  uint64_t sectors{}, fat{}, fatLength{}, heap{}, clusters{}, root{},
      clusterBytes{};
};
inline Geometry geometry(const uint8_t *p, uint32_t sector,
                         uint64_t available) {
  const auto fail = [] {
    throw std::runtime_error("Invalid or unsupported exFAT geometry");
  };
  if (std::memcmp(p + 3, "EXFAT   ", 8) || p[510] != 0x55 || p[511] != 0xaa ||
      p[108] < 9 || p[108] > 12 || (uint32_t{1} << p[108]) != sector ||
      p[109] > 25 - p[108] || le(p + 104, 2) != 0x100 || p[110] != 1 ||
      (le(p + 106, 2) & 1))
    fail(); // TexFAT is deliberately unsupported.
  for (size_t i = 11; i < 64; ++i)
    if (p[i])
      fail();
  Geometry g{le(p + 72, 8),
             le(p + 80, 4),
             le(p + 84, 4),
             le(p + 88, 4),
             le(p + 92, 4),
             le(p + 96, 4),
             uint64_t(sector) << p[109]};
  if (g.sectors < 24 || g.sectors > available || g.fat < 24 || !g.fatLength ||
      g.fat >= g.sectors || g.fatLength > g.sectors - g.fat ||
      g.heap < g.fat + g.fatLength || g.heap >= g.sectors || !g.clusters ||
      g.clusters > 0xfffffff5 ||
      g.clusters > (g.sectors - g.heap) / (uint64_t{1} << p[109]) ||
      (g.clusters + 2) * 4 > g.fatLength * sector || g.root < 2 ||
      g.root - 2 >= g.clusters)
    fail();
  return g;
}
inline std::string name(const Bytes &entries, size_t length) {
  std::string out;
  std::vector<uint16_t> units;
  for (size_t at = 64; at < entries.size(); at += 32) {
    if ((entries[at] | 0x80) != 0xc1)
      throw std::runtime_error("Unsupported exFAT secondary entry");
    for (size_t j = 2; j < 32 && units.size() < length; j += 2)
      units.push_back(static_cast<uint16_t>(le(entries.data() + at + j, 2)));
  }
  if (units.size() != length || !length)
    throw std::runtime_error("Incomplete exFAT name");
  for (size_t i = 0; i < units.size(); ++i) {
    uint32_t c = units[i];
    if (c >= 0xd800 && c <= 0xdbff) {
      if (++i == units.size() || units[i] < 0xdc00 || units[i] > 0xdfff)
        throw std::runtime_error("Invalid UTF-16 exFAT name");
      c = 0x10000 + ((c - 0xd800) << 10) + units[i] - 0xdc00;
    } else if (c >= 0xdc00 && c <= 0xdfff)
      throw std::runtime_error("Invalid UTF-16 exFAT name");
    if (c < 32 || (c < 128 && std::string("\"*/:<>?\\|").find(char(c)) !=
                                  std::string::npos))
      throw std::runtime_error("Unsafe exFAT name");
    if (c < 0x80)
      out += char(c);
    else {
      if (c >= 0x10000)
        out += char(0xf0 | (c >> 18));
      else if (c >= 0x800)
        out += char(0xe0 | (c >> 12));
      else
        out += char(0xc0 | (c >> 6));
      if (c >= 0x10000)
        out += char(0x80 | ((c >> 12) & 63));
      if (c >= 0x800)
        out += char(0x80 | ((c >> 6) & 63));
      out += char(0x80 | (c & 63));
    }
  }
  if (out == "." || out == ".." || out.back() == '.' || out.back() == ' ')
    throw std::runtime_error("Unsafe exFAT path component");
  return out;
}
struct File {
  std::string path, confidence;
  uint64_t size{};
  std::function<Bytes(uint64_t, size_t)> read;
};
struct Summary {
  uint64_t found{}, invalid{};
};
class Reader {
  disk::IDiskReader &source_;
  uint64_t start_, count_, metadataBytes_{};
  std::function<bool()> cancelled_;
  Geometry g_;
  Bytes bitmap_;
  std::set<uint32_t> metadata_, owned_;
  uint64_t fatPageOffset_ = UINT64_MAX;
  Bytes fatPage_;

public:
  Reader(disk::IDiskReader &source, uint64_t start, uint64_t count,
         std::function<bool()> cancelled)
      : source_(source), start_(start), count_(count),
        cancelled_(std::move(cancelled)) {
    const auto boot = bytes(0, 12 * uint64_t(source.getSectorSize()));
    g_ = geometry(boot.data(), source.getSectorSize(), count);
    uint32_t checksum = 0;
    const size_t end = size_t(source.getSectorSize()) * 11;
    for (size_t i = 0; i < end; ++i)
      if (i != 106 && i != 107 && i != 112)
        checksum =
            (checksum >> 1) + ((checksum & 1) ? 0x80000000u : 0) + boot[i];
    for (size_t i = end; i < boot.size(); i += 4)
      if (le(boot.data() + i, 4) != checksum)
        throw std::runtime_error("exFAT boot checksum mismatch");
  }
  void check() const {
    if (cancelled_ && cancelled_())
      throw std::runtime_error("Cancelled");
  }
  Bytes bytes(uint64_t offset, uint64_t size) {
    check();
    Bytes out;
    if (!readSourceBytes(&source_, start_, count_, offset, size, out,
                         cancelled_))
      throw std::runtime_error(
          "Incomplete exFAT source read or resource limit");
    return out;
  }
  uint64_t offset(uint32_t cluster) const {
    if (cluster < 2 || uint64_t(cluster) - 2 >= g_.clusters)
      throw std::runtime_error("exFAT cluster outside volume");
    return g_.heap * source_.getSectorSize() +
           (uint64_t(cluster) - 2) * g_.clusterBytes;
  }
  uint32_t next(uint32_t cluster) {
    offset(cluster);
    const uint64_t at = uint64_t(cluster) * 4;
    const auto page = at / 4096 * 4096;
    if (page != fatPageOffset_) {
      fatPage_ =
          bytes(g_.fat * source_.getSectorSize() + page,
                std::min<uint64_t>(
                    4096, g_.fatLength * source_.getSectorSize() - page));
      fatPageOffset_ = page;
    }
    return static_cast<uint32_t>(le(fatPage_.data() + at - page, 4));
  }
  std::vector<uint32_t> chain(uint32_t first, uint64_t length, bool contiguous,
                              bool toEnd = false) {
    if (!length && !toEnd) {
      if (first)
        throw std::runtime_error("Invalid empty exFAT stream");
      return {};
    }
    const auto needed =
        length / g_.clusterBytes + (length % g_.clusterBytes != 0);
    std::vector<uint32_t> result;
    std::set<uint32_t> visited;
    auto current = first;
    while (true) {
      check();
      offset(current);
      if (result.size() >= 1048576 || !visited.insert(current).second)
        throw std::runtime_error("exFAT cyclic or excessive cluster chain");
      result.push_back(current);
      if (!toEnd && result.size() == needed) {
        if (!contiguous && next(current) != 0xffffffff)
          throw std::runtime_error("exFAT chain length mismatch");
        break;
      }
      const auto n = contiguous ? current + 1 : next(current);
      if (n == 0xffffffff) {
        if (!toEnd)
          throw std::runtime_error("Truncated exFAT chain");
        break;
      }
      current = n;
      if (toEnd && result.size() * g_.clusterBytes > 16 * 1024 * 1024)
        throw std::runtime_error("exFAT directory exceeds 16 MiB");
    }
    return result;
  }
  Bytes pull(const std::vector<uint32_t> &clusters, uint64_t size,
             uint64_t valid, uint64_t at, size_t n) {
    if (at > size || n > size - at || n > 65536)
      throw std::runtime_error("Invalid exFAT read range");
    Bytes out(n, 0);
    size_t done = 0;
    while (done < n && at < valid) {
      check();
      const auto within = at % g_.clusterBytes;
      const auto take =
          std::min<uint64_t>({n - done, g_.clusterBytes - within, valid - at});
      auto part = bytes(
          offset(clusters.at(size_t(at / g_.clusterBytes))) + within, take);
      std::copy(part.begin(), part.end(), out.begin() + done);
      done += size_t(take);
      at += take;
    }
    return out;
  }
  Bytes metadata(const std::vector<uint32_t> &clusters, uint64_t length) {
    if (!length || length > 16 * 1024 * 1024 ||
        metadataBytes_ + length > 64 * 1024 * 1024)
      throw std::runtime_error("exFAT metadata resource limit");
    metadataBytes_ += length;
    Bytes out;
    for (uint64_t at = 0; at < length;) {
      const auto n = size_t(std::min<uint64_t>(65536, length - at));
      auto part = pull(clusters, length, length, at, n);
      out.insert(out.end(), part.begin(), part.end());
      at += n;
    }
    return out;
  }
  bool allocated(uint32_t cluster) const {
    offset(cluster);
    const auto bit = uint64_t(cluster) - 2;
    return (bitmap_.at(size_t(bit / 8)) & (1 << (bit % 8))) != 0;
  }
  Summary scan(const std::function<void(const File &)> &item,
               const std::function<void(uint64_t)> &progress) {
    Summary summary;
    const auto rootChain = chain(uint32_t(g_.root), 0, false, true);
    auto root = metadata(rootChain, rootChain.size() * g_.clusterBytes);
    metadata_.insert(rootChain.begin(), rootChain.end());
    std::vector<uint32_t> bitmapChain;
    for (size_t at = 0; at + 32 <= root.size() && root[at]; at += 32) {
      if (root[at] != 0x81 || (root[at + 1] & 1))
        continue;
      if (!bitmap_.empty())
        throw std::runtime_error("Duplicate exFAT allocation bitmap");
      const auto length = le(root.data() + at + 24, 8);
      if (length != (g_.clusters + 7) / 8)
        throw std::runtime_error("Invalid exFAT bitmap length");
      bitmapChain =
          chain(uint32_t(le(root.data() + at + 20, 4)), length, false);
      bitmap_ = metadata(bitmapChain, length);
      metadata_.insert(bitmapChain.begin(), bitmapChain.end());
    }
    if (bitmap_.empty())
      throw std::runtime_error("Missing exFAT allocation bitmap");
    for (auto c : metadata_)
      if (!allocated(c))
        throw std::runtime_error("Unallocated exFAT metadata");
    struct Directory {
      Bytes data;
      std::string path;
      unsigned depth;
    };
    std::vector<Directory> pending;
    pending.push_back({std::move(root), "", 0});
    size_t directories = 1;
    while (!pending.empty()) {
      auto directory = std::move(pending.back());
      pending.pop_back();
      for (size_t at = 0;
           at + 32 <= directory.data.size() && directory.data[at];) {
        check();
        const auto *p = directory.data.data() + at;
        if ((p[0] | 0x80) != 0x85) {
          at += 32;
          continue;
        }
        const auto secondary = p[1];
        const size_t size = (size_t(secondary) + 1) * 32;
        if (secondary < 2 || secondary > 18 ||
            size > directory.data.size() - at) {
          ++summary.invalid;
          at += 32;
          continue;
        }
        Bytes entries(p, p + size);
        at += size;
        File file;
        std::vector<uint32_t> clusters;
        try {
          const bool deleted = !(entries[0] & 0x80);
          uint16_t checksum = 0;
          for (size_t i = 0; i < entries.size(); ++i) {
            if (i == 2 || i == 3)
              continue;
            uint8_t value = entries[i];
            if (i % 32 == 0) {
              if (bool(value & 0x80) == deleted)
                throw std::runtime_error("Mixed exFAT entry states");
              value |= 0x80; // Restore InUse for retained deleted-set checksum.
            }
            checksum = uint16_t((checksum >> 1) +
                                ((checksum & 1) ? 0x8000 : 0) + value);
          }
          if (checksum != le(entries.data() + 2, 2) ||
              (entries[32] | 0x80) != 0xc0 || (entries[33] & ~3) ||
              !(entries[33] & 1))
            throw std::runtime_error("Invalid exFAT entry set");
          const bool isDirectory = (le(entries.data() + 4, 2) & 0x10) != 0;
          const auto valid = le(entries.data() + 40, 8);
          const auto length = le(entries.data() + 56, 8);
          if (valid > length || length > 2ULL * 1024 * 1024 * 1024 ||
              secondary != 1 + (entries[35] + 14) / 15)
            throw std::runtime_error(
                "Unsupported exFAT stream size or name set");
          const auto path = directory.path + name(entries, entries[35]);
          clusters = chain(uint32_t(le(entries.data() + 52, 4)), length,
                           entries[33] & 2);
          for (auto c : clusters)
            if (allocated(c) == deleted || metadata_.contains(c) ||
                owned_.contains(c))
              throw std::runtime_error("Conflicting exFAT allocation");
          if (isDirectory) {
            if (deleted)
              continue; // Do not infer descendants of deleted directories.
            if (directory.depth >= 32 || ++directories > 1024 ||
                valid != length || !length || length % g_.clusterBytes)
              throw std::runtime_error("Unsupported exFAT directory");
            auto data = metadata(clusters, length);
            metadata_.insert(clusters.begin(), clusters.end());
            pending.push_back(
                {std::move(data), path + "/", directory.depth + 1});
            continue;
          }
          if (!deleted)
            owned_.insert(clusters.begin(), clusters.end());
          file.path = (deleted ? "Deleted/" : "Existing/") + path;
          file.size = length;
          file.confidence = deleted ? "Deleted exFAT metadata and free "
                                      "clusters; contents unproven"
                                    : "exFAT entry checksum and allocation "
                                      "checked; contents unproven";
          file.read = [&, length, valid](uint64_t offset, size_t n) {
            return pull(clusters, length, valid, offset, n);
          };
        } catch (const std::runtime_error &) {
          check();
          ++summary.invalid;
          continue;
        }
        item(file); // Export/candidate-limit exceptions must propagate to
                    // caller.
        ++summary.found;
        if (progress)
          progress(metadataBytes_);
      }
    }
    return summary;
  }
};
} // namespace exfat
inline exfat::Summary
recoverExfatFiles(disk::IDiskReader &source, uint64_t start, uint64_t count,
                  const std::function<void(const exfat::File &)> &item,
                  const std::function<void(uint64_t)> &progress = {},
                  const std::function<bool()> &cancelled = {}) {
  return exfat::Reader(source, start, count, cancelled).scan(item, progress);
}
} // namespace recoverysuite::recovery
