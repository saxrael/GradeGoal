set -e

mkdir -p dist

if command -v gtk-mac-bundler >/dev/null 2>&1; then
    gtk-mac-bundler packaging/macos/GradeGoal.bundle
fi

if command -v create-dmg >/dev/null 2>&1; then
    create-dmg \
        --volname "GradeGoal" \
        --window-pos 200 120 \
        --window-size 600 400 \
        --icon-size 100 \
        --icon "GradeGoal.app" 175 190 \
        --app-drop-link 425 190 \
        "dist/GradeGoal_macOS.dmg" \
        "dist/GradeGoal.app"
fi
