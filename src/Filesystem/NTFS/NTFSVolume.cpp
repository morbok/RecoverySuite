#include "NTFSVolume.h"
#include "ntfs_boot_fields.h"
#include <limits>
#include <stdexcept>
namespace recoverysuite::filesystem::ntfs {
NTFSVolume::NTFSVolume(std::shared_ptr<FilesystemReader> reader)
    :reader_(std::move(reader)),context_(std::make_shared<NTFSContext>(reader_)){}
std::string NTFSVolume::getVolumeIdentifier()const{return reader_?"NTFS volume":"Unknown Volume";}
uint64_t NTFSVolume::getStartOffset()const{return reader_?reader_->getStartOffset():0;}
uint64_t NTFSVolume::getSize()const{const auto boot=readNtfsBootFields(reader_.get());return boot?boot->sectors*boot->sectorBytes:0;}
uint64_t NTFSVolume::getEndOffset()const{const auto start=getStartOffset(),size=getSize();return !size||size>std::numeric_limits<uint64_t>::max()-start?0:start+size;}
bool NTFSVolume::isBootable()const{throw std::logic_error("Bootability requires partition/boot-loader metadata");}
bool NTFSVolume::isActive()const{throw std::logic_error("Active status requires partition metadata");}
bool NTFSVolume::isHidden()const{throw std::logic_error("Hidden status requires partition metadata");}
uint8_t NTFSVolume::getPartitionType()const{return 0;} // Unknown; do not infer an MBR type from a filesystem.
FilesystemType NTFSVolume::getFilesystemType()const{return FilesystemType::NTFS;}
bool NTFSVolume::isValid()const{return readNtfsBootFields(reader_.get()).has_value();} // Boot geometry only.
bool NTFSVolume::readVolumeInfo()const{return readNtfsBootFields(reader_.get()).has_value();}
}
