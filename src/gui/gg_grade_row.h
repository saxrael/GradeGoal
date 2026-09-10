#ifndef GG_GRADE_ROW_H
#define GG_GRADE_ROW_H

#include "../core/gg_types.h"
#include "gg_gtk.h"

typedef struct {
    GtkWidget *container;
    GtkWidget *symbol_entry;
    GtkWidget *point_entry;
    GtkWidget *remove_button;
    GtkWidget *error_label;
} GGGradeRow;

GGGradeRow *gg_grade_row_create(const GGGradeItem *initial_item, GCallback on_changed, GCallback on_remove,
                                gpointer user_data);
void gg_grade_row_get_item(const GGGradeRow *row, GGGradeItem *out_item);
void gg_grade_row_set_error(GGGradeRow *row, const char *error_message);
void gg_grade_row_clear_error(GGGradeRow *row);
void gg_grade_row_destroy(GGGradeRow *row);

#endif
