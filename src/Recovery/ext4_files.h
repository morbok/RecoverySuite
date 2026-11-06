#pragma once
#include "source_bytes.h"
#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
namespace recoverysuite::recovery::ext4 {
using Bytes = std::vector<uint8_t>;
inline uint64_t le(const uint8_t *p, size_t n) {
  uint64_t v = 0;
  for (size_t i = 0; i < n; ++i)
    v |= uint64_t(p[i]) << (8 * i);
  return v;
}
inline uint32_t crc32c(uint32_t crc, const uint8_t *data, size_t n) {
  static const auto table = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int b = 0; b < 8; ++b)
        c = (c >> 1) ^ ((c & 1) ? 0x82f63b78u : 0);
      t[i] = c;
    }
    return t;
  }();
  for (size_t i = 0; i < n; ++i)
    crc = table[(crc ^ data[i]) & 255] ^ (crc >> 8);
  return crc;
}
inline uint32_t crcNumber(uint32_t crc, uint32_t number) {
  uint8_t b[4];
  for (size_t i = 0; i < 4; ++i)
    b[i] = uint8_t(number >> (8 * i));
  return crc32c(crc, b, 4);
}
struct Geometry {
  uint64_t blocks{}, blockSize{}, first{}, perGroup{}, inodes{},
      inodesPerGroup{}, inodeSize{}, descSize{}, groups{};
};
inline Geometry geometry(const uint8_t *p, uint64_t availableBytes) {
  const auto fail = [] {
    throw std::runtime_error("Invalid or unsupported ext4 geometry/features");
  };
  const auto incompat = le(p + 96, 4), ro = le(p + 100, 4);
  if (le(p + 56, 2) != 0xef53 || le(p + 24, 4) > 2 || le(p + 90, 2) != 0 ||
      (incompat & ~uint64_t(0x62c6)) || (ro & (0x200 | 0x8000)) ||
      !(incompat & 0x40))
    fail();
  Geometry g;
  g.blockSize = 1024ULL << le(p + 24, 4);
  g.blocks = le(p + 4, 4) | ((incompat & 0x80) ? le(p + 336, 4) << 32 : 0);
  g.first = le(p + 20, 4);
  g.perGroup = le(p + 32, 4);
  g.inodes = le(p, 4);
  g.inodesPerGroup = le(p + 40, 4);
  g.inodeSize = le(p + 88, 2);
  g.descSize = (incompat & 0x80) ? le(p + 254, 2) : 32;
  if (!g.blocks || g.blocks > availableBytes / g.blockSize ||
      g.first != (g.blockSize == 1024 ? 1 : 0) || g.blocks <= g.first ||
      !g.perGroup || g.perGroup > g.blockSize * 8 || !g.inodesPerGroup ||
      g.inodesPerGroup > g.blockSize * 8 || g.inodeSize < 128 ||
      g.inodeSize > g.blockSize || (g.inodeSize & (g.inodeSize - 1)) ||
      (g.descSize != 32 && g.descSize != 64) ||
      ((incompat & 0x80) && g.descSize != 64))
    fail();
  g.groups = (g.blocks - g.first + g.perGroup - 1) / g.perGroup;
  if (!g.groups || g.groups > 4096 || !g.inodes ||
      g.inodes > g.groups * g.inodesPerGroup)
    fail();
  return g;
}
struct Extent {
  uint64_t logical{}, physical{}, count{};
  bool unwritten{};
};
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
  uint64_t start_, count_, readBytes_{};
  std::function<bool()> cancelled_;
  Geometry g_;
  bool checksums_{};
  uint32_t seed_{};
  struct Group {
    uint64_t blockBitmap, inodeBitmap, table;
    Bytes blocks, inodes;
    uint32_t blockChecksum{}, inodeChecksum{};
    uint16_t flags{};
  };
  std::vector<Group> groups_;
  std::vector<std::pair<uint64_t, uint64_t>> metadata_;
  std::set<uint64_t> extentNodes_;
  void check() const {
    if (cancelled_ && cancelled_())
      throw std::runtime_error("Cancelled");
  }
  Bytes bytes(uint64_t at, uint64_t n, bool metadata = false) {
    check();
    if (metadata && (readBytes_ + n > 64 * 1024 * 1024))
      throw std::runtime_error("ext4 metadata limit (64 MiB)");
    Bytes out;
    if (!readSourceBytes(&source_, start_, count_, at, n, out, cancelled_))
      throw std::runtime_error("Incomplete ext4 source read");
    if (metadata)
      readBytes_ += n;
    return out;
  }
  void blockRange(uint64_t at, uint64_t n) const {
    if (at < g_.first || !n || at >= g_.blocks || n > g_.blocks - at)
      throw std::runtime_error("ext4 block outside filesystem");
  }
  void protect(uint64_t at, uint64_t n) {
    blockRange(at, n);
    metadata_.push_back({at, at + n});
  }
  bool allocated(uint64_t block) {
    blockRange(block, 1);
    const auto group = (block - g_.first) / g_.perGroup,
               bit = (block - g_.first) % g_.perGroup;
    auto &record = groups_.at(size_t(group));
    auto &b = record.blocks;
    if (record.flags & 2)
      throw std::runtime_error("Uninitialized ext4 block bitmap");
    if (b.empty()) {
      b = bytes(record.blockBitmap * g_.blockSize, g_.blockSize, true);
      const auto digest =
          crc32c(seed_, b.data(), size_t((g_.perGroup + 7) / 8));
      if (checksums_ && (g_.descSize == 64 ? digest : digest & 0xffff) !=
                            record.blockChecksum)
        throw std::runtime_error("ext4 block bitmap checksum mismatch");
    }
    return (b.at(size_t(bit / 8)) & (1 << (bit % 8))) != 0;
  }
  Bytes inode(uint64_t number) {
    if (!number || number > g_.inodes)
      throw std::runtime_error("Invalid ext4 inode");
    auto &group = groups_.at(size_t((number - 1) / g_.inodesPerGroup));
    const auto index = (number - 1) % g_.inodesPerGroup;
    if (group.flags & 1)
      throw std::runtime_error("Uninitialized ext4 inode bitmap");
    if (group.inodes.empty()) {
      group.inodes =
          bytes(group.inodeBitmap * g_.blockSize, g_.blockSize, true);
      const auto digest = crc32c(seed_, group.inodes.data(),
                                 size_t((g_.inodesPerGroup + 7) / 8));
      if (checksums_ &&
          (g_.descSize == 64 ? digest : digest & 0xffff) != group.inodeChecksum)
        throw std::runtime_error("ext4 inode bitmap checksum mismatch");
    }
    if (!(group.inodes.at(size_t(index / 8)) & (1 << (index % 8))))
      throw std::runtime_error("Unallocated ext4 inode");
    auto record = bytes(group.table * g_.blockSize + index * g_.inodeSize,
                        g_.inodeSize, true);
    if (checksums_) {
      const bool high = record.size() >= 132 && le(record.data() + 128, 2) >= 4;
      const auto expected = le(record.data() + 124, 2) |
                            (high ? le(record.data() + 130, 2) << 16 : 0);
      const auto seed = crcNumber(crcNumber(seed_, uint32_t(number)),
                                  uint32_t(le(record.data() + 100, 4)));
      record[124] = record[125] = 0;
      if (high)
        record[130] = record[131] = 0;
      const auto digest = crc32c(seed, record.data(), record.size());
      if ((high ? digest : digest & 0xffff) != expected)
        throw std::runtime_error("ext4 inode checksum mismatch");
    }
    const auto attributes =
        le(record.data() + 104, 4) |
        (g_.descSize == 64 ? le(record.data() + 118, 2) << 32 : 0);
    if (attributes)
      protect(attributes,
              1); // Extended attributes are metadata, not file data.
    return record;
  }
  void extents(const Bytes &node, unsigned expected,
               std::vector<Extent> &result, std::set<uint64_t> &visited,
               uint64_t lower = 0, uint64_t upper = 0x100000000ULL,
               uint32_t checksumSeed = 0) {
    if (node.size() < 12 || le(node.data(), 2) != 0xf30a ||
        le(node.data() + 6, 2) != expected || expected > 5)
      throw std::runtime_error("Invalid ext4 extent header");
    const auto entries = le(node.data() + 2, 2),
               capacity = le(node.data() + 4, 2);
    if (!capacity || entries > capacity || capacity > (node.size() - 12) / 12)
      throw std::runtime_error("Invalid ext4 extent count");
    if (checksums_ && node.size() != 60) {
      const auto tail = 12 + size_t(capacity) * 12;
      if (tail + 4 > node.size() ||
          crc32c(checksumSeed, node.data(), tail) != le(node.data() + tail, 4))
        throw std::runtime_error("ext4 extent block checksum mismatch");
    }
    uint64_t prior = lower;
    for (uint64_t i = 0; i < entries; ++i) {
      check();
      const auto *p = node.data() + 12 + i * 12;
      const auto logical = le(p, 4);
      if (logical < prior || logical >= upper)
        throw std::runtime_error("Unordered ext4 extent tree");
      if (expected) {
        const auto child = le(p + 4, 4) | (le(p + 8, 2) << 32);
        const auto next = i + 1 < entries ? le(p + 12, 4) : upper;
        if (next <= logical || next > upper || !visited.insert(child).second ||
            visited.size() > 4096)
          throw std::runtime_error("Cyclic or excessive ext4 extent tree");
        blockRange(child, 1);
        if (!allocated(child))
          throw std::runtime_error("Unallocated ext4 extent node");
        protect(child, 1);
        extentNodes_.insert(child);
        extents(bytes(child * g_.blockSize, g_.blockSize, true), expected - 1,
                result, visited, logical, next, checksumSeed);
        prior = next;
      } else {
        const auto raw = le(p + 4, 2);
        const bool unwritten = raw > 32768;
        const auto length = unwritten ? raw - 32768 : raw;
        const auto physical = le(p + 8, 4) | (le(p + 6, 2) << 32);
        if (!length || length > upper - logical || result.size() >= 100000)
          throw std::runtime_error("Invalid ext4 leaf extent");
        blockRange(physical, length);
        for (uint64_t j = 0; j < length; ++j) {
          if ((j & 1023) == 0)
            check();
          if (!allocated(physical + j))
            throw std::runtime_error("Unallocated ext4 data extent");
        }
        result.push_back({logical, physical, length, unwritten});
        prior = logical + length;
      }
    }
  }
  struct Stream {
    uint64_t size;
    std::vector<Extent> extents;
  };
  Stream stream(const Bytes &ino, uint64_t number) {
    const auto flags = le(ino.data() + 32, 4);
    if (le(ino.data() + 20, 4) || !(flags & 0x80000) ||
        (flags & (0x4 | 0x800 | 0x100000 | 0x10000000)))
      throw std::runtime_error("Unsupported or deleted ext4 inode data");
    Stream s{le(ino.data() + 4, 4) | (le(ino.data() + 108, 4) << 32), {}};
    if (s.size > 2ULL * 1024 * 1024 * 1024)
      throw std::runtime_error("ext4 file exceeds 2 GiB");
    Bytes root(ino.begin() + 40, ino.begin() + 100);
    std::set<uint64_t> visited;
    const auto seed = crcNumber(crcNumber(seed_, uint32_t(number)),
                                uint32_t(le(ino.data() + 100, 4)));
    extents(root, unsigned(le(root.data() + 6, 2)), s.extents, visited, 0,
            0x100000000ULL, seed);
    return s;
  }
  Bytes pull(const Stream &s, uint64_t at, size_t n) {
    if (at > s.size || n > s.size - at || n > 65536)
      throw std::runtime_error("Invalid ext4 file read");
    Bytes out(n, 0);
    size_t done = 0;
    while (done < n) {
      check();
      const auto logical = at / g_.blockSize, within = at % g_.blockSize;
      const auto take = std::min<uint64_t>(n - done, g_.blockSize - within);
      for (const auto &extent : s.extents)
        if (logical >= extent.logical &&
            logical - extent.logical < extent.count) {
          if (!extent.unwritten) {
            auto part = bytes((extent.physical + logical - extent.logical) *
                                      g_.blockSize +
                                  within,
                              take);
            std::copy(part.begin(), part.end(), out.begin() + done);
          }
          break;
        }
      done += size_t(take);
      at += take;
    }
    return out;
  }
  static std::string name(const uint8_t *p, size_t n) {
    std::string s(reinterpret_cast<const char *>(p), n);
    if (s.empty() || s == "." || s == ".." || s.back() == '.' ||
        s.back() == ' ')
      throw std::runtime_error("Unsafe ext4 name");
    for (size_t i = 0; i < n;) {
      uint32_t c = p[i++];
      unsigned extra = 0;
      uint32_t minimum = 0;
      if (c >= 0xc2 && c <= 0xdf) {
        extra = 1;
        minimum = 0x80;
        c &= 31;
      } else if (c >= 0xe0 && c <= 0xef) {
        extra = 2;
        minimum = 0x800;
        c &= 15;
      } else if (c >= 0xf0 && c <= 0xf4) {
        extra = 3;
        minimum = 0x10000;
        c &= 7;
      } else if (c >= 0x80)
        throw std::runtime_error("Non-UTF8 ext4 name unsupported");
      if (extra > n - i)
        throw std::runtime_error("Truncated ext4 name");
      for (unsigned j = 0; j < extra; ++j) {
        const auto b = p[i++];
        if ((b & 0xc0) != 0x80)
          throw std::runtime_error("Invalid UTF8 ext4 name");
        c = (c << 6) | (b & 63);
      }
      if ((extra && c < minimum) || c > 0x10ffff ||
          (c >= 0xd800 && c <= 0xdfff) || c < 32 ||
          (c < 128 &&
           std::string("/\\:\"*?<>|").find(char(c)) != std::string::npos))
        throw std::runtime_error("Unsafe ext4 name");
    }
    return s;
  }

public:
  Reader(disk::IDiskReader &source, uint64_t start, uint64_t count,
         std::function<bool()> cancelled)
      : source_(source), start_(start), count_(count),
        cancelled_(std::move(cancelled)) {
    const auto sb = bytes(1024, 1024, true);
    g_ = geometry(sb.data(), count * uint64_t(source.getSectorSize()));
    checksums_ = (le(sb.data() + 100, 4) & 0x400) != 0;
    seed_ = (le(sb.data() + 96, 4) & 0x2000)
                ? uint32_t(le(sb.data() + 624, 4))
                : crc32c(0xffffffffu, sb.data() + 104, 16);
    if (checksums_ && (sb[373] != 1 || crc32c(0xffffffffu, sb.data(), 1020) !=
                                           le(sb.data() + 1020, 4)))
      throw std::runtime_error("ext4 superblock checksum mismatch");
    const auto descriptorStart = g_.first + 1;
    const auto descriptorBytes = g_.groups * g_.descSize;
    const auto descriptors =
        bytes(descriptorStart * g_.blockSize, descriptorBytes, true);
    protect(g_.first, 1 + (descriptorBytes + g_.blockSize - 1) / g_.blockSize);
    for (uint64_t i = 0; i < g_.groups; ++i) {
      const auto *p = descriptors.data() + i * g_.descSize;
      const auto high = [&](size_t lo, size_t hi) {
        return le(p + lo, 4) | (g_.descSize == 64 ? le(p + hi, 4) << 32 : 0);
      };
      Bytes descriptor(p, p + g_.descSize);
      if (checksums_) {
        const auto expected = le(p + 30, 2);
        descriptor[30] = descriptor[31] = 0;
        if ((crc32c(crcNumber(seed_, uint32_t(i)), descriptor.data(),
                    descriptor.size()) &
             0xffff) != expected)
          throw std::runtime_error("ext4 group descriptor checksum mismatch");
      }
      Group group{high(0, 32),
                  high(4, 36),
                  high(8, 40),
                  {},
                  {},
                  uint32_t(le(p + 24, 2) |
                           (g_.descSize == 64 ? le(p + 56, 2) << 16 : 0)),
                  uint32_t(le(p + 26, 2) |
                           (g_.descSize == 64 ? le(p + 58, 2) << 16 : 0)),
                  uint16_t(le(p + 18, 2))};
      protect(group.blockBitmap, 1);
      protect(group.inodeBitmap, 1);
      protect(group.table,
              (g_.inodesPerGroup * g_.inodeSize + g_.blockSize - 1) /
                  g_.blockSize);
      groups_.push_back(std::move(group));
    }
  }
  Summary scan(const std::function<void(const File &)> &item,
               const std::function<void(uint64_t)> &progress) {
    Summary summary;
    struct Directory {
      uint64_t inode;
      std::string path;
      unsigned depth;
    };
    struct Candidate {
      File file;
      Stream stream;
    };
    std::vector<Directory> pending{{2, "", 0}};
    std::set<uint64_t> directories{2}, files;
    std::vector<Candidate> candidates;
    uint64_t extentCount = 0;
    // Reserved inodes contain journal/resize metadata, not user files.
    for (uint64_t n = 1; n <= 10 && n <= g_.inodes; ++n)
      if (n != 2) {
        try {
          const auto ino = inode(n);
          const auto s = stream(ino, n);
          for (const auto &e : s.extents)
            protect(e.physical, e.count);
        } catch (const std::runtime_error &) {
          check();
        }
      }
    while (!pending.empty()) {
      check();
      auto directory = std::move(pending.back());
      pending.pop_back();
      try {
        const auto ino = inode(directory.inode);
        if ((le(ino.data(), 2) & 0xf000) != 0x4000)
          throw std::runtime_error("ext4 directory type mismatch");
        if (checksums_ && (le(ino.data() + 32, 4) & 0x1000))
          throw std::runtime_error(
              "Checksummed ext4 indexed directories are unsupported");
        const auto data = stream(ino, directory.inode);
        if (!data.size || data.size > 16 * 1024 * 1024 ||
            data.size % g_.blockSize)
          throw std::runtime_error("ext4 directory size unsupported");
        for (const auto &e : data.extents)
          protect(e.physical, e.count);
        for (uint64_t at = 0; at < data.size; at += g_.blockSize) {
          const auto block = pull(data, at, size_t(g_.blockSize));
          if (checksums_) {
            const auto tail = block.data() + block.size() - 12;
            const auto seed =
                crcNumber(crcNumber(seed_, uint32_t(directory.inode)),
                          uint32_t(le(ino.data() + 100, 4)));
            if (le(tail, 4) || le(tail + 4, 2) != 12 || tail[6] ||
                tail[7] != 0xde ||
                crc32c(seed, block.data(), block.size() - 12) !=
                    le(tail + 8, 4))
              throw std::runtime_error(
                  "ext4 directory block checksum mismatch");
          }
          for (size_t pos = 0; pos + 8 <= block.size();) {
            const auto *p = block.data() + pos;
            const auto number = le(p, 4), length = le(p + 4, 2);
            const auto namesize = p[6];
            if (length < 8 || length % 4 || length > block.size() - pos ||
                namesize > length - 8)
              throw std::runtime_error("Invalid ext4 directory entry");
            pos += size_t(length);
            if (!number || (namesize == 1 && p[8] == '.') ||
                (namesize == 2 && p[8] == '.' && p[9] == '.'))
              continue;
            try {
              const auto path = directory.path + name(p + 8, namesize);
              if (path.size() > 4095)
                throw std::runtime_error("ext4 path limit");
              const auto child = inode(number);
              const auto mode = le(child.data(), 2) & 0xf000;
              if (mode == 0x4000) {
                if (directory.depth >= 32 || directories.size() >= 1024 ||
                    !directories.insert(number).second)
                  throw std::runtime_error(
                      "Cyclic or excessive ext4 directory tree");
                pending.push_back({number, path + "/", directory.depth + 1});
              } else if (mode == 0x8000 && files.insert(number).second) {
                auto content = stream(child, number);
                extentCount += content.extents.size();
                if (candidates.size() >= 10000 || extentCount > 100000)
                  throw std::runtime_error("ext4 catalog limit");
                File file{"Existing/" + path,
                          checksums_ ? "ext4 metadata checksums and allocation "
                                       "checked; contents unproven"
                                     : "ext4 allocation records; no metadata "
                                       "checksums; contents unproven",
                          content.size,
                          {}};
                candidates.push_back({std::move(file), std::move(content)});
              }
            } catch (const std::runtime_error &) {
              check();
              ++summary.invalid;
            }
          }
        }
        if (progress)
          progress(readBytes_);
      } catch (const std::runtime_error &) {
        check();
        ++summary.invalid;
      }
    }
    struct Range {
      uint64_t begin, end;
      size_t candidate;
    };
    std::vector<Range> ranges;
    std::set<size_t> conflicts;
    for (size_t i = 0; i < candidates.size(); ++i)
      for (const auto &e : candidates[i].stream.extents)
        ranges.push_back({e.physical, e.physical + e.count, i});
    std::sort(metadata_.begin(), metadata_.end());
    std::vector<std::pair<uint64_t, uint64_t>> merged;
    for (const auto &r : metadata_) {
      if (!merged.empty() && r.first <= merged.back().second)
        merged.back().second = std::max(merged.back().second, r.second);
      else
        merged.push_back(r);
    }
    std::sort(ranges.begin(), ranges.end(),
              [](auto a, auto b) { return a.begin < b.begin; });
    uint64_t maximumEnd = 0;
    size_t owner = 0;
    for (const auto &r : ranges) {
      if (r.begin < maximumEnd) {
        conflicts.insert(r.candidate);
        conflicts.insert(owner);
      }
      if (r.end > maximumEnd) {
        maximumEnd = r.end;
        owner = r.candidate;
      }
      const auto it =
          std::lower_bound(merged.begin(), merged.end(), r.end,
                           [](auto a, uint64_t end) { return a.first < end; });
      if (it != merged.begin() && std::prev(it)->second > r.begin)
        conflicts.insert(r.candidate);
    }
    for (size_t i = 0; i < candidates.size(); ++i) {
      check();
      if (conflicts.contains(i)) {
        ++summary.invalid;
        continue;
      }
      auto &candidate = candidates[i];
      candidate.file.read = [&](uint64_t at, size_t n) {
        return pull(candidate.stream, at, n);
      };
      item(candidate.file);
      ++summary.found;
    }
    return summary;
  }
};
} // namespace recoverysuite::recovery::ext4
