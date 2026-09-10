#include "history_widget.h"
#include "../core/course_list.h"
#include "../io/backup_manager.h"
#include "gg_confirmation_dialog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    GGAppContext *ctx;
    GtkWidget *container;
    GtkWidget *table_box;
    GtkWidget *empty_label;
    GtkWidget *filter_dropdown;
    GtkWidget *sem_entry;
    GtkWidget *code_entry;
    GtkWidget *unit_entry;
    GtkWidget *grade_dropdown;
    char current_filter_semester[32];
    GGGradingScale current_scale;
    bool is_refreshing_filter;
} GGHistoryState;

typedef struct {
    GGHistoryState *state;
    int64_t course_id;
} GGDeleteCourseContext;

typedef struct {
    GGHistoryState *state;
    GtkWidget *dialog_window;
    GtkWidget *sem_entry;
    GtkWidget *code_entry;
    GtkWidget *unit_entry;
    GtkWidget *grade_dropdown;
    GGCourseEntry course;
    GGGradingScale scale;
} GGHistoryEditDialogState;

static const char *get_grade_badge_class(const char *symbol) {
    if (symbol == NULL || symbol[0] == '\0') {
        return "badge";
    }
    char c = symbol[0];
    if (c == 'A' || c == 'a') {
        return "badge-grade-a";
    } else if (c == 'B' || c == 'b') {
        return "badge-grade-b";
    } else if (c == 'C' || c == 'c') {
        return "badge-grade-c";
    } else if (c == 'D' || c == 'd') {
        return "badge-grade-d";
    }
    return "badge-grade-f";
}

static void on_delete_confirmed(bool confirmed, gpointer user_data) {
    GGDeleteCourseContext *ctx = (GGDeleteCourseContext *)user_data;
    if (confirmed && ctx != NULL && ctx->state != NULL && ctx->state->ctx != NULL &&
        ctx->state->ctx->course_repo != NULL) {
        ctx->state->ctx->course_repo->delete_course(ctx->state->ctx->course_repo->context, ctx->course_id);
        gg_backup_create_snapshot(ctx->state->ctx->db_filepath, ctx->state->ctx->backup_dir);
        gg_history_widget_refresh(ctx->state->container);
    }
    free(ctx);
}

static void on_delete_course_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGDeleteCourseContext *ctx = (GGDeleteCourseContext *)user_data;
    if (ctx == NULL || ctx->state == NULL) {
        return;
    }
    GtkWindow *win = ctx->state->ctx != NULL ? ctx->state->ctx->main_window : NULL;
    gg_confirmation_dialog_show(win, "Delete Course Entry", "Delete this course entry?",
                                "This action cannot be undone and will immediately update your CGPA.", "Delete",
                                "Cancel", true, on_delete_confirmed, ctx);
}

static void on_edit_course_submit(GtkButton *button, gpointer user_data) {
    (void)button;
    GGHistoryEditDialogState *dlg = (GGHistoryEditDialogState *)user_data;
    if (dlg == NULL || dlg->state == NULL || dlg->state->ctx == NULL || dlg->state->ctx->course_repo == NULL) {
        return;
    }

    const char *sem = gtk_editable_get_text(GTK_EDITABLE(dlg->sem_entry));
    const char *code = gtk_editable_get_text(GTK_EDITABLE(dlg->code_entry));
    const char *unit_str = gtk_editable_get_text(GTK_EDITABLE(dlg->unit_entry));
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dlg->grade_dropdown));

    if (sem == NULL || sem[0] == '\0' || unit_str == NULL || unit_str[0] == '\0') {
        return;
    }
    unsigned long units = strtoul(unit_str, NULL, 10);
    if (units == 0 || units > 10) {
        return;
    }
    if (selected == GTK_INVALID_LIST_POSITION || (size_t)selected >= dlg->scale.count) {
        return;
    }

    snprintf(dlg->course.semester_label, sizeof(dlg->course.semester_label), "%s", sem);
    snprintf(dlg->course.course_label, sizeof(dlg->course.course_label), "%s", code != NULL ? code : "");
    dlg->course.credit_unit = (uint32_t)units;
    snprintf(dlg->course.grade_symbol, sizeof(dlg->course.grade_symbol), "%s", dlg->scale.items[selected].grade_symbol);

    GGStatus st = dlg->state->ctx->course_repo->update_course(dlg->state->ctx->course_repo->context, &dlg->course);
    if (st == GG_OK) {
        gg_backup_create_snapshot(dlg->state->ctx->db_filepath, dlg->state->ctx->backup_dir);
        gg_history_widget_refresh(dlg->state->container);
        gtk_window_destroy(GTK_WINDOW(dlg->dialog_window));
    }
}

static void on_edit_course_cancel(GtkButton *button, gpointer user_data) {
    (void)button;
    GGHistoryEditDialogState *dlg = (GGHistoryEditDialogState *)user_data;
    if (dlg != NULL && dlg->dialog_window != NULL) {
        gtk_window_destroy(GTK_WINDOW(dlg->dialog_window));
    }
}

static void on_edit_course_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGDeleteCourseContext *ctx = (GGDeleteCourseContext *)user_data;
    if (ctx == NULL || ctx->state == NULL || ctx->state->ctx == NULL || ctx->state->ctx->course_repo == NULL) {
        return;
    }

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    GGStatus st =
        ctx->state->ctx->course_repo->get_course_by_id(ctx->state->ctx->course_repo->context, ctx->course_id, &entry);
    if (st != GG_OK) {
        return;
    }

    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    if (ctx->state->ctx->scale_repo != NULL) {
        ctx->state->ctx->scale_repo->load_scale(ctx->state->ctx->scale_repo->context, &scale);
    }
    if (scale.count == 0) {
        return;
    }

    GGHistoryEditDialogState *dlg = (GGHistoryEditDialogState *)calloc(1, sizeof(GGHistoryEditDialogState));
    if (dlg == NULL) {
        return;
    }
    dlg->state = ctx->state;
    memcpy(&dlg->course, &entry, sizeof(entry));
    memcpy(&dlg->scale, &scale, sizeof(scale));

    dlg->dialog_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dlg->dialog_window), "Edit Course Entry");
    gtk_window_set_modal(GTK_WINDOW(dlg->dialog_window), TRUE);
    if (ctx->state->ctx->main_window != NULL) {
        gtk_window_set_transient_for(GTK_WINDOW(dlg->dialog_window), ctx->state->ctx->main_window);
    }
    gtk_window_set_default_size(GTK_WINDOW(dlg->dialog_window), 420, 320);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    gtk_window_set_child(GTK_WINDOW(dlg->dialog_window), box);

    GtkWidget *title = gtk_label_new("Update Course Entry");
    gtk_widget_add_css_class(title, "title-3");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *lbl_sem = gtk_label_new("Semester");
    gtk_widget_set_halign(lbl_sem, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl_sem, "form-label");
    dlg->sem_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(dlg->sem_entry), entry.semester_label);
    gtk_box_append(GTK_BOX(box), lbl_sem);
    gtk_box_append(GTK_BOX(box), dlg->sem_entry);

    GtkWidget *lbl_code = gtk_label_new("Course Code");
    gtk_widget_set_halign(lbl_code, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl_code, "form-label");
    dlg->code_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(dlg->code_entry), entry.course_label);
    gtk_box_append(GTK_BOX(box), lbl_code);
    gtk_box_append(GTK_BOX(box), dlg->code_entry);

    GtkWidget *lbl_unit = gtk_label_new("Credit Units");
    gtk_widget_set_halign(lbl_unit, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl_unit, "form-label");
    dlg->unit_entry = gtk_entry_new();
    char ubuf[16];
    snprintf(ubuf, sizeof(ubuf), "%u", entry.credit_unit);
    gtk_editable_set_text(GTK_EDITABLE(dlg->unit_entry), ubuf);
    gtk_box_append(GTK_BOX(box), lbl_unit);
    gtk_box_append(GTK_BOX(box), dlg->unit_entry);

    GtkWidget *lbl_grade = gtk_label_new("Assigned Grade");
    gtk_widget_set_halign(lbl_grade, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl_grade, "form-label");
    gtk_box_append(GTK_BOX(box), lbl_grade);

    const char *symbols[17];
    guint selected_idx = 0;
    for (size_t i = 0; i < scale.count; i++) {
        symbols[i] = scale.items[i].grade_symbol;
        if (strcmp(scale.items[i].grade_symbol, entry.grade_symbol) == 0) {
            selected_idx = (guint)i;
        }
    }
    symbols[scale.count] = NULL;
    GtkStringList *slist = gtk_string_list_new(symbols);
    dlg->grade_dropdown = gtk_drop_down_new(G_LIST_MODEL(slist), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dlg->grade_dropdown), selected_idx);
    gtk_box_append(GTK_BOX(box), dlg->grade_dropdown);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 12);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_edit_course_cancel), dlg);
    GtkWidget *save_btn = gtk_button_new_with_label("Save Changes");
    gtk_widget_add_css_class(save_btn, "suggested-action");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_edit_course_submit), dlg);

    gtk_box_append(GTK_BOX(btn_box), cancel_btn);
    gtk_box_append(GTK_BOX(btn_box), save_btn);
    gtk_box_append(GTK_BOX(box), btn_box);

    g_object_set_data_full(G_OBJECT(dlg->dialog_window), "dlg_state", dlg, free);
    gtk_window_present(GTK_WINDOW(dlg->dialog_window));
}

static void on_add_course_submitted(GtkButton *button, gpointer user_data) {
    (void)button;
    GGHistoryState *state = (GGHistoryState *)user_data;
    if (state == NULL || state->ctx == NULL || state->ctx->course_repo == NULL) {
        return;
    }

    const char *sem = gtk_editable_get_text(GTK_EDITABLE(state->sem_entry));
    const char *code = gtk_editable_get_text(GTK_EDITABLE(state->code_entry));
    const char *unit_str = gtk_editable_get_text(GTK_EDITABLE(state->unit_entry));
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(state->grade_dropdown));

    if (sem == NULL || sem[0] == '\0' || unit_str == NULL || unit_str[0] == '\0') {
        return;
    }

    unsigned long units = strtoul(unit_str, NULL, 10);
    if (units == 0 || units > 10) {
        return;
    }

    if (selected == GTK_INVALID_LIST_POSITION || (size_t)selected >= state->current_scale.count) {
        return;
    }

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "%s", sem);
    snprintf(entry.course_label, sizeof(entry.course_label), "%s", code != NULL ? code : "");
    entry.credit_unit = (uint32_t)units;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "%s", state->current_scale.items[selected].grade_symbol);
    entry.entry_date = (int64_t)time(NULL);

    int64_t out_id = 0;
    GGStatus status = state->ctx->course_repo->insert_course(state->ctx->course_repo->context, &entry, &out_id);
    if (status == GG_OK) {
        gg_backup_create_snapshot(state->ctx->db_filepath, state->ctx->backup_dir);
        gtk_editable_set_text(GTK_EDITABLE(state->code_entry), "");
        gtk_editable_set_text(GTK_EDITABLE(state->unit_entry), "");
        gg_history_widget_refresh(state->container);
    }
}

static void on_filter_changed(GObject *object, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    GGHistoryState *state = (GGHistoryState *)user_data;
    if (state == NULL || state->is_refreshing_filter) {
        return;
    }

    GtkDropDown *dd = GTK_DROP_DOWN(object);
    guint selected = gtk_drop_down_get_selected(dd);
    if (selected == 0 || selected == GTK_INVALID_LIST_POSITION) {
        state->current_filter_semester[0] = '\0';
    } else {
        GListModel *model = gtk_drop_down_get_model(dd);
        const char *chosen = gtk_string_list_get_string(GTK_STRING_LIST(model), selected);
        if (chosen != NULL) {
            snprintf(state->current_filter_semester, sizeof(state->current_filter_semester), "%s", chosen);
        }
    }
    gg_history_widget_refresh(state->container);
}

static void on_new_semester_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGHistoryState *state = (GGHistoryState *)user_data;
    if (state == NULL) {
        return;
    }
    gtk_widget_grab_focus(state->sem_entry);
    gtk_editable_set_text(GTK_EDITABLE(state->sem_entry), "");
}

void gg_history_widget_refresh(GtkWidget *widget) {
    if (widget == NULL) {
        return;
    }
    GGHistoryState *state = (GGHistoryState *)g_object_get_data(G_OBJECT(widget), "state");
    if (state == NULL || state->ctx == NULL || state->ctx->course_repo == NULL) {
        return;
    }

    memset(&state->current_scale, 0, sizeof(state->current_scale));
    if (state->ctx->scale_repo != NULL) {
        state->ctx->scale_repo->load_scale(state->ctx->scale_repo->context, &state->current_scale);
    }

    if (state->current_scale.count > 0 && state->grade_dropdown != NULL) {
        const char *symbols[17];
        for (size_t i = 0; i < state->current_scale.count; i++) {
            symbols[i] = state->current_scale.items[i].grade_symbol;
        }
        symbols[state->current_scale.count] = NULL;
        GtkStringList *slist = gtk_string_list_new(symbols);
        gtk_drop_down_set_model(GTK_DROP_DOWN(state->grade_dropdown), G_LIST_MODEL(slist));
    }

    GGCourseList *all_courses = NULL;
    state->ctx->course_repo->list_all_courses(state->ctx->course_repo->context, &all_courses);

    state->is_refreshing_filter = true;
    char seen_semesters[64][32];
    size_t seen_count = 0;

    if (all_courses != NULL) {
        for (size_t i = 0; i < all_courses->count; i++) {
            bool found = false;
            for (size_t s = 0; s < seen_count; s++) {
                if (strcmp(seen_semesters[s], all_courses->entries[i].semester_label) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found && seen_count < 64) {
                snprintf(seen_semesters[seen_count], sizeof(seen_semesters[seen_count]), "%s",
                         all_courses->entries[i].semester_label);
                seen_count++;
            }
        }
    }

    const char *filter_items[66];
    filter_items[0] = "All Semesters";
    for (size_t s = 0; s < seen_count; s++) {
        filter_items[s + 1] = seen_semesters[s];
    }
    filter_items[seen_count + 1] = NULL;

    GtkStringList *filter_slist = gtk_string_list_new(filter_items);
    gtk_drop_down_set_model(GTK_DROP_DOWN(state->filter_dropdown), G_LIST_MODEL(filter_slist));

    guint active_idx = 0;
    if (state->current_filter_semester[0] != '\0') {
        for (size_t s = 0; s < seen_count; s++) {
            if (strcmp(seen_semesters[s], state->current_filter_semester) == 0) {
                active_idx = (guint)(s + 1);
                break;
            }
        }
    }
    gtk_drop_down_set_selected(GTK_DROP_DOWN(state->filter_dropdown), active_idx);
    state->is_refreshing_filter = false;

    if (all_courses != NULL) {
        gg_course_list_destroy(all_courses);
        all_courses = NULL;
    }

    GtkWidget *child = gtk_widget_get_first_child(state->table_box);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(state->table_box), child);
        child = next;
    }

    GGCourseList *list = NULL;
    if (state->current_filter_semester[0] != '\0') {
        state->ctx->course_repo->list_courses_by_semester(state->ctx->course_repo->context,
                                                          state->current_filter_semester, &list);
    } else {
        state->ctx->course_repo->list_all_courses(state->ctx->course_repo->context, &list);
    }

    if (list != NULL && list->count > 0) {
        gtk_widget_set_visible(state->empty_label, FALSE);

        GtkWidget *table_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(table_card, "data-table-container");

        GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(header_row, "data-table-header");

        GtkWidget *th_sem = gtk_label_new("SEMESTER");
        gtk_widget_set_size_request(th_sem, 160, -1);
        gtk_widget_set_halign(th_sem, GTK_ALIGN_START);

        GtkWidget *th_code = gtk_label_new("COURSE CODE");
        gtk_widget_set_hexpand(th_code, TRUE);
        gtk_widget_set_halign(th_code, GTK_ALIGN_START);

        GtkWidget *th_unit = gtk_label_new("CREDITS");
        gtk_widget_set_size_request(th_unit, 90, -1);
        gtk_widget_set_halign(th_unit, GTK_ALIGN_START);

        GtkWidget *th_grade = gtk_label_new("GRADE");
        gtk_widget_set_size_request(th_grade, 70, -1);
        gtk_widget_set_halign(th_grade, GTK_ALIGN_START);

        GtkWidget *th_act = gtk_label_new("ACTIONS");
        gtk_widget_set_size_request(th_act, 130, -1);
        gtk_widget_set_halign(th_act, GTK_ALIGN_END);

        gtk_box_append(GTK_BOX(header_row), th_sem);
        gtk_box_append(GTK_BOX(header_row), th_code);
        gtk_box_append(GTK_BOX(header_row), th_unit);
        gtk_box_append(GTK_BOX(header_row), th_grade);
        gtk_box_append(GTK_BOX(header_row), th_act);
        gtk_box_append(GTK_BOX(table_card), header_row);

        for (size_t i = 0; i < list->count; i++) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_add_css_class(row, "data-table-row");

            GtkWidget *lbl_sem = gtk_label_new(list->entries[i].semester_label);
            gtk_widget_set_size_request(lbl_sem, 160, -1);
            gtk_widget_set_halign(lbl_sem, GTK_ALIGN_START);

            GtkWidget *lbl_code =
                gtk_label_new(list->entries[i].course_label[0] != '\0' ? list->entries[i].course_label : "—");
            gtk_widget_set_hexpand(lbl_code, TRUE);
            gtk_widget_set_halign(lbl_code, GTK_ALIGN_START);

            char unit_buf[16];
            snprintf(unit_buf, sizeof(unit_buf), "%u units", list->entries[i].credit_unit);
            GtkWidget *lbl_unit = gtk_label_new(unit_buf);
            gtk_widget_set_size_request(lbl_unit, 90, -1);
            gtk_widget_set_halign(lbl_unit, GTK_ALIGN_START);
            gtk_widget_add_css_class(lbl_unit, "dim-label");

            GtkWidget *lbl_grade = gtk_label_new(list->entries[i].grade_symbol);
            gtk_widget_add_css_class(lbl_grade, get_grade_badge_class(list->entries[i].grade_symbol));
            GtkWidget *grade_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_set_size_request(grade_box, 70, -1);
            gtk_widget_set_halign(grade_box, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(grade_box), lbl_grade);

            GtkWidget *act_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
            gtk_widget_set_size_request(act_box, 130, -1);
            gtk_widget_set_halign(act_box, GTK_ALIGN_END);

            GtkWidget *edit_btn = gtk_button_new_with_label("Edit");
            gtk_widget_add_css_class(edit_btn, "action-btn-sm");
            GGDeleteCourseContext *edit_ctx = (GGDeleteCourseContext *)calloc(1, sizeof(GGDeleteCourseContext));
            if (edit_ctx != NULL) {
                edit_ctx->state = state;
                edit_ctx->course_id = list->entries[i].id;
                g_object_set_data_full(G_OBJECT(edit_btn), "ctx", edit_ctx, free);
                g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_edit_course_clicked), edit_ctx);
            }

            GtkWidget *del_btn = gtk_button_new_with_label("Delete");
            gtk_widget_add_css_class(del_btn, "action-btn-sm");
            gtk_widget_add_css_class(del_btn, "destructive-action");
            GGDeleteCourseContext *del_ctx = (GGDeleteCourseContext *)calloc(1, sizeof(GGDeleteCourseContext));
            if (del_ctx != NULL) {
                del_ctx->state = state;
                del_ctx->course_id = list->entries[i].id;
                g_object_set_data_full(G_OBJECT(del_btn), "ctx", del_ctx, free);
                g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_course_clicked), del_ctx);
            }

            gtk_box_append(GTK_BOX(act_box), edit_btn);
            gtk_box_append(GTK_BOX(act_box), del_btn);

            gtk_box_append(GTK_BOX(row), lbl_sem);
            gtk_box_append(GTK_BOX(row), lbl_code);
            gtk_box_append(GTK_BOX(row), lbl_unit);
            gtk_box_append(GTK_BOX(row), grade_box);
            gtk_box_append(GTK_BOX(row), act_box);

            gtk_box_append(GTK_BOX(table_card), row);
        }
        gtk_box_append(GTK_BOX(state->table_box), table_card);
    } else {
        gtk_widget_set_visible(state->empty_label, TRUE);
    }

    if (list != NULL) {
        gg_course_list_destroy(list);
    }
}

GtkWidget *gg_history_widget_create(GGAppContext *ctx) {
    GGHistoryState *state = (GGHistoryState *)calloc(1, sizeof(GGHistoryState));
    if (state == NULL) {
        return NULL;
    }
    state->ctx = ctx;

    state->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(state->container, 20);
    gtk_widget_set_margin_bottom(state->container, 20);
    gtk_widget_set_margin_start(state->container, 24);
    gtk_widget_set_margin_end(state->container, 24);

    GtkWidget *title = gtk_label_new("History");
    gtk_widget_add_css_class(title, "screen-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(state->container), title);

    GtkWidget *screen_sub = gtk_label_new("View, filter, edit, or remove all recorded academic courses");
    gtk_widget_add_css_class(screen_sub, "dim-label");
    gtk_widget_set_halign(screen_sub, GTK_ALIGN_START);
    gtk_widget_set_margin_bottom(screen_sub, 4);
    gtk_box_append(GTK_BOX(state->container), screen_sub);

    GtkWidget *filter_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *filter_label = gtk_label_new("Filter by Semester:");
    gtk_widget_set_halign(filter_label, GTK_ALIGN_START);

    const char *initial_filters[] = {"All Semesters", NULL};
    GtkStringList *init_slist = gtk_string_list_new(initial_filters);
    state->filter_dropdown = gtk_drop_down_new(G_LIST_MODEL(init_slist), NULL);
    gtk_widget_set_size_request(state->filter_dropdown, 200, -1);
    g_signal_connect(state->filter_dropdown, "notify::selected", G_CALLBACK(on_filter_changed), state);

    GtkWidget *new_sem_btn = gtk_button_new_with_label("+ New Semester");
    g_signal_connect(new_sem_btn, "clicked", G_CALLBACK(on_new_semester_clicked), state);

    gtk_box_append(GTK_BOX(filter_bar), filter_label);
    gtk_box_append(GTK_BOX(filter_bar), state->filter_dropdown);
    gtk_box_append(GTK_BOX(filter_bar), new_sem_btn);
    gtk_box_append(GTK_BOX(state->container), filter_bar);

    GtkWidget *add_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(add_card, "card");
    gtk_widget_set_margin_bottom(add_card, 8);

    GtkWidget *add_title = gtk_label_new("Record New Course Entry");
    gtk_widget_add_css_class(add_title, "title-3");
    gtk_widget_set_halign(add_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(add_card), add_title);

    GtkWidget *form_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    GtkWidget *col_sem = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_sem = gtk_label_new("Semester");
    gtk_widget_set_halign(lbl_sem, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl_sem, "form-label");
    state->sem_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->sem_entry), "e.g. Year 1 Sem 1");
    gtk_widget_set_size_request(state->sem_entry, 180, -1);
    gtk_box_append(GTK_BOX(col_sem), lbl_sem);
    gtk_box_append(GTK_BOX(col_sem), state->sem_entry);

    GtkWidget *col_code = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(col_code, TRUE);
    GtkWidget *lbl_code = gtk_label_new("Course Code");
    gtk_widget_set_halign(lbl_code, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl_code, "form-label");
    state->code_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->code_entry), "e.g. MTH101");
    gtk_widget_set_hexpand(state->code_entry, TRUE);
    gtk_box_append(GTK_BOX(col_code), lbl_code);
    gtk_box_append(GTK_BOX(col_code), state->code_entry);

    GtkWidget *col_unit = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_unit = gtk_label_new("Credits");
    gtk_widget_set_halign(lbl_unit, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl_unit, "form-label");
    state->unit_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->unit_entry), "Units (1-10)");
    gtk_widget_set_size_request(state->unit_entry, 100, -1);
    gtk_box_append(GTK_BOX(col_unit), lbl_unit);
    gtk_box_append(GTK_BOX(col_unit), state->unit_entry);

    GtkWidget *col_grade = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_grade = gtk_label_new("Grade");
    gtk_widget_set_halign(lbl_grade, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl_grade, "form-label");
    const char *initial_grades[] = {"A", "B", "C", "D", "E", "F", NULL};
    GtkStringList *init_grades = gtk_string_list_new(initial_grades);
    state->grade_dropdown = gtk_drop_down_new(G_LIST_MODEL(init_grades), NULL);
    gtk_widget_set_size_request(state->grade_dropdown, 110, -1);
    gtk_box_append(GTK_BOX(col_grade), lbl_grade);
    gtk_box_append(GTK_BOX(col_grade), state->grade_dropdown);

    GtkWidget *col_btn = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl_spacer = gtk_label_new("");
    gtk_widget_add_css_class(lbl_spacer, "form-label");
    GtkWidget *add_btn = gtk_button_new_with_label("+ Add Course");
    gtk_widget_add_css_class(add_btn, "suggested-action");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_course_submitted), state);
    gtk_box_append(GTK_BOX(col_btn), lbl_spacer);
    gtk_box_append(GTK_BOX(col_btn), add_btn);

    gtk_box_append(GTK_BOX(form_row), col_sem);
    gtk_box_append(GTK_BOX(form_row), col_code);
    gtk_box_append(GTK_BOX(form_row), col_unit);
    gtk_box_append(GTK_BOX(form_row), col_grade);
    gtk_box_append(GTK_BOX(form_row), col_btn);
    gtk_box_append(GTK_BOX(add_card), form_row);
    gtk_box_append(GTK_BOX(state->container), add_card);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);

    GtkWidget *scroll_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    state->table_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_append(GTK_BOX(scroll_content), state->table_box);

    GtkWidget *empty_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(empty_card, "empty-state-card");
    gtk_widget_set_margin_top(empty_card, 24);

    GtkWidget *empty_title = gtk_label_new("No Course Records Found");
    gtk_widget_add_css_class(empty_title, "title-3");
    gtk_widget_set_halign(empty_title, GTK_ALIGN_CENTER);

    GtkWidget *empty_desc = gtk_label_new("Use the form above to add your coursework or import a backup spreadsheet from Settings.");
    gtk_widget_add_css_class(empty_desc, "dim-label");
    gtk_widget_set_halign(empty_desc, GTK_ALIGN_CENTER);

    gtk_box_append(GTK_BOX(empty_card), empty_title);
    gtk_box_append(GTK_BOX(empty_card), empty_desc);
    state->empty_label = empty_card;
    gtk_box_append(GTK_BOX(scroll_content), state->empty_label);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), scroll_content);
    gtk_box_append(GTK_BOX(state->container), scroll);

    g_object_set_data_full(G_OBJECT(state->container), "state", state, free);

    gg_history_widget_refresh(state->container);

    return state->container;
}
