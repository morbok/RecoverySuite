#include "CLIHandler.h"
#include <csignal>
namespace {
volatile std::sig_atomic_t interrupted = 0;
extern "C" void requestCancellation(int) { interrupted = 1; }
}
int main(int argc, char* argv[]) {
    const auto previousInterrupt = std::signal(SIGINT, requestCancellation);
    const auto previousTerminate = std::signal(SIGTERM, requestCancellation);
    recoverysuite::cli::CLIHandler cli([] { return interrupted != 0; });
    const int result = cli.run(argc, argv);
    if (previousInterrupt != SIG_ERR) std::signal(SIGINT, previousInterrupt);
    if (previousTerminate != SIG_ERR) std::signal(SIGTERM, previousTerminate);
    return result;
}
