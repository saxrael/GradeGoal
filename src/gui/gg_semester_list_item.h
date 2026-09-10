#ifndef GG_SEMESTER_LIST_ITEM_H
#define GG_SEMESTER_LIST_ITEM_H

#include "gg_gtk.h"
#include "../core/gg_types.h"

typedef struct {
    GtkWidget *container;
    GtkWidget *expander;
    GtkWidget *title_label;
    GtkWidget *tcp_label;
    GtkWidget *tcu_label;
    GtkWidget *gpa_label;
    GtkWidget *course_list_box;
} GGSemesterListItem;

GGSemesterListItem *gg_semester_list_item_create(
    const char *semester_label,
    const GGCourseList *courses,
    const GGGradingScale *scale,
    GCallback on_edit_course,
    GCallback on_delete_course,
    gpointer user_data
);
void gg_semester_list_item_destroy(GGSemesterListItem *item);

#endif
