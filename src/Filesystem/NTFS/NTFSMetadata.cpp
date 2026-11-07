#include "NTFSMetadata.h"
#include "ntfs_boot_fields.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace recoverysuite::filesystem::ntfs {
NTFSMetadata::NTFSMetadata(std::shared_ptr<FilesystemReader> reader)
    :reader_(std::move(reader)),context_(std::make_shared<NTFSContext>(reader_)){}
FilesystemType NTFSMetadata::getType()const{return FilesystemType::NTFS;}
std::string NTFSMetadata::getName()const{return "NTFS";}
std::string NTFSMetadata::getVersion()const{return "Unknown";} // Requires the $Volume record.
std::string NTFSMetadata::getVolumeLabel()const{return "Unknown";}
std::string NTFSMetadata::getSerialNumber()const {
    const auto boot=readNtfsBootFields(reader_.get());if(!boot)return {};
    std::ostringstream value;value<<std::hex<<std::uppercase<<std::setfill('0')<<std::setw(16)<<boot->serial;return value.str();
}
uint64_t NTFSMetadata::getTotalSize()const{const auto boot=readNtfsBootFields(reader_.get());return boot?boot->sectors*boot->sectorBytes:0;}
uint32_t NTFSMetadata::getBlockSize()const{const auto boot=readNtfsBootFields(reader_.get());return boot?static_cast<uint32_t>(boot->clusterBytes):0;}
uint64_t NTFSMetadata::getBlockCount()const{const auto boot=readNtfsBootFields(reader_.get());return boot?boot->sectors/(boot->clusterBytes/boot->sectorBytes):0;}
uint64_t NTFSMetadata::getFreeSpace()const{throw std::logic_error("NTFS free space is unknown: allocation bitmap was not examined");}
uint64_t NTFSMetadata::getUsedSpace()const{throw std::logic_error("NTFS used space is unknown: allocation bitmap was not examined");}
uint64_t NTFSMetadata::getFreeBlockCount()const{throw std::logic_error("NTFS free blocks are unknown: allocation bitmap was not examined");}
bool NTFSMetadata::isReadOnly()const{return true;} // FilesystemReader exposes read-only access.
bool NTFSMetadata::isMounted()const{throw std::logic_error("Mount state is unavailable through FilesystemReader");}
bool NTFSMetadata::readBootSectorInfo()const{return readNtfsBootFields(reader_.get()).has_value();}
bool NTFSMetadata::readVolumeInfo()const{return false;} // $Volume attributes have not been read.
}
