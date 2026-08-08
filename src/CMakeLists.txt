# RecoverySuite Source Directory
add_subdirectory(Core)
add_subdirectory(Common)
add_subdirectory(Platform)
add_subdirectory(Logging)
add_subdirectory(Config)
add_subdirectory(Errors)
add_subdirectory(Utilities)
add_subdirectory(Disk)
add_subdirectory(Partition)
add_subdirectory(Storage)
add_subdirectory(Filesystem)
add_subdirectory(Application)
add_subdirectory(CLI)
add_subdirectory(GUI)

# Install headers from include directory
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include)