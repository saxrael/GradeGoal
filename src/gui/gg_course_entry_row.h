#ifndef GG_COURSE_ENTRY_ROW_H
#define GG_COURSE_ENTRY_ROW_H

#include "../core/gg_types.h"
#include "gg_gtk.h"

typedef struct {
    GtkWidget *container;
    GtkWidget *course_label_entry;
    GtkWidget *credit_unit_entry;
    GtkWidget *grade_combo;
    GtkWidget *remove_button;
    GtkWidget *unit_error_label;
} GGCourseEntryRow;

GGCourseEntryRow *gg_course_entry_row_create(const GGGradingScale *scale, const GGCourseEntry *initial_entry,
                                             bool show_grade_combo, GCallback on_changed, GCallback on_remove,
                                             gpointer user_data);
void gg_course_entry_row_get_entry(const GGCourseEntryRow *row, GGCourseEntry *out_entry);
bool gg_course_entry_row_validate(GGCourseEntryRow *row, const GGGradingScale *scale);
void gg_course_entry_row_destroy(GGCourseEntryRow *row);

#endif
