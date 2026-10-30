#include "safe_file_exporter.h"
#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif
namespace recoverysuite::application::service {
#ifndef _WIN32
namespace {
class Descriptor {
public:
    explicit Descriptor(int value = -1) : value_(value) {}
    ~Descriptor() { if (value_ >= 0) ::close(value_); }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    int get() const { return value_; }
    void reset(int value) { if (value_ >= 0) ::close(value_); value_ = value; }
private:
    int value_;
};
void checkCancel(const std::function<bool()>& cancelled) {
    if (cancelled && cancelled()) throw std::runtime_error("Output cancelled");
}
}
struct SafeFileExporter::Impl { Descriptor directory; };
SafeFileExporter::SafeFileExporter(const std::string& directory) : impl_(std::make_unique<Impl>()) {
    if (directory.empty() || directory.find(char{0}) != std::string::npos)
        throw std::invalid_argument("Choose an existing output directory");
    const auto input = std::filesystem::path(directory);
    for (const auto& component : input) {
        if (component == "..") throw std::invalid_argument("Output directory must not contain parent traversal");
    }
    const auto path = std::filesystem::absolute(input).lexically_normal();
    impl_->directory.reset(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    if (impl_->directory.get() < 0) throw std::runtime_error("Cannot open output directory");
    for (const auto& part : path.relative_path()) {
        if (part.empty() || part == ".") continue;
        const int next = ::openat(impl_->directory.get(), part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0) throw std::runtime_error("Output directory is missing, inaccessible or contains a symlink");
        impl_->directory.reset(next);
    }
}
SafeFileExporter::~SafeFileExporter() = default;
namespace {
void writeAt(int directory, const std::string& name, const std::vector<uint8_t>& data, bool verify,
             const std::function<bool()>& cancelled) {
    if (name.empty() || name.size() > 255 || name == "." || name == ".." ||
        name.find_first_of("/\\:") != std::string::npos ||
        std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        throw std::invalid_argument("Unsafe recovered filename");
    checkCancel(cancelled);
    Descriptor output(::openat(directory, name.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (output.get() < 0) throw std::runtime_error("Cannot create output file; it may already exist");
    bool completed = false;
    // Remove only this operation's incomplete inode; never unlink a replacement or previous file.
    const auto cleanup = [&] {
        struct stat opened{}, current{};
        if (!completed && !::fstat(output.get(), &opened) &&
            !::fstatat(directory, name.c_str(), &current, AT_SYMLINK_NOFOLLOW) &&
            opened.st_dev == current.st_dev && opened.st_ino == current.st_ino)
            (void)::unlinkat(directory, name.c_str(), 0);
    };
    try {
        for (size_t offset = 0; offset < data.size();) {
            checkCancel(cancelled);
            const auto count = ::write(output.get(), data.data() + offset, std::min<size_t>(65536, data.size() - offset));
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) throw std::runtime_error("Output write failed");
            offset += static_cast<size_t>(count);
        }
        if (::fsync(output.get())) throw std::runtime_error("Output flush failed");
        if (verify) {
            std::vector<uint8_t> buffer(65536);
            for (size_t offset = 0; offset < data.size();) {
                checkCancel(cancelled);
                const auto count = ::pread(output.get(), buffer.data(), std::min(buffer.size(), data.size() - offset), static_cast<off_t>(offset));
                if (count < 0 && errno == EINTR) continue;
                if (count <= 0 || !std::equal(buffer.begin(), buffer.begin() + count, data.begin() + static_cast<std::ptrdiff_t>(offset)))
                    throw std::runtime_error("Output verification failed");
                offset += static_cast<size_t>(count);
            }
        }
        checkCancel(cancelled);
        struct stat opened{}, named{};
        if (::fstat(output.get(), &opened) ||
            ::fstatat(directory, name.c_str(), &named, AT_SYMLINK_NOFOLLOW) ||
            opened.st_dev != named.st_dev || opened.st_ino != named.st_ino ||
            opened.st_size < 0 || static_cast<uint64_t>(opened.st_size) != data.size())
            throw std::runtime_error("Output file changed before completion");
        if (::fsync(directory)) throw std::runtime_error("Output directory flush failed");
        completed = true;
    } catch (const std::exception&) { cleanup(); throw; }
}
} // namespace
void SafeFileExporter::write(const std::string& name, const std::vector<uint8_t>& data, bool verify,
                             const std::function<bool()>& cancelled) {
    writeAt(impl_->directory.get(),name,data,verify,cancelled);
}
void SafeFileExporter::writeTree(const std::string& path, const std::vector<uint8_t>& data, bool verify,
                                 const std::function<bool()>& cancelled) {
    if(path.empty()||path.size()>4095)throw std::invalid_argument("Invalid recovered relative path");
    std::vector<std::string> components;
    for(size_t start=0;start<=path.size();) {
        const auto end=path.find('/',start);
        const auto part=path.substr(start,end==std::string::npos?end:end-start);
        if(part.empty()||part.size()>255||part=="."||part==".."||part.find_first_of("\\:")!=std::string::npos||
           std::any_of(part.begin(),part.end(),[](unsigned char c){return c<32||c==127;}))
            throw std::invalid_argument("Unsafe recovered path component");
        components.push_back(part);
        if(end==std::string::npos)break;
        start=end+1;
    }
    if(components.size()>33)throw std::invalid_argument("Recovered directory depth exceeds 32");
    checkCancel(cancelled);
    Descriptor parent(::fcntl(impl_->directory.get(),F_DUPFD_CLOEXEC,0));
    if(parent.get()<0)throw std::runtime_error("Cannot anchor recovered directory");
    for(size_t i=0;i+1<components.size();++i) {
        checkCancel(cancelled);
        if(::mkdirat(parent.get(),components[i].c_str(),0700)&&errno!=EEXIST)
            throw std::runtime_error("Cannot create recovered directory");
        const int next=::openat(parent.get(),components[i].c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(next<0)throw std::runtime_error("Recovered directory is inaccessible or contains a symlink");
        Descriptor child(next);
        if(::fsync(parent.get()))throw std::runtime_error("Cannot flush recovered parent directory");
        parent.reset(::fcntl(child.get(),F_DUPFD_CLOEXEC,0));
        if(parent.get()<0)throw std::runtime_error("Cannot anchor recovered child directory");
    }
    writeAt(parent.get(),components.back(),data,verify,cancelled);
}
#else
struct SafeFileExporter::Impl {};
SafeFileExporter::SafeFileExporter(const std::string&) { throw std::runtime_error("Safe output export is currently supported on POSIX systems only"); }
SafeFileExporter::~SafeFileExporter() = default;
void SafeFileExporter::write(const std::string&, const std::vector<uint8_t>&, bool, const std::function<bool()>&) {
    throw std::runtime_error("Safe output export is unavailable");
}
void SafeFileExporter::writeTree(const std::string&, const std::vector<uint8_t>&, bool, const std::function<bool()>&) {
    throw std::runtime_error("Safe output export is unavailable");
}
#endif
}
