#!/usr/bin/env bash
set -e
shopt -s nullglob

BUILD_DIR="${1:-build/bin}"
DIST_DIR="${2:-dist}"

mkdir -p "$DIST_DIR/bin"
mkdir -p "$DIST_DIR/share/glib-2.0/schemas"
mkdir -p "$DIST_DIR/share/icons"

cp "$BUILD_DIR/GradeGoal.exe" "$DIST_DIR/bin/"

if [ -d "/mingw64/share/glib-2.0/schemas" ]; then
    mkdir -p "$DIST_DIR/share/glib-2.0/schemas"
    cp -r /mingw64/share/glib-2.0/schemas/* "$DIST_DIR/share/glib-2.0/schemas/" 2>/dev/null || true
    if command -v glib-compile-schemas >/dev/null 2>&1; then
        glib-compile-schemas "$DIST_DIR/share/glib-2.0/schemas" 2>/dev/null || true
    fi
fi

if [ -d "/mingw64/share/icons/hicolor" ]; then
    mkdir -p "$DIST_DIR/share/icons/hicolor"
    cp -r /mingw64/share/icons/hicolor/* "$DIST_DIR/share/icons/hicolor/" 2>/dev/null || true
fi

if [ -d "assets/icon/hicolor" ]; then
    mkdir -p "$DIST_DIR/share/icons/hicolor"
    cp -r assets/icon/hicolor/* "$DIST_DIR/share/icons/hicolor/" 2>/dev/null || true
fi

if [ -d "/mingw64/etc/fonts" ]; then
    mkdir -p "$DIST_DIR/etc/fonts"
    cp -r /mingw64/etc/fonts/* "$DIST_DIR/etc/fonts/" 2>/dev/null || true
fi

if [ -d "/mingw64/lib/gdk-pixbuf-2.0" ]; then
    mkdir -p "$DIST_DIR/lib/gdk-pixbuf-2.0"
    cp -r /mingw64/lib/gdk-pixbuf-2.0/* "$DIST_DIR/lib/gdk-pixbuf-2.0/" 2>/dev/null || true
fi

if [ -d "/mingw64/lib/gio/modules" ]; then
    mkdir -p "$DIST_DIR/lib/gio/modules"
    cp -r /mingw64/lib/gio/modules/* "$DIST_DIR/lib/gio/modules/" 2>/dev/null || true
fi

for dll in \
    libgtk-4-1.dll libgio-2.0-0.dll libglib-2.0-0.dll libgobject-2.0-0.dll \
    libgmodule-2.0-0.dll libpango-1.0-0.dll libpangocairo-1.0-0.dll libpangoft2-1.0-0.dll \
    libpangowin32-1.0-0.dll libcairo-2.dll libcairo-gobject-2.dll libgdk_pixbuf-2.0-0.dll \
    libgraphene-1.0-0.dll libepoxy-0.dll libharfbuzz-0.dll libfontconfig-1.dll \
    libfreetype-6.dll libfribidi-0.dll libintl-8.dll libffi-8.dll libsqlite3-0.dll \
    zlib1.dll libexpat-1.dll libpng16-16.dll libwinpthread-1.dll \
    libgcc_s_seh-1.dll libstdc++-6.dll libpcre2-8-0.dll libiconv-2.dll \
    libbrotlidec.dll libbrotlicommon.dll libbz2-1.dll libdeflate.dll \
    liblzma-5.dll libzstd.dll libpixman-1-0.dll; do
    if [ -f "/mingw64/bin/$dll" ]; then
        cp "/mingw64/bin/$dll" "$DIST_DIR/bin/"
    fi
done

if command -v objdump >/dev/null 2>&1; then
    CHANGES=1
    ITER=0
    while [ "$CHANGES" -eq 1 ] && [ "$ITER" -lt 15 ]; do
        CHANGES=0
        ITER=$((ITER + 1))
        for target in "$DIST_DIR/bin/"*.exe "$DIST_DIR/bin/"*.dll "$DIST_DIR/lib/gdk-pixbuf-2.0/2.10.0/loaders/"*.dll; do
            [ -f "$target" ] || continue
            for dep in $(objdump -p "$target" 2>/dev/null | grep -i "DLL Name:" | awk '{print $3}'); do
                if [ -f "/mingw64/bin/$dep" ] && [ ! -f "$DIST_DIR/bin/$dep" ]; then
                    cp "/mingw64/bin/$dep" "$DIST_DIR/bin/"
                    CHANGES=1
                fi
            done
        done
    done
fi