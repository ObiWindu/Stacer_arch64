#!/usr/bin/env bash
#
# Stacer installer for Fedora aarch64 (arm64).
#
# Stacer is not packaged for Fedora, so this script builds it from this source
# tree and installs it with the project's own CMake install rules. The build
# itself is architecture independent; the aarch64 specifics (dnf package names,
# no x86-only dependencies) are handled here.
#
# Usage: ./install.sh [options]
#
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---------------------------------------------------------------- options ---

PREFIX="/usr/local"
BUILD_TYPE="Release"
BUILD_DIR="build"
JOBS="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN || echo 2)"
DO_DEPS=0
DO_CLEAN=0

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

  --prefix <dir>    Install into <dir> (default: /usr/local)
  --user            Install into ~/.local (no sudo needed)
  --system          Install machine wide into /usr/local (default)
  --debug           Build with debug symbols (build only, not installable)
  --release         Build optimized and install it (default)
  --jobs <n>        Parallel build jobs (default: $JOBS)
  --build-dir <d>   Build directory (default: $BUILD_DIR)
  --deps            Install the Fedora build dependencies with dnf first
  --clean           Remove the build directory before building
  -h, --help        Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)      PREFIX="$2"; shift 2 ;;
        --prefix=*)    PREFIX="${1#*=}"; shift ;;
        --user)        PREFIX="$HOME/.local"; shift ;;
        --system)      PREFIX="/usr/local"; shift ;;
        --debug)       BUILD_TYPE="Debug"; shift ;;
        --release)     BUILD_TYPE="Release"; shift ;;
        --jobs)        JOBS="$2"; shift 2 ;;
        --jobs=*)      JOBS="${1#*=}"; shift ;;
        --build-dir)   BUILD_DIR="$2"; shift 2 ;;
        --build-dir=*) BUILD_DIR="${1#*=}"; shift ;;
        --deps)        DO_DEPS=1; shift ;;
        --clean)       DO_CLEAN=1; shift ;;
        -h|--help)     usage; exit 0 ;;
        *) echo "error: unknown option '$1'" >&2; usage >&2; exit 1 ;;
    esac
done

# ------------------------------------------------------------ environment ---

ARCH="$(uname -m)"
DISTRO="$(sed -n 's/^ID=//p' /etc/os-release 2>/dev/null | tr -d '"')"

if [[ "$ARCH" != "aarch64" && "$ARCH" != "arm64" ]]; then
    echo "warning: this installer targets Fedora aarch64, but this machine is '$ARCH'." >&2
    echo "         it may still work, assuming x86-only packages are not needed." >&2
fi

if [[ "$DISTRO" != "fedora" && -n "$DISTRO" ]]; then
    echo "warning: this installer targets Fedora, but detected '$DISTRO'." >&2
fi

# sudo is only needed when the prefix is outside the home directory
SUDO=""
if [[ ! -w "$(dirname "$PREFIX")" ]]; then
    if command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    elif [[ "$(id -u)" != "0" ]]; then
        echo "error: $PREFIX is not writable and sudo is not available." >&2
        echo "       rerun with --user to install into $HOME/.local." >&2
        exit 1
    fi
fi

# ------------------------------------------------------- build dependencies ---

FEDORA_BUILD_DEPS=(
    cmake
    gcc-c++
    make
    pkgconf-pkg-config
    qt5-qtbase-devel
    qt5-qtcharts-devel
    qt5-qtsvg-devel
    qt5-qttools-devel
    qt5-qttranslations-devel
    lksmice            # htop, used by the Processes page
    gawk               # system cleaner rules
    polkit             # the autostart page asks it to manage startup apps
)

if [[ "$DO_DEPS" == "1" ]]; then
    echo "==> installing Fedora build dependencies"
    $SUDO dnf install -y "${FEDORA_BUILD_DEPS[@]}"
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "error: cmake not found. rerun with --deps, or install it manually:" >&2
    echo "       $SUDO dnf install cmake gcc-c++" >&2
    exit 1
fi

# ------------------------------------------------------------------- build ---

if [[ "$DO_CLEAN" == "1" ]]; then
    echo "==> removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

# CMake 4 dropped compatibility with cmake_minimum_required() below 3.5, which
# is what this project still declares, so opt back into the old policy set.
CMAKE_EXTRA_ARGS=()
CMAKE_MAJOR="$(cmake --version | head -1 | sed -n 's/^cmake version \([0-9][0-9]*\).*/\1/p')"
if [[ "${CMAKE_MAJOR:-0}" -ge 4 ]]; then
    CMAKE_EXTRA_ARGS+=(-DCMAKE_POLICY_VERSION_MINIMUM=3.5)
fi

echo "==> building stacer ($BUILD_TYPE, $JOBS jobs, $ARCH)"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    ${CMAKE_EXTRA_ARGS[@]+"${CMAKE_EXTRA_ARGS[@]}"}

cmake --build "$BUILD_DIR" -j "$JOBS"

# ------------------------------------------------------------------ install ---

echo "==> installing into $PREFIX"
if [[ "$BUILD_TYPE" == "Debug" ]]; then
    # The install() rules are limited to optimized configurations, so a debug
    # build has nothing to install.
    echo "warning: debug builds have no install rules; skipping installation." >&2
    echo "         the binary is at $BUILD_DIR/output/stacer" >&2
    exit 0
fi
$SUDO cmake --install "$BUILD_DIR"

if command -v update-desktop-database >/dev/null 2>&1 && [[ -d "$PREFIX/share/applications" ]]; then
    $SUDO update-desktop-database "$PREFIX/share/applications" 2>/dev/null || true
fi

echo
echo "stacer installed to $PREFIX/bin/stacer"
case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *) echo "note: $PREFIX/bin is not in your PATH, start it with the full path" ;;
esac
echo "run 'stacer' to start it."
