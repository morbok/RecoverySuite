#include "CLIHandler.h"

int main(int argc, char* argv[]) {
    recoverysuite::cli::CLIHandler cli;
    return cli.run(argc, argv);
}