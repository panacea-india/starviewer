#!/bin/bash
# Packs the built Starviewer into a Linux AppImage.
#
# Usage: make-appimage.sh <build directory>
#
# The build directory must have been configured with USE_QTCONF=ON and built
# first (the embedded qt.conf makes Qt look for plugins and translations next
# to the installed executable). Like the DEB package, the AppImage bundles
# Qt, ICU and the patched SDK shared libraries; it still needs the system
# libraries listed in CPACK_DEBIAN_PACKAGE_DEPENDS
# (starviewer/CMake/Installers.cmake) to be present on the host system, and
# it is only portable to systems as new as the one that built it (glibc and
# friends are not bundled).
set -e

BUILD_DIR=$1
APPDIR=$BUILD_DIR/AppDir
rm -rf "$APPDIR"

# The same installed tree as the DEB package, under opt/starviewer.
cmake --install "$BUILD_DIR" --prefix "$APPDIR/opt/starviewer"

# AppRun replaces starviewer.sh as the entry point: it resolves the AppImage
# mount point and adds the bundled libraries to the loader path.
cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/bash
HERE=$(dirname "$(readlink -f "$0")")
export LD_LIBRARY_PATH="$HERE/opt/starviewer/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$HERE/opt/starviewer/starviewer" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# Icon and desktop file at the locations the AppImage tools expect. Note
# that appimagetool wants the icon at the AppDir root, named after the
# desktop file's Icon key, in addition to the freedesktop icon layout.
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp "$APPDIR/opt/starviewer/logo/ico-256.png" \
   "$APPDIR/usr/share/icons/hicolor/256x256/apps/starviewer.png"
cp "$APPDIR/opt/starviewer/logo/ico-256.png" "$APPDIR/starviewer.png"
ln -s usr/share/icons/hicolor/256x256/apps/starviewer.png "$APPDIR/.DirIcon"
cat > "$APPDIR/starviewer.desktop" <<'EOF'
[Desktop Entry]
Name=Starviewer
Exec=AppRun
Icon=starviewer
Type=Application
Categories=Graphics;Science;Biology;MedicalSoftware;
Comment=Medical imaging DICOM image viewer
MimeType=x-scheme-handler/starviewer;
EOF

# appimagetool is itself an AppImage; --appimage-extract-and-run lets it run
# without FUSE.
APPIMAGETOOL_URL=https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage
wget -q --tries=3 -O appimagetool.AppImage "$APPIMAGETOOL_URL"
chmod +x appimagetool.AppImage

# Name the AppImage like the DEB package, without the operating system
# suffix ("starviewer~<version>~<arch>-<os>").
NAME=$(sed -n 's/^set(CPACK_PACKAGE_FILE_NAME "\(.*\)")$/\1/p' "$BUILD_DIR/CPackConfig.cmake" | sed 's/-[^-]*$//')
if [ -z "$NAME" ]
then
    echo "ERROR: Could not read CPACK_PACKAGE_FILE_NAME from $BUILD_DIR/CPackConfig.cmake"
    exit 1
fi

./appimagetool.AppImage --appimage-extract-and-run --no-appstream \
    "$(readlink -f "$APPDIR")" "$(readlink -f "$BUILD_DIR/$NAME.AppImage")"
