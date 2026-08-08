# RecoverySuite Tests
enable_testing()

# Google Test would be found here in a real implementation
# For now, we'll create a simple test that doesn't require external dependencies

add_executable(RecoverySuite_BasicTest
    test_basic.cpp
)

# Link with core libraries
target_link_libraries(RecoverySuite_BasicTest
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Logging
)

# Add the test to CTest
add_test(NAME RecoverySuite_BasicTest
         COMMAND RecoverySuite_BasicTest)

# Disk module test
add_executable(RecoverySuite_DiskTest
    test_disk.cpp
)

# Link with core and disk libraries
target_link_libraries(RecoverySuite_DiskTest
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
)

# Add the test to CTest
add_test(NAME RecoverySuite_DiskTest
         COMMAND RecoverySuite_DiskTest)

# Storage module test
add_executable(RecoverySuite_StorageTest
    test_storage.cpp
)

# Link with core and storage libraries
target_link_libraries(RecoverySuite_StorageTest
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Storage
)

# Platform-specific libraries for storage access (needed for static library linking)
if(UNIX AND NOT APPLE)
    target_link_libraries(RecoverySuite_StorageTest
        PRIVATE
            /usr/lib/x86_64-linux-gnu/libudev.so.1
    )
elseif(APPLE)
    target_link_libraries(RecoverySuite_StorageTest
        PRIVATE
            pthread
            IOKit
            CoreFoundation
    )
endif()

# Add the test to CTest
add_test(NAME RecoverySuite_StorageTest
         COMMAND RecoverySuite_StorageTest)

# Partition module test
add_executable(test_partition_mbr
    test_partition_mbr.cpp
)

# Link with core, disk, and partition libraries
target_link_libraries(test_partition_mbr
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Partition
)

# Add the test to CTest
add_test(NAME test_partition_mbr
         COMMAND test_partition_mbr)

# GPT module test
add_executable(test_gpt
    test_gpt.cpp
)

# Link with core, disk, and partition libraries
target_link_libraries(test_gpt
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Partition
)

# Add the test to CTest
add_test(NAME test_gpt
         COMMAND test_gpt)