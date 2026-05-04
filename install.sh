#!/usr/bin/env bash
# install.sh — install libbeethoven (discrete + zynq) and the runtime
# cmake source-package to $PREFIX (default ~/.local). Idempotent.
#
# Usage:
#   ./install.sh                    # ~/.local, Release
#   ./install.sh --prefix /opt/foo
#   ./install.sh --debug            # -O0 -g
#   ./install.sh --clean            # wipe build/ first
#   ./install.sh --help

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${CMAKE_INSTALL_PREFIX:-$HOME/.local}"
BUILD_TYPE="Release"
CLEAN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)    PREFIX="$2"; shift 2;;
        --prefix=*)  PREFIX="${1#*=}"; shift;;
        --debug)     BUILD_TYPE="Debug"; shift;;
        --clean)     CLEAN=1; shift;;
        --help|-h)   sed -n '2,10p' "$0" | sed 's|^# \{0,1\}||'; exit 0;;
        *)           echo "install.sh: unknown argument '$1'" >&2; exit 2;;
    esac
done

BUILD_DIR="${HERE}/build"
[[ "${CLEAN}" -eq 1 ]] && rm -rf "${BUILD_DIR}"

cmake -S "${HERE}" -B "${BUILD_DIR}" \
      "-DCMAKE_INSTALL_PREFIX=${PREFIX}" \
      "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" -j
cmake --install "${BUILD_DIR}"

echo
echo "Installed to ${PREFIX}/. Use:"
echo "  find_package(beethoven REQUIRED)"
echo "  beethoven_build(my_tb SOURCES my_tb.cc)"
echo "Uninstall: ./uninstall.sh"
