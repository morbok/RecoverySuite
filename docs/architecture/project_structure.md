# RecoverySuite Project Structure

## Overview
This document describes the high-level directory structure of the RecoverySuite enterprise data recovery software. Each directory serves a specific purpose in maintaining separation of concerns, enabling extensibility, and supporting the plugin-based architecture.

## Directory Structure

```
/RecoverySuite
├── /Core                     # Fundamental system components
│   ├── /Logging              # Centralized logging system
│   ├── /Configuration        # Application configuration management
│   ├── /Utilities            # Cross-cutting helper functions
│   ├── /Types                # Shared data structures and interfaces
│   ├── /Memory               # Memory management utilities
│   └── /Concurrency          # Threading and async primitives
│
├── /GUI                      # Graphical User Interface layer
│   ├── /MainWindow           # Primary application window
│   ├── /Dialogs              # Reusable dialog components
│   ├── /Controls             # Custom UI controls
│   ├── /Themes               # UI theming system
│   └── /Resources            # Icons, images, and other assets
│
├── /Filesystem               # Filesystem-specific parsers and utilities
│   ├── /NTFS                 # NTFS filesystem implementation
│   ├── /FAT32                # FAT32 filesystem implementation
│   ├── /exFAT                # exFAT filesystem implementation
│   ├── /ReFS                 # ReFS filesystem implementation
│   ├── /EXT4                 # EXT4 filesystem implementation (read-only)
│   ├── /APFS                 # APFS filesystem implementation (read-only)
│   └── /Common               # Shared filesystem interfaces and utilities
│
├── /Recovery                 # Core recovery algorithms and engines
│   ├── /Scanning             # Filesystem scanning stages
│   ├── /Carving              # File carving algorithms
│   ├── /JournalAnalysis      # Transaction log analysis
│   ├── /MetadataRecovery     # Filesystem metadata reconstruction
│   ├── /Validation           # Recovery result validation
│   └── /Strategies           # Recovery strategy selection
│
├── /Disk                     # Low-level disk access and partitioning
│   ├── /Access               # Raw disk I/O abstractions
│   ├── /Partitioning         # Partition table parsing (MBR/GPT)
│   ├── /BootSectors          # Boot sector analysis and repair
│   └── /Volume               # Volume management and mounting
│
├── /SSD                      # Solid State Drive specific analysis
│   ├── /Detection            # TRIM status and SSD identification
│   ├── /Analysis             # SSD behavior and health analysis
│   ├── /NVMe                 # NVMe-specific utilities
│   ├── /SATAS                # SATA-specific utilities
│   └── /Vendor               # Vendor-specific behavior detection
│
├── /Drivers                  # Kernel-mode driver components (Windows only)
│   ├── /Filter               # File system filter driver
│   ├── /Communication        # User-kernel communication layer
│   └── /Utilities            # Driver-specific helper functions
│
├── /Plugins                  # Plugin system infrastructure
│   ├── /SDK                  # Plugin development kit
│   ├── /Manager              # Plugin loading and lifecycle management
│   ├── /Interfaces           # Plugin API contracts
│   └── /Marketplace          # Built-in and third-party plugins
│
├── /CLI                      # Command-line interface
│   ├── /Commands             # Individual CLI command implementations
│   ├── /Parser               # Argument parsing
│   └── /Output               # CLI output formatting
│
├── /Database                 # Recovery session data storage
│   ├── /Schema               # Database schema definitions
│   ├── /Access               # Data access layer
│   ├── /Migration            # Schema version management
│   └── /Cache                # Temporary data caching
│
├── /Tests                    # Comprehensive test suite
│   ├── /Unit                 # Unit tests for individual components
│   ├── /Integration          # Integration tests between modules
│   ├── /Filesystem           # Filesystem-specific tests
│   ├── /SSD                  # SSD analysis tests
│   ├── /Recovery             # Recovery engine tests
│   ├── /Mocks                # Test doubles and fixtures
│   └── /Fixtures             # Test data files
│
├── /Tools                    # Developer and diagnostic utilities
│   ├── /Builder              # Build automation scripts
│   ├── /Analyzer             # Code analysis tools
│   ├── /Debugger             # Debugging utilities
│   └── /Benchmarker          # Performance benchmarking tools
│
├── /Scripts                  # Automation and deployment scripts
│   ├── /Build                # Compilation and linking scripts
│   ├── /Deploy               # Deployment and packaging scripts
│   ├── /Test                 # Test execution scripts
│   └── /CI                   # Continuous integration scripts
│
├── /Documentation            # User and developer documentation
│   ├── /UserGuide            # End-user documentation
│   ├── /APIReference         # Developer API documentation
│   ├── /Architecture         # System architecture documents
│   └── /ReleaseNotes         # Version-specific release notes
│
�└── /ThirdParty               # External libraries and dependencies
    ├── /SDKs                 # Third-party SDKs
    ├── /Headers              # Header-only libraries
    └── /Binaries             # Pre-compiled binaries
```

## Design Principles

### Separation of Concerns
Each directory has a single, well-defined responsibility:
- Core provides foundational services used by all other modules
- Filesystem, Recovery, Disk, and SSD domains are isolated but collaborate through well-defined interfaces
- GUI and CLI are presentation layers that consume the same underlying services
- Plugins extend functionality without modifying core code

### Extensibility
- Plugin system allows adding new filesystem support, recovery algorithms, and SSD analysis techniques
- Abstract base classes and interfaces enable easy extension
- Configuration-driven behavior minimizes hardcoded assumptions

### Maintainability
- Clear directory hierarchy reduces cognitive load
- Consistent naming conventions across modules
- Minimal cross-directory dependencies
- Each module can be understood and modified in isolation

### Platform Abstraction
- Windows-specific code is isolated in Drivers and SSD directories
- Core and Filesystem layers strive for OS-agnostic design where possible
- Platform abstraction layers handle OS-specific differences

## Inter-Module Communication

Modules communicate through:
1. **Interface-based dependencies**: Modules depend on abstractions, not concrete implementations
2. **Event-driven notifications**: Loose coupling for asynchronous events
3. **Data transfer objects**: Structured data passed between layers
4. **Service locator**: Core services accessed through dependency injection

## Build Artifacts
- Static libraries for core modules (`RecoverySuite.Core.lib`, etc.)
- Dynamic link libraries for plugins (`*.plugin.dll`)
- Executable for GUI application (`RecoverySuite.GUI.exe`)
- Executable for CLI tool (`RecoverySuite.CLI.exe`)
- Kernel-mode driver (`RecoverySuite.Driver.sys`)

## Technology Stack
- **Language**: Modern C++20
- **GUI Framework**: To be determined (Qt, wxWidgets, or custom DirectX)
- **Build System**: CMake with vcpkg for dependency management
- **Database**: SQLite for session persistence
- **Logging**: spdlog or similar high-performance logging library
- **Testing**: Google Test and Google Mock
- **CI/CD**: GitHub Actions for automated builds and testing