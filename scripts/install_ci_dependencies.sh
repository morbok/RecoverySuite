#!/usr/bin/env bash
# Disposable GitHub Ubuntu runner only. Do not modify workstation apt sources.
set -euo pipefail
[[ "${CI:-}" == true && "${GITHUB_ACTIONS:-}" == true ]] || { echo 'CI-only dependency installation' >&2; exit 1; }
source_file=$(mktemp --suffix=.sources)
trap 'rm -f -- "$source_file"' EXIT
cat > "$source_file" <<'SOURCES'
Types: deb
URIs: https://archive.ubuntu.com/ubuntu/
Suites: noble noble-updates
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: https://security.ubuntu.com/ubuntu/
Suites: noble-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
SOURCES
options=(-o "Dir::Etc::sourcelist=$source_file" -o Dir::Etc::sourceparts=-
    -o Acquire::ForceIPv4=true -o Acquire::Retries=2
    -o Acquire::http::Timeout=20 -o Acquire::https::Timeout=20)
packages=(qt6-base-dev libudev-dev)
for specification in 'cmake:cmake' 'ninja:ninja-build' 'g++:g++'; do
    command -v "${specification%%:*}" >/dev/null || packages+=("${specification#*:}")
done
if [[ "${1:-}" == fuzz ]]; then
    command -v clang++ >/dev/null || packages+=(clang)
else
    packages+=(exfatprogs udisks2 dmsetup)
fi
sudo apt-get "${options[@]}" update
sudo apt-get "${options[@]}" install --no-install-recommends -y "${packages[@]}"
