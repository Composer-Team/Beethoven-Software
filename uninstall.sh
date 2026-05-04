#!/usr/bin/env bash
# uninstall.sh — remove Beethoven-Software files installed by install.sh
# from ${CMAKE_INSTALL_PREFIX} (default ~/.local), plus the cmake user
# package registry breadcrumb. Lists what will be removed and asks for
# confirmation by default.
#
# Usage:
#   ./uninstall.sh                    # interactive: list + confirm
#   ./uninstall.sh --prefix /opt/foo  # different prefix
#   ./uninstall.sh --yes              # skip confirmation
#   ./uninstall.sh --dry-run          # show what would be removed; remove nothing
#   ./uninstall.sh --help

set -euo pipefail
shopt -s nullglob

PREFIX="${CMAKE_INSTALL_PREFIX:-$HOME/.local}"
ASSUME_YES=0
DRY_RUN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)    PREFIX="$2"; shift 2;;
        --prefix=*)  PREFIX="${1#*=}"; shift;;
        --yes|-y)    ASSUME_YES=1; shift;;
        --dry-run)   DRY_RUN=1; shift;;
        --help|-h)
            sed -n '2,12p' "$0" | sed 's|^# \{0,1\}||'
            exit 0;;
        *)
            echo "uninstall.sh: unknown argument '$1' (try --help)" >&2
            exit 2;;
    esac
done

# === Build the candidate-removal list ===

CANDIDATES=()

# libbeethoven and libbeethoven_baremetal artifacts under lib/ or lib64/.
for d in "$PREFIX/lib" "$PREFIX/lib64"; do
    [[ -d "$d" ]] || continue
    for f in "$d"/libbeethoven-*.so "$d"/libbeethoven-*.so.* \
             "$d"/libbeethoven_baremetal.a; do
        CANDIDATES+=( "$f" )
    done
done

# cmake configs (whole subdirs).
for d in "$PREFIX/lib/cmake" "$PREFIX/lib64/cmake"; do
    [[ -d "$d/beethoven" ]]            && CANDIDATES+=( "$d/beethoven" )
    [[ -d "$d/beethoven_baremetal" ]]  && CANDIDATES+=( "$d/beethoven_baremetal" )
done

# Public headers and share/ source-packages.
[[ -d "$PREFIX/include/beethoven" ]]              && CANDIDATES+=( "$PREFIX/include/beethoven" )
[[ -d "$PREFIX/share/beethoven" ]]                && CANDIDATES+=( "$PREFIX/share/beethoven" )
[[ -d "$PREFIX/share/beethoven_baremetal" ]]      && CANDIDATES+=( "$PREFIX/share/beethoven_baremetal" )

# CMake user-package-registry breadcrumbs whose target falls inside
# our prefix. There may be other entries pointing at other installs;
# leave those alone.
REGISTRY_TO_REMOVE=()
for f in "$HOME/.cmake/packages/beethoven"/* \
         "$HOME/.cmake/packages/beethoven_baremetal"/*; do
    [[ -f "$f" ]] || continue
    target=$(< "$f")
    case "$target" in
        "$PREFIX"/*) REGISTRY_TO_REMOVE+=( "$f" );;
    esac
done

# === Show what will happen ===

echo "Will remove from prefix=${PREFIX}:"
if (( ${#CANDIDATES[@]} == 0 && ${#REGISTRY_TO_REMOVE[@]} == 0 )); then
    echo "  (nothing — install not present at this prefix)"
    exit 0
fi
for p in "${CANDIDATES[@]}";         do echo "  rm -rf  $p"; done
for p in "${REGISTRY_TO_REMOVE[@]}"; do echo "  rm      $p"; done

if (( DRY_RUN )); then
    echo
    echo "(dry-run — nothing was removed)"
    exit 0
fi

if (( ! ASSUME_YES )); then
    echo
    read -r -p "Proceed? [y/N] " ans
    case "$ans" in
        y|Y|yes|YES) ;;
        *) echo "Aborted."; exit 1;;
    esac
fi

# === Actually remove ===

for p in "${CANDIDATES[@]}";         do rm -rf "$p"; done
for p in "${REGISTRY_TO_REMOVE[@]}"; do rm -f  "$p"; done

# Tidy up empty parent dirs (best-effort; ignore failures).
rmdir "$PREFIX/lib/cmake"                 2>/dev/null || true
rmdir "$PREFIX/lib64/cmake"               2>/dev/null || true
rmdir "$HOME/.cmake/packages/beethoven"            2>/dev/null || true
rmdir "$HOME/.cmake/packages/beethoven_baremetal"  2>/dev/null || true

echo
echo "Done."
