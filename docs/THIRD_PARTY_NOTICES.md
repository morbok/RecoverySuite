# Third-party runtime notices

RecoverySuite uses Qt 6 Core, Gui and Widgets; Linux authorization also uses Qt DBus. Qt is dynamically linked. The Windows portable package bundles Qt 6.8.3 DLLs and plugins; its `third-party` directory contains license, copyright, attribution and related notice files collected from the exact upstream Qt Base, Qt SVG and Qt Image Formats source archives. The collection manifest records the source URL, archive SHA-256 and every notice hash. Qt Base/Image Formats include third-party components such as libpng, libjpeg-turbo, zlib, FreeType HarfBuzz and optional image codecs; their upstream notice files are retained in their source-relative locations.

Qt licensing: https://www.qt.io/licensing/open-source-lgpl-obligations
Qt source (matching Windows runtime): https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz
Qt source repository: https://code.qt.io/cgit/qt/qtbase.git/?h=v6.8.3

Linux packages link the distribution's Qt, D-Bus and libudev libraries, whose license texts are supplied by those system packages. UDisks2/polkit are separate system services, not bundled executables. Install the runtime dependencies from your distribution. The TGZ targets the same Ubuntu 24.04 environment as the DEB.

Windows additionally needs the Microsoft Visual C++ 2015–2022 x64 Redistributable, installed separately from Microsoft: https://aka.ms/vs/17/release/vc_redist.x64.exe . No compiler runtime installer is redistributed here.

This notice inventory does not choose a license for RecoverySuite or certify a distribution's legal compliance. The application owner must decide its license and complete independent release review before public distribution. Qt license alternatives and all applicable third-party terms must be evaluated for the chosen distribution. The application is not code signed.
