set -e

mkdir -p dist

APP_DIR="dist/GradeGoal.app"
mkdir -p "$APP_DIR/Contents/MacOS"
mkdir -p "$APP_DIR/Contents/Resources"

cp build/bin/GradeGoal "$APP_DIR/Contents/MacOS/GradeGoal"
chmod +x "$APP_DIR/Contents/MacOS/GradeGoal"
cp packaging/macos/Info.plist "$APP_DIR/Contents/Info.plist"
cp assets/icon/icon.icns "$APP_DIR/Contents/Resources/icon.icns"

if command -v gtk-mac-bundler >/dev/null 2>&1; then
    gtk-mac-bundler packaging/macos/GradeGoal.bundle 2>/dev/null || true
fi

rm -f "dist/GradeGoal_macOS.dmg"

DMG_CREATED=0
if command -v create-dmg >/dev/null 2>&1; then
    create-dmg \
        --volname "GradeGoal" \
        --window-pos 200 120 \
        --window-size 600 400 \
        --icon-size 100 \
        --icon "GradeGoal.app" 175 190 \
        --app-drop-link 425 190 \
        --overwrite \
        "dist/GradeGoal_macOS.dmg" \
        "$APP_DIR" && DMG_CREATED=1 || true
fi

if [ "$DMG_CREATED" -eq 0 ] || [ ! -f "dist/GradeGoal_macOS.dmg" ]; then
    hdiutil create -volname "GradeGoal" -srcfolder "$APP_DIR" -ov -format UDZO "dist/GradeGoal_macOS.dmg"
fi
