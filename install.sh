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
    qt6-qtbase-devel
    qt6-qtcharts-devel
    qt6-qtsvg-devel
    qt6-qttools-devel
    gawk               # system cleaner rules
    polkit             # the autostart page asks it to manage startup apps
)

if [[ "$DO_DEPS" == "1" ]]; then
    echo "==> installing Fedora build dependencies"
    # --skip-unavailable so a single stale entry cannot abort the whole
    # transaction; anything genuinely required to build is caught by the
    # explicit checks further down, which give actionable messages.
    $SUDO dnf install -y --skip-unavailable "${FEDORA_BUILD_DEPS[@]}"
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "error: cmake not found. rerun with --deps, or install it manually:" >&2
    echo "       $SUDO dnf install cmake gcc-c++" >&2
    exit 1
fi

# -------------------------------------------------------------- Qt6 lookup ---

# The oldest Qt 6 this project builds against; keep in sync with QT6_MIN_VERSION
# in CMakeLists.txt.
QT6_MIN_VERSION="6.5"

# Qt6 sets PACKAGE_VERSION in Qt6ConfigVersionImpl.cmake, which
# Qt6ConfigVersion.cmake only includes; check both so this keeps working if a
# distribution generates a self-contained version file instead.
qt6_version() {
    local version_file version
    for version_file in "$1/Qt6ConfigVersionImpl.cmake" "$1/Qt6ConfigVersion.cmake"; do
        [[ -f "$version_file" ]] || continue
        version="$(sed -n 's/^[[:space:]]*set(PACKAGE_VERSION[[:space:]]*"\([^"]*\)".*/\1/p' "$version_file" | head -1)"
        if [[ -n "$version" ]]; then
            printf '%s\n' "$version"
            return 0
        fi
    done
    return 1
}

# find_package(Qt6) only searches its own install prefix, so it fails on setups
# where Qt6 lives elsewhere. Locate the config files ourselves so the build
# fails with an actionable message instead of a bare CMake error.
qt6_devel_package() {
    case "$1" in
        Core|Gui|Widgets|Concurrent|Network) echo "qt6-qtbase-devel" ;;
        Charts)                           echo "qt6-qtcharts-devel" ;;
        Svg)                              echo "qt6-qtsvg-devel" ;;
        LinguistTools)                    echo "qt6-qttools-devel" ;;
        *)                                echo "qt6-qtbase-devel" ;;
    esac
}

# Qt6Config.cmake only holds the umbrella package config; the per-module configs
# live in sibling directories next to it, e.g. /usr/lib64/cmake/Qt6Charts.
# Check both layouts so distributions that nest them still work.
qt6_component_dir() {
    local qt6_dir="$1" component="$2" dir
    for dir in "$(dirname "$qt6_dir")/Qt6${component}" "$qt6_dir/Qt6${component}"; do
        if [[ -f "$dir/Qt6${component}Config.cmake" ]]; then
            printf '%s\n' "$dir"
            return 0
        fi
    done
    return 1
}

QT6_HINTS=()
for prefix in "${QT6_PREFIX:-}" /usr /usr/local /opt; do
    [[ -n "$prefix" ]] || continue
    QT6_HINTS+=("$prefix/lib64/cmake/Qt6" "$prefix/lib/cmake/Qt6")
    for libdir in "$prefix"/lib/*/cmake/Qt6; do
        [[ -d "$libdir" ]] && QT6_HINTS+=("$libdir")
    done
done
for qmake_bin in qmake6 qmake; do
    if command -v "$qmake_bin" >/dev/null 2>&1; then
        qmake_prefix="$("$qmake_bin" -query QT_INSTALL_PREFIX 2>/dev/null)"
        [[ -n "$qmake_prefix" ]] && QT6_HINTS+=("$qmake_prefix/lib/cmake/Qt6" "$qmake_prefix/lib64/cmake/Qt6")
    fi
done
for libdir in "$HOME"/Qt/6.*/lib/cmake/Qt6 "$HOME"/Qt/6.*/lib64/cmake/Qt6; do
    [[ -d "$libdir" ]] && QT6_HINTS+=("$libdir")
done

QT6_DIR=""
for hint in "${QT6_HINTS[@]+"${QT6_HINTS[@]}"}"; do
    if [[ -f "$hint/Qt6Config.cmake" ]]; then
        QT6_DIR="$hint"
        break
    fi
done

if [[ -z "$QT6_DIR" ]]; then
    echo "error: no Qt6 development files found; this project requires Qt 6." >&2
    echo "       searched:" >&2
    printf '         %s\n' "${QT6_HINTS[@]+"${QT6_HINTS[@]}"}" >&2
    echo "       install them with:" >&2
    echo "         $SUDO dnf install qt6-qtbase-devel qt6-qtcharts-devel qt6-qtsvg-devel qt6-qttools-devel" >&2
    echo "       or rerun this script with --deps. If a /usr/lib64/cmake/Qt6" >&2
    echo "       directory exists but the package is gone, reinstall the qt6" >&2
    echo "       -devel packages to clear the stale entries." >&2
    exit 1
fi

# The requested components have to be present too, and each one maps to a
# specific Fedora package, so report the exact ones that are missing.
missing_deps=()
for component in Core Gui Widgets Charts Svg Concurrent LinguistTools; do
    if ! qt6_component_dir "$QT6_DIR" "$component" >/dev/null; then
        missing_deps+=("$(qt6_devel_package "$component")")
    fi
done
# several components come from the same -devel package, so report each once
if [[ ${#missing_deps[@]} -gt 0 ]]; then
    mapfile -t missing_deps < <(printf '%s\n' "${missing_deps[@]}" | sort -u)
    echo "error: incomplete Qt6 installation in $QT6_DIR; missing components:" >&2
    for pkg in "${missing_deps[@]}"; do
        echo "         $pkg" >&2
    done
    echo "       install them with: $SUDO dnf install ${missing_deps[*]}" >&2
    exit 1
fi

# Best-effort version gate. If the version cannot be determined, leave the
# decision to CMake's own find_package() version check rather than failing here.
QT6_HAVE_VERSION="$(qt6_version "$QT6_DIR" || true)"
if [[ -n "$QT6_HAVE_VERSION" ]]; then
    if [[ "$(printf '%s\n%s\n' "$QT6_MIN_VERSION" "$QT6_HAVE_VERSION" | sort -V | head -1)" != "$QT6_MIN_VERSION" ]]; then
        echo "error: found Qt $QT6_HAVE_VERSION in $QT6_DIR, but this project needs Qt $QT6_MIN_VERSION or newer." >&2
        echo "       newer Qt6 builds from https://download.qt.io are not in the Fedora repos;" >&2
        echo "       point QT6_PREFIX at one, or raise QT6_MIN_VERSION if the port really" >&2
        echo "       needs a lower floor." >&2
        exit 1
    fi
fi

echo "==> using Qt ${QT6_HAVE_VERSION:-<version unknown>} from $QT6_DIR"

# Qt6Charts and Qt6Svg are packaged separately from qt6-qtbase-devel, and Qt6
# only looks for them via its own search paths, so pass each config directory
# explicitly instead of relying on CMAKE_PREFIX_PATH.
QT6_COMPONENT_ARGS=()
for component in Charts Svg; do
    if component_dir="$(qt6_component_dir "$QT6_DIR" "$component")"; then
        QT6_COMPONENT_ARGS+=("-DQt6${component}_DIR=$component_dir")
    fi
done

# ------------------------------------------------------------------- build ---

if [[ "$DO_CLEAN" == "1" ]]; then
    echo "==> removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

# The project requires CMake 3.16+ (Qt6), so no legacy policy opt-in is needed.
CMAKE_EXTRA_ARGS=()

echo "==> building stacer ($BUILD_TYPE, $JOBS jobs, $ARCH)"
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DQt6_DIR="$QT6_DIR" \
    "${QT6_COMPONENT_ARGS[@]+"${QT6_COMPONENT_ARGS[@]}"}" \
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
