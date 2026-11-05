#include "GUIRecoveryService.h"
#include "safe_file_exporter.h"
#include <set>

namespace recoverysuite::application::service {
std::vector<uint8_t>
GUIRecoveryService::previewCandidate(const std::string &name,
                                     const std::function<bool()> &cancelled) {
  std::unique_lock lock(imageMutex_, std::try_to_lock);
  if (!lock.owns_lock())
    throw std::runtime_error(
        "Wait for the current operation before previewing a file");
  if (!scanValid_ || !sourceReader_ || !scanCatalog_.contains(name))
    throw std::runtime_error("Choose a file from the current scan first");
  if (imageReader_)
    imageReader_->validateIdentity();
  if (deviceReader_)
    deviceReader_->validateIdentity();
  std::vector<uint8_t> bytes;
  bool found = false;
  const auto stopping = [&] { return found || (cancelled && cancelled()); };
  recovery::scanDiskFiles(
      *sourceReader_, scanVolumes_,
      [&](const std::string &path, uint64_t size, const auto &read,
          const std::string &) {
        if (path != name || stopping())
          return;
        if (size != scanCatalog_.at(name))
          throw std::runtime_error("The file changed since the scan");
        const auto count = static_cast<size_t>(std::min<uint64_t>(size, 65536));
        if (count)
          bytes = read(0, count);
        if (bytes.size() != count)
          throw std::runtime_error("Incomplete preview read");
        found = true;
      },
      {}, stopping);
  if (cancelled && cancelled())
    throw std::runtime_error("Preview cancelled");
  if (!found)
    throw std::runtime_error(
        "This candidate could not be read again; scan the drive again");
  return bytes;
}

models::StorageDevice GUIRecoveryService::openDevice(const std::string &path) {
  std::unique_lock lock(imageMutex_, std::try_to_lock);
  if (!lock.owns_lock())
    throw std::runtime_error(
        "Wait for the current scan before changing drives");
  if (deviceReader_ && path == sourcePath_) {
    deviceReader_->validateIdentity();
    return getStorageDeviceInfo(sourcePath_);
  }
  auto reader = std::make_shared<disk::ReadOnlyDevice>();
  reader->open(path);
  const auto info = reader->getDiskInfo();
  auto backend = std::make_unique<RecoveryService>(reader);
  sourceReader_ = reader;
  deviceReader_ = std::move(reader);
  imageReader_.reset();
  sourcePath_ = info.devicePath;
  recoveryService_ = std::move(backend);
  scanValid_ = false;
  scanVolumes_.clear();
  scanCatalog_.clear();
  return {info.devicePath,
          info.modelNumber,
          info.serialNumber,
          info.totalSizeBytes,
          static_cast<uint32_t>(info.bytesPerSector),
          true};
}

models::RecoveryResult GUIRecoveryService::executeDiskOperation(
    const models::RecoveryOperation &operation,
    const std::function<void(const models::RecoveryProgress &)> &progress,
    const std::function<bool()> &cancelled) {
  models::RecoveryResult result{};
  result.scanOnly = operation.operationType == "disk_scan";
  result.operationType = operation.operationType;
  result.destination = operation.outputPath;
  result.startTime = std::chrono::system_clock::now();
  result.processedBytesKnown = result.failedBytesKnown =
      result.failedItemsKnown = true;
  const auto stopping = [&] { return cancelled && cancelled(); };
  std::unique_lock lock(imageMutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    result.errorMessage = "A scan or recovery is already active";
    result.endTime = std::chrono::system_clock::now();
    return result;
  }
  const auto report = [&](const std::string &stage, uint64_t bytes,
                          uint64_t total) {
    result.processedBytes = bytes;
    if (!progress)
      return;
    models::RecoveryProgress p{};
    p.operationType = operation.operationType;
    p.status = "running";
    p.currentStep = stage;
    p.processedBytes = bytes;
    p.totalBytes = total;
    p.percentage =
        total ? uint32_t(std::min<long double>(100, 100.0L * bytes / total))
              : 0;
    p.recoveredItemsCount = result.recoveredItemsCount;
    p.failedItemsCount = result.failedItemsCount;
    p.isCancellable = true;
    p.timestamp = std::chrono::system_clock::now();
    p.elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                           p.timestamp - result.startTime)
                           .count();
    progress(p);
  };
  try {
    if (!lock.owns_lock())
      throw std::runtime_error("A scan or recovery is already active");
    const auto error = validateImageOperation(operation);
    if (!error.empty())
      throw std::runtime_error(error);
    if (operation.startSector ||
        operation.numSectors != sourceReader_->getTotalSectors())
      throw std::runtime_error(
          "Automatic recovery searches the whole selected drive");
    std::set<std::string> selected(operation.selectedPaths.begin(),
                                   operation.selectedPaths.end());
    std::unique_ptr<SafeFileExporter> exporter;
    if (result.scanOnly) {
      scanValid_ = false;
      scanCatalog_.clear();
      scanVolumes_.clear();
      scanVolumes_ =
          recovery::findDiskVolumes(*sourceReader_, report, cancelled);
    } else {
      if (!scanValid_ || selected.empty())
        throw std::runtime_error(
            "Scan the drive and choose files to recover first");
      for (const auto &name : selected)
        if (!scanCatalog_.contains(name))
          throw std::runtime_error(
              "Selection does not belong to the current drive scan");
      exporter = std::make_unique<SafeFileExporter>(operation.outputPath);
    }
    std::set<std::string> seen;
    const auto issues = recovery::scanDiskFiles(
        *sourceReader_, scanVolumes_,
        [&](const std::string &name, uint64_t size, const auto &read,
            const std::string &confidence) {
          if (stopping())
            return;
          if (result.scanOnly) {
            if (scanCatalog_.size() >= 10000)
              throw recovery::DiskCandidateLimit();
            if (!scanCatalog_.emplace(name, size).second)
              return;
            result.recoveredItems.push_back({name, size, confidence});
            ++result.recoveredItemsCount;
            result.recoveredBytes += size;
          } else if (selected.contains(name) && seen.insert(name).second) {
            try {
              if (scanCatalog_.at(name) != size)
                throw std::runtime_error(
                    "A selected file changed since the scan");
              if (deviceReader_)
                deviceReader_->validateDestination(operation.outputPath);
              exporter->writeStream(name, size, read, true, true, cancelled);
              result.outputWritten = true;
              ++result.recoveredItemsCount;
              result.recoveredBytes += size;
              result.recoveredItems.push_back({name, size, confidence});
            } catch (const std::exception &failure) {
              if (!stopping()) {
                ++result.failedItemsCount;
                result.failedBytes += size;
                result.details[name] = failure.what();
              }
            }
          }
        },
        report, cancelled);
    for (size_t i = 0; i < issues.size(); ++i)
      result.details["Scan issue " + std::to_string(i + 1)] = issues[i];
    if (!result.scanOnly && !stopping()) {
      for (const auto &name : selected)
        if (!seen.contains(name)) {
          ++result.failedItemsCount;
          result.failedBytes += scanCatalog_.at(name);
          result.details[name] =
              "File was not rediscovered; it was not recovered";
        }
    }
    scanValid_ = true;
    result.success = !stopping() && !result.failedItemsCount;
    if (result.failedItemsCount)
      result.errorMessage =
          "Some chosen files could not be recovered; review their errors";
  } catch (const recovery::DiskCandidateLimit &failure) {
    // Partial catalogs remain selectable; no source or output files were
    // written by the scan.
    scanValid_ = result.scanOnly;
    result.errorMessage = failure.what();
  } catch (const std::exception &failure) {
    result.errorMessage = failure.what();
  }
  result.cancelled = stopping();
  if (result.cancelled)
    result.success = false;
  result.endTime = std::chrono::system_clock::now();
  result.details["Source"] = sourcePath_;
  result.details["Filesystem volumes found"] =
      std::to_string(scanVolumes_.size());
  result.details["Supported recovery"] =
      "FAT12/16/32, exFAT 1.0 (single FAT), NTFS and JPEG/PDF/ZIP signatures. "
      "ext4, encrypted "
      "and compressed filesystem data are not supported yet.";
  result.details["Recovery limits"] =
      "Candidates can contain overwritten or incomplete bytes. Interrupted "
      "writes and SSD TRIM may prevent recovery. No partition repair or source "
      "writes are performed.";
  if (result.scanOnly)
    result.details["Scan"] = "Files listed for review; nothing has been saved. "
                             "Choose files and recover to another drive.";
  report(result.cancelled  ? "Cancelled"
         : result.scanOnly ? "Review found files"
                           : "Recovery finished",
         result.processedBytes, 0);
  return result;
}
} // namespace recoverysuite::application::service
