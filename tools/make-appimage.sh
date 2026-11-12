#!/bin/sh
# make-appimage.sh -- TenForge as one AppImage, from a fresh Release build.
#
# Usage: tools/make-appimage.sh [output-dir]
#
# Uses linuxdeploy and its Qt plugin (vetted, widely used; tenet 4), pinned
# by release and SHA-256 below and fetched into the build tree on first use.
# They run as programs here only; nothing of theirs is linked into TenForge.
#
# The AppImage carries Qt, VTK and every other library TenForge needs, but
# not the C library or the OpenGL driver, which must come from the system:
# it runs on distributions at least as new as the one it was built on
# (Ubuntu 26.04 in CI, the first whose VTK is built against Qt 6).

set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${1:-"$here/dist"}
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd)   # absolute: the packaging runs from inside it
build="$here/build-appimage"
tools="$build/tools"

LD_TAG=1-alpha-20251107-1
LD_SHA=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
QT_TAG=1-alpha-20250213-1
QT_SHA=15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724

fetch() {  # repo tag asset sha
    dest="$tools/$3"
    if [ ! -f "$dest" ] || ! echo "$4  $dest" | sha256sum -c --quiet - 2>/dev/null; then
        mkdir -p "$tools"
        curl -fsSL -o "$dest.part" "https://github.com/$1/releases/download/$2/$3"
        echo "$4  $dest.part" | sha256sum -c --quiet - || {
            echo "make-appimage: $3 does not match its pinned SHA-256; refusing it" >&2
            rm -f "$dest.part"
            exit 1
        }
        mv "$dest.part" "$dest"
        chmod +x "$dest"
    fi
}
fetch linuxdeploy/linuxdeploy "$LD_TAG" linuxdeploy-x86_64.AppImage "$LD_SHA"
fetch linuxdeploy/linuxdeploy-plugin-qt "$QT_TAG" linuxdeploy-plugin-qt-x86_64.AppImage "$QT_SHA"

cmake -S "$here" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr -DTF_VALIDATION_DOWNLOADS=OFF >/dev/null
cmake --build "$build"
rm -rf "$build/AppDir"
DESTDIR="$build/AppDir" cmake --install "$build" >/dev/null

version=$(sed -n 's/^project(TenForge VERSION \([0-9.]*\).*/\1/p' "$here/CMakeLists.txt")
# Lessons from SurView's packaging (same tools, same pins):
#  - the Qt plugin brings the platform, image-format and style plugins the
#    program loads at run time; the SVG image format among them, or the
#    window icon silently draws nothing;
#  - an empty LDAI_UPDATE_INFORMATION reaches appimagetool as -u "" and the
#    very last step fails, so it is unset unless someone sets it.
export QMAKE="${QMAKE:-/usr/lib/qt6/bin/qmake6}"
export EXTRA_QT_PLUGINS="${EXTRA_QT_PLUGINS:-svg;imageformats}"
export LDAI_OUTPUT="TenForge-$version-x86_64.AppImage"
if [ -z "${LDAI_UPDATE_INFORMATION:-}" ]; then
    unset LDAI_UPDATE_INFORMATION
fi
export PATH="$tools:$PATH"            # linuxdeploy finds its plugin on PATH
export APPIMAGE_EXTRACT_AND_RUN=1     # no FUSE needed (CI runners have none)

mkdir -p "$out"
cd "$out"
"$tools/linuxdeploy-x86_64.AppImage" \
    --appdir "$build/AppDir" \
    --executable "$build/AppDir/usr/bin/tf-analyse" \
    --desktop-file "$build/AppDir/usr/share/applications/tenforge.desktop" \
    --icon-file "$build/AppDir/usr/share/icons/hicolor/scalable/apps/tenforge.svg" \
    --plugin qt \
    --output appimage
ls -l "$out/$LDAI_OUTPUT"
