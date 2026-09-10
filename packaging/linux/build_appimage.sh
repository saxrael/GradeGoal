set -e

APP_DIR="GradeGoal.AppDir"
mkdir -p "$APP_DIR/usr/bin"
mkdir -p "$APP_DIR/usr/share/applications"
mkdir -p "$APP_DIR/usr/share/icons/hicolor/256x256/apps"

cp build/bin/GradeGoal "$APP_DIR/usr/bin/"
cp platform/linux/gradegoal.desktop "$APP_DIR/usr/share/applications/"
cp assets/icon/hicolor/256x256/apps/gradegoal.png "$APP_DIR/usr/share/icons/hicolor/256x256/apps/gradegoal.png"
cp assets/icon/hicolor/256x256/apps/gradegoal.png "$APP_DIR/gradegoal.png"
cp assets/icon/hicolor/256x256/apps/gradegoal.png "$APP_DIR/.DirIcon"
cp platform/linux/gradegoal.desktop "$APP_DIR/gradegoal.desktop"

if command -v linuxdeploy >/dev/null 2>&1; then
    linuxdeploy --appdir "$APP_DIR" \
        --plugin gtk \
        --output appimage \
        --desktop-file platform/linux/gradegoal.desktop \
        --icon-file assets/icon/hicolor/256x256/apps/gradegoal.png
fi
