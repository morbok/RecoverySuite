#pragma once
#include "Disk/IDiskReader.hpp"
#include "Models/GUIModels.h"
#include "safe_file_exporter.h"
#include <QLockFile>
#include <map>
#include <memory>
#include <set>
namespace recoverysuite::application::service {
struct SessionDefinition {
  models::RecoveryOperation operation;
  uint32_t sectorSize{};
  std::string sourceHash, status;
};
class RecoverySession {
public:
  static SessionDefinition inspect(const std::string &path);
  RecoverySession(
      const models::RecoveryOperation &operation, disk::IDiskReader &source,
      SafeFileExporter &output, const std::function<bool()> &cancelled,
      const std::function<void(uint64_t)> &fingerprintProgress = {});
  // True means an earlier output was read back and verified; false means a new
  // output was saved.
  bool exportItem(const std::string &name, uint64_t size,
                  const SafeFileExporter::ReadAt &read, bool tree, bool verify);
  void finish(const std::string &status);

private:
  struct Entry {
    uint64_t size{};
    std::string hash;
    bool complete{};
  };
  models::RecoveryOperation operation_;
  uint32_t sectorSize_{};
  std::string sourceHash_, status_ = "running", filename_;
  std::unique_ptr<SafeFileExporter> state_;
  SafeFileExporter &output_;
  std::unique_ptr<QLockFile> lock_;
  std::function<bool()> cancelled_;
  std::map<std::string, Entry> entries_;
  std::set<std::string> seen_;
  void save();
};
} // namespace recoverysuite::application::service
