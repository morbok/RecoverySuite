# GUI Build Instructions

The GUI implementation for RecoverySuite is complete. To build and test the GUI, follow these steps:

## 1. Install Required Packages

You need to install the following development packages:
- cmake
- Qt6 base development libraries (qt6-qtbase-devel)
- Qt6 tools development libraries (qt6-qttools-devel)

On Fedora/RHEL systems, run:
```bash
sudo dnf install -y cmake qt6-qtbase-devel qt6-qttools-devel
```

## 2. Build the GUI

```bash
cd /home/jagermeister/Desktop/RecoverySuite
mkdir -p build && cd build
cmake ..
make
```

## 3. Verify the Build

After building, you can:
- Run the test suites: `ctest` or run specific test executables
- Launch the GUI application: `./bin/RecoverySuite_GUI` (or similar, depending on the install target)
- Verify that the main window appears and controls are responsive

## 4. Run Tests

Execute the test suite to ensure everything works:
```bash
ctest --verbose
```

Or run specific tests:
```bash
./bin/test_gui_recovery_service
./bin/test_phase13d_product_integration
```

## Notes

- The GUI module depends on the application layer services (RecoveryService, etc.)
- Ensure all dependencies are built before building the GUI
- If you encounter missing Qt6 components, verify that the Qt6 development packages are installed correctly

Once the GUI is built and verified, the system will be ready for PHASE 13E preparations.
