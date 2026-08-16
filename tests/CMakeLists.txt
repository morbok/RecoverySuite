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
            /usr/lib64/libudev.so.1
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

# FAT module test
add_executable(test_fat
    test_fat.cpp
)

# Link with core, disk, and filesystem libraries
target_link_libraries(test_fat
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Filesystem
)

# Add the test to CTest
add_test(NAME test_fat
         COMMAND test_fat)

# Carving robustness test
add_executable(test_carving_robustness
    test_carving_robustness.cpp
)

# Link with core, disk, and recovery libraries
target_link_libraries(test_carving_robustness
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Recovery
)

# Add the test to CTest
add_test(NAME test_carving_robustness
         COMMAND test_carving_robustness)

# Output failure handling test
add_executable(test_output_failure_handling
    test_output_failure_handling.cpp
)

# Link with core, disk, and recovery libraries
target_link_libraries(test_output_failure_handling
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Recovery
)

# Add the test to CTest
add_test(NAME test_output_failure_handling
         COMMAND test_output_failure_handling)

# Cancellation handling test
add_executable(test_cancellation_handling
    test_cancellation_handling.cpp
)

# Link with core, disk, and recovery libraries
target_link_libraries(test_cancellation_handling
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Recovery
)

# Add the test to CTest
add_test(NAME test_cancellation_handling
         COMMAND test_cancellation_handling)

# Recovery service test
add_executable(test_recovery_service
    test_recovery_service/test_recovery_service.cpp
)

# Link with core, disk, recovery, and application libraries
target_link_libraries(test_recovery_service
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Recovery
        RecoverySuite_Application
)

# Add the test to CTest
add_test(NAME test_recovery_service
         COMMAND test_recovery_service)

# Filesystem detector test
add_executable(test_filesystem_detector
    test_filesystem_detector/test_filesystem_detector.cpp
)

# Link with core, disk, and recovery libraries
target_link_libraries(test_filesystem_detector
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Recovery
)

# Add the test to CTest
add_test(NAME test_filesystem_detector
         COMMAND test_filesystem_detector)

# Disk reader interface test
add_executable(test_disk_reader_interface
    test_disk_reader_interface/test_disk_reader_interface.cpp
)

# Link with core and disk libraries
target_link_libraries(test_disk_reader_interface
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
)

# Add the test to CTest
add_test(NAME test_disk_reader_interface
         COMMAND test_disk_reader_interface)

# Filesystem analysis test
add_executable(test_filesystem_analysis
    test_filesystem_analysis/test_filesystem_analysis.cpp
)

# Link with core, disk, and recovery libraries
target_link_libraries(test_filesystem_analysis
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Recovery
)

# Add the test to CTest
add_test(NAME test_filesystem_analysis
         COMMAND test_filesystem_analysis)

# Metadata recovery security test
	add_executable(test_metadata_recovery_security
	    test_metadata_recovery_security/test_metadata_recovery_security.cpp
	)

	# Link with core, disk, and recovery libraries
	target_link_libraries(test_metadata_recovery_security
	    PRIVATE
	    RecoverySuite_Core
	    RecoverySuite_Disk
	    RecoverySuite_Recovery
	)

	# Add the test to CTest
	add_test(NAME test_metadata_recovery_security
	         COMMAND test_metadata_recovery_security)

# Application layer readiness test
	add_executable(test_application_layer_readiness
	    test_application_layer_readiness/test_application_layer_readiness.cpp
	)

	# Link with core, disk, recovery, application, and logging libraries
	target_link_libraries(test_application_layer_readiness
	    PRIVATE
	    RecoverySuite_Core
	    RecoverySuite_Disk
	    RecoverySuite_Recovery
	    RecoverySuite_Application
	    RecoverySuite_Logging
	)

	# Add the test to CTest
	add_test(NAME test_application_layer_readiness
	         COMMAND test_application_layer_readiness)

# Phase 13D Product Integration Validation Test
	add_executable(test_phase13d_product_integration
	    test_phase13d_product_integration.cpp
	)

	# Link with core, disk, recovery, application, and filesystem libraries
	target_link_libraries(test_phase13d_product_integration
	    PRIVATE
	    RecoverySuite_Core
	    RecoverySuite_Disk
	    RecoverySuite_Recovery
	    RecoverySuite_Application
	    RecoverySuite_Filesystem
	)

	# Add the test to CTest
	add_test(NAME test_phase13d_product_integration
	         COMMAND test_phase13d_product_integration)

# GUI interface simulation test
	add_executable(test_gui_interface_simulation
	    test_application_layer_readiness/test_gui_interface_simulation.cpp
	)

	# Link with core, disk, recovery, application, and logging libraries
	target_link_libraries(test_gui_interface_simulation
	    PRIVATE
	    RecoverySuite_Core
	    RecoverySuite_Disk
	    RecoverySuite_Recovery
	    RecoverySuite_Application
	    RecoverySuite_Logging
	)

	# Add the test to CTest
	add_test(NAME test_gui_interface_simulation
	         COMMAND test_gui_interface_simulation)

# CLI test
add_executable(RecoverySuite_CLI_Test
    test_cli/test_cli.cpp
    ${CMAKE_SOURCE_DIR}/src/CLI/CLIHandler.cpp
)

target_include_directories(RecoverySuite_CLI_Test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/CLI
)

target_link_libraries(RecoverySuite_CLI_Test
    PRIVATE
        RecoverySuite_Application
        RecoverySuite_Logging
        RecoverySuite_Config
        RecoverySuite_Utilities
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Recovery
)

# Add the test to CTest
add_test(NAME RecoverySuite_CLI_Test
         COMMAND RecoverySuite_CLI_Test)

# GUI state management test
add_executable(test_gui_state_management
    test_gui_state_management.cpp
)

# Link with core and GUI libraries
target_link_libraries(test_gui_state_management
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_GUI
)

# Add the test to CTest
add_test(NAME test_gui_state_management
         COMMAND test_gui_state_management)

# GUI recovery service test
add_executable(test_gui_recovery_service
    test_gui_recovery_service.cpp
)

# Link with core, disk, recovery, application, GUI, and logging libraries
target_link_libraries(test_gui_recovery_service
    PRIVATE
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Recovery
        RecoverySuite_Application
        RecoverySuite_GUI
        RecoverySuite_Logging
)

# Add the test to CTest
add_test(NAME test_gui_recovery_service
         COMMAND test_gui_recovery_service)