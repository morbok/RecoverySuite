#include "CLIHandler.h"
#include <algorithm>
#include <cctype>
#include <csignal>
#ifdef _WIN32
#include <vector>
#include <windows.h>
#endif
namespace {
volatile std::sig_atomic_t interrupted = 0;
extern "C" void requestCancellation(int) { interrupted = 1; }
} // namespace
int runCli(int argc, char *argv[]) {
  std::string command = argc > 1 ? argv[1] : "";
  std::transform(command.begin(), command.end(), command.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });
  const bool cancellable =
      command == "session" || command == "device" ||
      (command == "image" && argc > 2 &&
       (std::string(argv[2]) == "carve" || std::string(argv[2]) == "metadata" ||
        std::string(argv[2]) == "files" || std::string(argv[2]) == "tree" ||
        std::string(argv[2]) == "mft" || std::string(argv[2]) == "ntfs-files" ||
        std::string(argv[2]) == "ntfs-tree" ||
        std::string(argv[2]) == "ntfs-checked" ||
        std::string(argv[2]) == "ntfs-deleted" ||
        std::string(argv[2]) == "fat-deleted"));
  const auto previousInterrupt =
      cancellable ? std::signal(SIGINT, requestCancellation) : SIG_ERR;
  const auto previousTerminate =
      cancellable ? std::signal(SIGTERM, requestCancellation) : SIG_ERR;
  recoverysuite::cli::CLIHandler cli([] { return interrupted != 0; });
  const int result = cli.run(argc, argv);
  if (previousInterrupt != SIG_ERR)
    std::signal(SIGINT, previousInterrupt);
  if (previousTerminate != SIG_ERR)
    std::signal(SIGTERM, previousTerminate);
  return result;
}

#ifdef _WIN32
int wmain(int argc, wchar_t *argv[]) {
  std::vector<std::string> arguments;
  arguments.reserve(static_cast<size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    const int length =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, nullptr,
                            0, nullptr, nullptr);
    if (!length)
      return 1;
    std::string value(static_cast<size_t>(length), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1,
                             value.data(), length, nullptr, nullptr))
      return 1;
    value.pop_back();
    arguments.push_back(std::move(value));
  }
  std::vector<char *> pointers;
  for (auto &value : arguments)
    pointers.push_back(value.data());
  pointers.push_back(nullptr);
  return runCli(argc, pointers.data());
}
#else
int main(int argc, char *argv[]) { return runCli(argc, argv); }
#endif
