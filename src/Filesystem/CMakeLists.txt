# RecoverySuite Filesystem Module
add_library(RecoverySuite_Filesystem STATIC
    FilesystemTypes.h
    FilesystemExceptions.h
    Filesystem.h
    FilesystemReader.h
    FilesystemAnalyzer.h
    FilesystemRegistry.h
    FilesystemDriver.h
    FilesystemMetadata.h
    FilesystemVolume.h
    FilesystemManager.h
    SimpleFilesystemRegistry.h
    SimpleFilesystemRegistry.cpp

    # NTFS component
    NTFS/NTFSConstants.h
    NTFS/NTFSStructures.h
    NTFS/NTFSExceptions.h
    NTFS/NTFSContext.h
    NTFS/NTFSDriver.h
    NTFS/NTFSFilesystem.h
    NTFS/NTFSReader.h
    NTFS/NTFSAnalyzer.h
    NTFS/NTFSMetadata.h
    NTFS/NTFSVolume.h

    NTFS/NTFSDriver.cpp
    NTFS/NTFSFilesystem.cpp
    NTFS/NTFSReader.cpp
    NTFS/NTFSAnalyzer.cpp
    NTFS/NTFSMetadata.cpp
    NTFS/NTFSVolume.cpp

    # FAT component
    FAT/FATConstants.h
    FAT/FATStructures.h
    FAT/FATFilesystem.h
    FAT/FATReader.h
    FAT/FATAnalyzer.h
    FAT/FATMetadata.h
    FAT/FATVolume.h
    FAT/FATEntry.h
    FAT/FATClusterState.h
    FAT/FATTable.h
    FAT/FATTableParser.h
    FAT/FATChainReader.h
    FAT/FATValidator.h

    FAT/FATReader.cpp
    FAT/FATAnalyzer.cpp
    FAT/FATMetadata.cpp
    FAT/FATVolume.cpp
    FAT/FATFilesystem.cpp
    FAT/FATTable.cpp
    FAT/FATTableParser.cpp
    FAT/FATChainReader.cpp
    FAT/FATValidator.cpp
)

target_include_directories(RecoverySuite_Filesystem
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/Core
        ${PROJECT_BINARY_DIR}/src/Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Filesystem PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Filesystem
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
        RecoverySuite_Logging
        RecoverySuite_Errors
        RecoverySuite_Config
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Filesystem
        FILES_MATCHING PATTERN "*.h")
