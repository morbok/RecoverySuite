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
