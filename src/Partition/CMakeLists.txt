# RecoverySuite Partition Module
add_library(RecoverySuite_Partition STATIC
    MBRHeader.cpp
    MBRPartitionEntry.cpp
    PartitionTable.cpp
    PartitionGeometry.cpp
    MBParser.cpp
    PartitionValidator.cpp
    PartitionManager.cpp
    GPTHeader.cpp
    GPTPartitionEntry.cpp
    GPTPartitionTable.cpp
    GPTParser.cpp
    GPTValidator.cpp
)

target_include_directories(RecoverySuite_Partition
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/Core
        ${PROJECT_BINARY_DIR}/src/Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Partition PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Partition
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
        RecoverySuite_Logging
        RecoverySuite_Errors
        RecoverySuite_Config
        RecoverySuite_Disk
)

# Install headers
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/RecoverySuite/Partition/ DESTINATION include/RecoverySuite/Partition
        FILES_MATCHING PATTERN "*.hpp")