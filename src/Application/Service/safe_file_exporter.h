#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
namespace recoverysuite::application::service {
/** Anchors an existing destination directory and creates recovered files exclusively.
 * POSIX directory handles reject symlinks in every component, including races after validation.
 * Windows export fails closed until equivalent handle-based protection is implemented.
 * Not thread-safe; own one instance for an operation. Existing files are never replaced.
 */
class SafeFileExporter {
public:
    explicit SafeFileExporter(const std::string& directory);
    ~SafeFileExporter();
    SafeFileExporter(const SafeFileExporter&) = delete;
    SafeFileExporter& operator=(const SafeFileExporter&) = delete;
    using ReadAt = std::function<std::vector<uint8_t>(uint64_t,size_t)>;
    // Pulls <=64 KiB at a time; exact-length chunks required. Cleanup and no-overwrite match write().
    void writeStream(const std::string& path,uint64_t size,const ReadAt& read,bool verify,bool tree,
                     const std::function<bool()>& cancelled = {});
    // Relative tree paths are validated component by component; directories use mode 0700.
    // Existing real directories may be reused; symlinks and existing files are rejected.
    void writeTree(const std::string& path, const std::vector<uint8_t>& data, bool verify,
                   const std::function<bool()>& cancelled = {});
    void write(const std::string& name, const std::vector<uint8_t>& data, bool verify,
               const std::function<bool()>& cancelled = {});
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
