#!/usr/bin/env bash
# =============================================================================
# build_deb.sh — package the release-profile libmpscring artifacts into a .deb
#
# author  Roman Horshkov <github.com/RomanHorshkov>
# date    2026
# (c) 2026
# =============================================================================
set -euo pipefail

ROOT_DIR="${ROOT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
PKG_NAME="mpscring"
STRIP="${STRIP:-strip}"

die() { printf '%s: %s\n' "${BASH_SOURCE[0]}" "$1" >&2; exit 1; }

cd "$ROOT_DIR"

# Read + validate version (packaged versions must be strict semver).
VER="$(tr -d '[:space:]' < VERSION)"
[[ "$VER" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "VERSION '${VER}' does not match ^[0-9]+\\.[0-9]+\\.[0-9]+\$"

# Build the release library artifacts (also refreshes the flat build/ symlinks
# and runs the hardening gate on the freshly linked .so).
./utils/build_libs.sh release

ARCH="$(dpkg --print-architecture)"
MULTIARCH="$(gcc -print-multiarch)"
[[ -n "${MULTIARCH}" ]] || die "compiler did not report a Debian multiarch tuple"

# Split version safely (keep IFS local)
IFS='.' read -r MAJOR MINOR PATCH <<< "$VER"

# Prepare package staging dir (kept under build/ so it doesn't pollute the repo root).
STAGE="${ROOT_DIR}/build/pkgroot"
rm -rf "$STAGE"
LIB_DIR="$STAGE/usr/lib/$MULTIARCH"
INCLUDE_DIR="$STAGE/usr/include"
mkdir -p "$STAGE/DEBIAN" "$LIB_DIR" "$INCLUDE_DIR"
# Explicit 0755: mkdir -p otherwise inherits the calling shell's umask, which on a permissive
# umask (e.g. 002) yields group-writable (0775) directories in the shipped package — Debian
# packages should never depend on umask for the mode of the paths they own.
chmod 0755 "$STAGE" "$STAGE/DEBIAN" "$STAGE/usr" "$STAGE/usr/lib" "$LIB_DIR" "$INCLUDE_DIR"

# Debian-managed payload belongs under /usr, not the administrator-owned /usr/local tree.
install -m 0644 app/mpscring.h "$INCLUDE_DIR/mpscring.h"

install -m 0755 "build/release/libmpscring.so.$VER" "$LIB_DIR/libmpscring.so.$VER"
"$STRIP" --strip-unneeded "$LIB_DIR/libmpscring.so.$VER"
ln -sf "libmpscring.so.$VER" "$LIB_DIR/libmpscring.so.$MAJOR"
ln -sf "libmpscring.so.$VER" "$LIB_DIR/libmpscring.so"

install -m 0644 build/release/libmpscring.a "$LIB_DIR/libmpscring.a"

# Gate the staged, stripped shared library: the exact deb payload must carry
# the hardening the release profile promises. A hard failure aborts the build.
"${ROOT_DIR}/utils/check_hardening.sh" "$LIB_DIR/libmpscring.so.$VER"

# Control file
cat > "$STAGE/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $VER
Section: libs
Priority: optional
Architecture: $ARCH
Maintainer: Roman Horshkov <https://github.com/RomanHorshkov>
Description: Bounded multi-producer single-consumer ring buffer library
EOF

# post installation script
# ldconfig hooks so runtime linker sees it immediately
cat > "$STAGE/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
ldconfig
exit 0
EOF
chmod 0755 "$STAGE/DEBIAN/postinst"

cat > "$STAGE/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
ldconfig
exit 0
EOF
chmod 0755 "$STAGE/DEBIAN/postrm"

# Build .deb
DEB="${PKG_NAME}_${VER}_${ARCH}.deb"
fakeroot dpkg-deb --build "$STAGE" "$DEB"

printf '\nBuilt complete\n'

OUT_DIR="${OUT_DIR:-${ROOT_DIR}/build/debs}"
mkdir -p "$OUT_DIR"
# Keep incremental builds single-valued so CI and humans cannot select an older package.
rm -f "${OUT_DIR}/${PKG_NAME}_"*.deb "${OUT_DIR}/SHA256SUMS"
mv -f "$DEB" "$OUT_DIR/"

# The manifest describes exactly the artifact produced by this invocation.
(
    cd "$OUT_DIR"
    sha256sum -- "$DEB" > SHA256SUMS
)
printf 'checksums: %s/SHA256SUMS\n' "$OUT_DIR"

printf 'see .deb info with dpkg-deb -c %s or dpkg-deb -I %s\n' "$DEB" "$DEB"
printf 'moved to %s/\n' "$OUT_DIR"
printf 'install with sudo apt install %s/%s\n' "$OUT_DIR" "$DEB"
