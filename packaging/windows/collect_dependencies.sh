#!/usr/bin/env bash
set -e

BUILD_DIR="${1:-build/bin}"
DIST_DIR="${2:-dist}"

mkdir -p "$DIST_DIR/bin"
mkdir -p "$DIST_DIR/share/glib-2.0/schemas"
mkdir -p "$DIST_DIR/share/icons"

cp "$BUILD_DIR/GradeGoal.exe" "$DIST_DIR/bin/"

if command -v ntldd >/dev/null 2>&1; then
    for dll in $(ntldd -R "$BUILD_DIR/GradeGoal.exe" 2>/dev/null | grep -i '/mingw64/bin/' | awk '{print $3}' | sort -u); do
        if [ -f "$dll" ]; then
            cp "$dll" "$DIST_DIR/bin/"
        fi
    done
fi

for dll in \
    libgtk-4-1.dll libgio-2.0-0.dll libglib-2.0-0.dll libgobject-2.0-0.dll \
    libgmodule-2.0-0.dll libpango-1.0-0.dll libpangocairo-1.0-0.dll libpangoft2-1.0-0.dll \
    libcairo-2.dll libcairo-gobject-2.dll libgdk_pixbuf-2.0-0.dll libgraphene-1.0-0.dll \
    libepoxy-0.dll libharfbuzz-0.dll libfontconfig-1.dll libfreetype-6.dll \
    libfribidi-0.dll libintl-8.dll libffi-8.dll libsqlite3-0.dll \
    zlib1.dll libexpat-1.dll libpng16-16.dll libwinpthread-1.dll \
    libgcc_s_seh-1.dll libstdc++-6.dll; do
    if [ -f "/mingw64/bin/$dll" ]; then
        cp "/mingw64/bin/$dll" "$DIST_DIR/bin/"
    fi
done

if [ -d "/mingw64/share/glib-2.0/schemas" ]; then
    cp -r /mingw64/share/glib-2.0/schemas/* "$DIST_DIR/share/glib-2.0/schemas/"
    if command -v glib-compile-schemas >/dev/null 2>&1; then
        glib-compile-schemas "$DIST_DIR/share/glib-2.0/schemas"
    fi
fi

if [ -d "/mingw64/share/icons/hicolor" ]; then
    mkdir -p "$DIST_DIR/share/icons/hicolor"
    cp -r /mingw64/share/icons/hicolor/* "$DIST_DIR/share/icons/hicolor/"
fi

if [ -d "/mingw64/lib/gdk-pixbuf-2.0" ]; then
    mkdir -p "$DIST_DIR/lib/gdk-pixbuf-2.0"
    cp -r /mingw64/lib/gdk-pixbuf-2.0/* "$DIST_DIR/lib/gdk-pixbuf-2.0/"
fi

if [ -d "/mingw64/lib/gio/modules" ]; then
    mkdir -p "$DIST_DIR/lib/gio/modules"
    cp -r /mingw64/lib/gio/modules/* "$DIST_DIR/lib/gio/modules/"
fi