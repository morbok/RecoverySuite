#include "NTFSAnalyzer.h"
#include "ntfs_boot_fields.h"
#include <stdexcept>
namespace recoverysuite::filesystem::ntfs {
NTFSAnalyzer::NTFSAnalyzer(std::shared_ptr<FilesystemReader> reader)
    :reader_(std::move(reader)),context_(std::make_shared<NTFSContext>(reader_)){}
bool NTFSAnalyzer::analyze(const FilesystemReader&)const{return false;} // Full consistency analysis is unavailable.
std::string NTFSAnalyzer::getAnalysisDescription()const{return "NTFS boot-field inspection; MFT and whole-volume consistency unexamined";}
std::string NTFSAnalyzer::getAnalysisDetails()const {
    return std::string("NTFS boot geometry: ")+(validateBootSector()?"valid":"invalid or unreadable")+
        "\nMFT: not examined by this analyzer; use the bounded image mft workflow\nCorruption: unknown\n";
}
bool NTFSAnalyzer::needsRepair()const{throw std::logic_error("NTFS repair needs are unknown without consistency analysis");}
uint32_t NTFSAnalyzer::getErrorCount()const{throw std::logic_error("NTFS corruption count is unknown without consistency analysis");}
bool NTFSAnalyzer::validateBootSector()const{return readNtfsBootFields(reader_.get()).has_value();}
}
