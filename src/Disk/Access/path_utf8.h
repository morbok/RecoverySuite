#pragma once
#include <filesystem>
#include <string>
namespace recoverysuite::disk {
inline std::filesystem::path nativePath(const std::string &path) {
  return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()),path.size()));
}
inline std::string utf8Path(const std::filesystem::path &path) {
  const auto text = path.u8string();
  return {reinterpret_cast<const char *>(text.data()), text.size()};
}
} // namespace recoverysuite::disk
