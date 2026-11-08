# Install RecoverySuite on Linux

The x86_64 Flatpak packages RecoverySuite against the KDE/Qt runtime, so you do not
need Ubuntu or Ubuntu's Qt libraries. CachyOS/Arch, Fedora and other Linux desktops
can install it when Flatpak and a compatible x86_64 system are available.

## Install

Download **RecoverySuite-Flatpak-x86_64** from a successful
[Linux Flatpak workflow run](https://github.com/morbok/RecoverySuite/actions/workflows/flatpak.yml)
and extract the ZIP. Open a terminal in the extracted folder:

```sh
flatpak install --user ./RecoverySuite-x86_64.flatpak
```

Flatpak offers to install the matching KDE runtime from Flathub if it is missing.
This first download can be large. If Flathub has not been configured, add it once:

```sh
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
```

Open **RecoverySuite** from your application menu, or run:

```sh
flatpak run io.github.morbok.RecoverySuite
```

Flatpak itself can be installed on CachyOS/Arch with `sudo pacman -S flatpak`.
The application is not currently listed on Flathub. This is a manually installed
development bundle; updates require installing a newer verified bundle. The shared
KDE runtime can be updated normally with `flatpak update`.

## Source disks and permissions

The bundle requests device access and host-file access for source selection,
disk images and recovery destinations. It has no network permission. It may
communicate only with the host UDisks2 service on the system bus; it does not
request an unrestricted system bus or host-command execution.

Device permission does not make the application root. Choose **Authorize read-only
access…** if a source cannot be opened. The host must have UDisks2, polkit and a
working desktop authentication agent. Source volumes must be unmounted first.
The application does not unmount, repair or write to the source.

Destination folders must be on another physical disk. Use a normal host folder
such as `/run/media/<user>/<destination>/Recovered`. Do not grant extra global
permissions or run Flatpak as root to bypass a rejected source.

## Verification and removal

`manifest.json` identifies the source revision and CI run. Verify the file with:

```sh
sha256sum -c RecoverySuite-x86_64.flatpak.sha256
```

Remove the application with `flatpak uninstall --user io.github.morbok.RecoverySuite`.

Builds and installed-package checks run on disposable GitHub runners. No
application execution or disk tests are performed on the user's workstation.
Physical hardware, desktop authentication and failing-drive behavior still need
separate qualification; passing sandbox tests does not establish those results.

## Maintainer build (CI / evaluation machines)

```sh
git archive --format=tar --output=packaging/flatpak/recoverysuite-source.tar HEAD
flatpak-builder --user --install-deps-from=flathub --repo=flatpak-repo flatpak-build packaging/flatpak/io.github.morbok.RecoverySuite.json
flatpak build-bundle flatpak-repo RecoverySuite-x86_64.flatpak io.github.morbok.RecoverySuite prototype --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo
```

Only tracked source enters the build archive. Local assistant configuration,
credentials, disk images and other ignored files are never bundled.
