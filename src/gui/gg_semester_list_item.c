#include "gg_semester_list_item.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int64_t course_id;
    GCallback callback;
    gpointer user_data;
} GGItemActionData;

static void on_item_action_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGItemActionData *act = (GGItemActionData *)user_data;
    if (act != NULL && act->callback != NULL) {
        ((void (*)(int64_t, gpointer))act->callback)(act->course_id, act->user_data);
    }
}

GGSemesterListItem *gg_semester_list_item_create(const char *semester_label, const GGCourseList *courses,
                                                 const GGGradingScale *scale, GCallback on_edit_course,
                                                 GCallback on_delete_course, gpointer user_data) {
    GGSemesterListItem *item = (GGSemesterListItem *)calloc(1, sizeof(GGSemesterListItem));
    if (item == NULL) {
        return NULL;
    }

    double sem_tcp = 0.0;
    uint32_t sem_tcu = 0;
    if (courses != NULL && scale != NULL) {
        for (size_t i = 0; i < courses->count; i++) {
            if (semester_label != NULL && strcmp(courses->entries[i].semester_label, semester_label) == 0) {
                sem_tcu += courses->entries[i].credit_unit;
                for (size_t s = 0; s < scale->count; s++) {
                    if (strcmp(scale->items[s].grade_symbol, courses->entries[i].grade_symbol) == 0) {
                        sem_tcp += (double)courses->entries[i].credit_unit * scale->items[s].grade_point;
                        break;
                    }
                }
            }
        }
    }

    double sem_gpa = sem_tcu > 0 ? (sem_tcp / (double)sem_tcu) : 0.0;

    item->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(item->container, 6);
    gtk_widget_set_margin_bottom(item->container, 6);
    gtk_widget_add_css_class(item->container, "card");

    char header_buf[128];
    snprintf(header_buf, sizeof(header_buf), "%s  —  GPA: %.2f (TCP: %.1f, TCU: %u)",
             semester_label != NULL ? semester_label : "Semester", sem_gpa, sem_tcp, sem_tcu);

    item->expander = gtk_expander_new(header_buf);
    gtk_box_append(GTK_BOX(item->container), item->expander);

    item->course_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(item->course_list_box, 16);
    gtk_widget_set_margin_end(item->course_list_box, 8);
    gtk_widget_set_margin_top(item->course_list_box, 4);

    if (courses != NULL) {
        for (size_t i = 0; i < courses->count; i++) {
            if (semester_label != NULL && strcmp(courses->entries[i].semester_label, semester_label) == 0) {
                GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                gtk_widget_set_margin_top(row, 2);
                gtk_widget_set_margin_bottom(row, 2);

                char course_line[128];
                snprintf(course_line, sizeof(course_line), "• %s (%u units) — Grade: %s",
                         courses->entries[i].course_label[0] != '\0' ? courses->entries[i].course_label : "Course",
                         courses->entries[i].credit_unit, courses->entries[i].grade_symbol);
                GtkWidget *lbl = gtk_label_new(course_line);
                gtk_widget_set_halign(lbl, GTK_ALIGN_START);
                gtk_widget_set_hexpand(lbl, TRUE);
                gtk_box_append(GTK_BOX(row), lbl);

                if (on_edit_course != NULL) {
                    GtkWidget *edit_btn = gtk_button_new_with_label("Edit");
                    GGItemActionData *act_edit = (GGItemActionData *)calloc(1, sizeof(GGItemActionData));
                    if (act_edit != NULL) {
                        act_edit->course_id = courses->entries[i].id;
                        act_edit->callback = on_edit_course;
                        act_edit->user_data = user_data;
                        g_object_set_data_full(G_OBJECT(edit_btn), "act_data", act_edit, free);
                        g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_item_action_clicked), act_edit);
                    }
                    gtk_box_append(GTK_BOX(row), edit_btn);
                }

                if (on_delete_course != NULL) {
                    GtkWidget *del_btn = gtk_button_new_with_label("Delete");
                    gtk_widget_add_css_class(del_btn, "destructive-action");
                    GGItemActionData *act_del = (GGItemActionData *)calloc(1, sizeof(GGItemActionData));
                    if (act_del != NULL) {
                        act_del->course_id = courses->entries[i].id;
                        act_del->callback = on_delete_course;
                        act_del->user_data = user_data;
                        g_object_set_data_full(G_OBJECT(del_btn), "act_data", act_del, free);
                        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_item_action_clicked), act_del);
                    }
                    gtk_box_append(GTK_BOX(row), del_btn);
                }

                gtk_box_append(GTK_BOX(item->course_list_box), row);
            }
        }
    }

    gtk_expander_set_child(GTK_EXPANDER(item->expander), item->course_list_box);

    return item;
}

void gg_semester_list_item_destroy(GGSemesterListItem *item) {
    if (item != NULL) {
        free(item);
    }
}
