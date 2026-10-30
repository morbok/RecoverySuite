#include "CLIHandler.h"
#include <csignal>
#include <algorithm>
#include <cctype>
namespace {
volatile std::sig_atomic_t interrupted = 0;
extern "C" void requestCancellation(int) { interrupted = 1; }
}
int main(int argc, char* argv[]) {
    std::string command = argc > 1 ? argv[1] : "";
    std::transform(command.begin(),command.end(),command.begin(),[](unsigned char value){return static_cast<char>(std::tolower(value));});
    const bool cancellable = command == "image" && argc > 2 &&
        (std::string(argv[2]) == "carve" || std::string(argv[2]) == "metadata" || std::string(argv[2]) == "files");
    const auto previousInterrupt = cancellable ? std::signal(SIGINT, requestCancellation) : SIG_ERR;
    const auto previousTerminate = cancellable ? std::signal(SIGTERM, requestCancellation) : SIG_ERR;
    recoverysuite::cli::CLIHandler cli([] { return interrupted != 0; });
    const int result = cli.run(argc, argv);
    if (previousInterrupt != SIG_ERR) std::signal(SIGINT, previousInterrupt);
    if (previousTerminate != SIG_ERR) std::signal(SIGTERM, previousTerminate);
    return result;
}
