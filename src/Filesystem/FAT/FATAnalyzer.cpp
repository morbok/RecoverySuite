#include "FATAnalyzer.h"
#include <stdexcept>

namespace recoverysuite {
namespace filesystem {
namespace fat {

FATAnalyzer::FATAnalyzer(const FilesystemReader &reader) { (void)reader; }

bool FATAnalyzer::analyze(const FilesystemReader &reader) const {
  (void)reader;
  throw std::logic_error(
      "FAT whole-volume consistency analysis is unavailable");
}

std::string FATAnalyzer::getAnalysisDescription() const {
  return "FAT whole-volume consistency analysis unavailable";
}

std::string FATAnalyzer::getAnalysisDetails() const {
  return "Allocation chains and directory consistency: unexamined; corruption "
         "and repair needs: unknown";
}

bool FATAnalyzer::needsRepair() const {
  throw std::logic_error(
      "FAT repair needs are unknown without consistency analysis");
}

uint32_t FATAnalyzer::getErrorCount() const {
  throw std::logic_error(
      "FAT corruption count is unknown without consistency analysis");
}

} // namespace fat
} // namespace filesystem
} // namespace recoverysuite
