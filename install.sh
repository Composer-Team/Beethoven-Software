#!/usr/bin/env bash
# install.sh — install Beethoven-Software (libbeethoven + the runtime
# cmake source-package) to ${CMAKE_INSTALL_PREFIX} (default ~/.local).
# Idempotent: re-run after `git pull` to refresh.
#
# Usage:
#   ./install.sh                       # install to ~/.local in Release mode
#   ./install.sh --prefix /opt/foo     # install to a different prefix
#   ./install.sh --debug               # build with -O0 -g
#   ./install.sh --platforms "zynq"    # restrict host platforms (default both)
#   ./install.sh --clean               # wipe the build dir before reconfiguring
#   ./install.sh --help                # this message
#
# Note on baremetal: this script installs the host build (discrete + zynq).
# The Cortex-M55 baremetal variant needs a separate toolchain file and the
# M55_SRC env var; see README's "Baremetal (Cortex-M55) variant" section.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${CMAKE_INSTALL_PREFIX:-$HOME/.local}"
BUILD_TYPE="Release"
PLATFORMS=""           # empty → use cmake default ("discrete;zynq")
CLEAN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)
            PREFIX="$2"
            shift 2
            ;;
        --prefix=*)
            PREFIX="${1#*=}"
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --platforms)
            PLATFORMS="$2"
            shift 2
            ;;
        --platforms=*)
            PLATFORMS="${1#*=}"
            shift
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        --help|-h)
            sed -n '2,17p' "$0" | sed 's|^# \{0,1\}||'
            exit 0
            ;;
        *)
            echo "install.sh: unknown argument '$1' (try --help)" >&2
            exit 2
            ;;
    esac
done

BUILD_DIR="${HERE}/build"

if [[ "${CLEAN}" -eq 1 ]]; then
    echo "==> Wiping ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

CMAKE_ARGS=(
    -S "${HERE}"
    -B "${BUILD_DIR}"
    "-DCMAKE_INSTALL_PREFIX=${PREFIX}"
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
)
if [[ -n "${PLATFORMS}" ]]; then
    CMAKE_ARGS+=("-DBEETHOVEN_PLATFORMS=${PLATFORMS}")
fi

echo "==> Configuring (prefix=${PREFIX}, build-type=${BUILD_TYPE}${PLATFORMS:+, platforms=${PLATFORMS}})"
cmake "${CMAKE_ARGS[@]}"

echo "==> Building"
cmake --build "${BUILD_DIR}" -j

echo "==> Installing"
cmake --install "${BUILD_DIR}"

# Resolve lib vs lib64 for the summary (GNUInstallDirs picks per distro).
LIBDIR="lib"
[[ -d "${PREFIX}/lib64" ]] && LIBDIR="lib64"

cat <<EOF

==> Done.

Installed to ${PREFIX}/:
  ${PREFIX}/${LIBDIR}/libbeethoven-*.so          (built artifacts)
  ${PREFIX}/${LIBDIR}/cmake/beethoven/           (cmake configs)
  ${PREFIX}/include/beethoven/                  (public headers)
  ${PREFIX}/share/beethoven/runtime-src/        (runtime cmake source-pkg)

Registry breadcrumb at ~/.cmake/packages/beethoven/<md5> means downstream
projects can find_package(beethoven) with no env vars.

To use: in your project's sw/CMakeLists.txt:
  find_package(beethoven REQUIRED)
  beethoven_build(my_tb SOURCES my_tb.cc)

To uninstall: ./uninstall.sh (use the same --prefix you passed here).
EOF
