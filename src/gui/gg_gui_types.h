#ifndef GG_GUI_TYPES_H
#define GG_GUI_TYPES_H

#include "../core/course_repository.h"
#include "../core/gg_types.h"
#include "../core/scale_repository.h"
#include "gg_gtk.h"

typedef struct {
    GtkApplication *app;
    GtkWindow *main_window;
    GGScaleRepository *scale_repo;
    GGCourseRepository *course_repo;
    const char *db_filepath;
    const char *backup_dir;
} GGAppContext;

#endif
