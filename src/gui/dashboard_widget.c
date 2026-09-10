#include "dashboard_widget.h"
#include "gg_semester_list_item.h"
#include "gg_confirmation_dialog.h"
#include "../core/cgpa_calculator.h"
#include "../core/course_list.h"
#include "../io/backup_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    GGAppContext *ctx;
    GtkWidget *container;
    GtkWidget *cgpa_val_label;
    GtkWidget *tcp_val_label;
    GtkWidget *tcu_val_label;
    GtkWidget *standing_val_label;
    GtkWidget *semesters_box;
    GtkWidget *empty_label;
} GGDashboardState;

typedef struct {
    GGDashboardState *dashboard_state;
    GtkWidget *dialog_window;
    GtkWidget *sem_entry;
    GtkWidget *code_entry;
    GtkWidget *unit_entry;
    GtkWidget *grade_dropdown;
    GGGradingScale scale;
} GGQuickAddDialogState;

typedef struct {
    GGDashboardState *dashboard_state;
    GtkWidget *dialog_window;
    GtkWidget *sem_entry;
    GtkWidget *code_entry;
    GtkWidget *unit_entry;
    GtkWidget *grade_dropdown;
    GGCourseEntry course;
    GGGradingScale scale;
} GGEditCourseDialogState;

typedef struct {
    GGDashboardState *dashboard_state;
    int64_t course_id;
} GGDashboardDeleteContext;

static const char *get_class_standing(double cgpa) {
    if (cgpa >= 4.50) {
        return "First Class Honours";
    } else if (cgpa >= 3.50) {
        return "Second Class Honours (Upper Division)";
    } else if (cgpa >= 2.40) {
        return "Second Class Honours (Lower Division)";
    } else if (cgpa >= 1.50) {
        return "Third Class Honours";
    } else if (cgpa >= 1.00) {
        return "Pass";
    }
    return "Below Pass Threshold";
}

static void on_quick_add_submit(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GGQuickAddDialogState *dlg = (GGQuickAddDialogState *)user_data;
    if (dlg == NULL || dlg->dashboard_state == NULL || dlg->dashboard_state->ctx == NULL) {
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

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "%s", sem);
    snprintf(entry.course_label, sizeof(entry.course_label), "%s", code != NULL ? code : "");
    entry.credit_unit = (uint32_t)units;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "%s", dlg->scale.items[selected].grade_symbol);
    entry.entry_date = (int64_t)time(NULL);

    int64_t new_id = 0;
    GGStatus st = dlg->dashboard_state->ctx->course_repo->insert_course(dlg->dashboard_state->ctx->course_repo->context, &entry, &new_id);
    if (st == GG_OK) {
        gg_backup_create_snapshot(dlg->dashboard_state->ctx->db_filepath, dlg->dashboard_state->ctx->backup_dir);
        gg_dashboard_widget_refresh(dlg->dashboard_state->container);
        gtk_window_destroy(GTK_WINDOW(dlg->dialog_window));
    }
}

static void on_quick_add_cancel(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GGQuickAddDialogState *dlg = (GGQuickAddDialogState *)user_data;
    if (dlg != NULL && dlg->dialog_window != NULL) {
        gtk_window_destroy(GTK_WINDOW(dlg->dialog_window));
    }
}

static void on_quick_add_course_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGDashboardState *state = (GGDashboardState *)user_data;
    if (state == NULL || state->ctx == NULL || state->ctx->scale_repo == NULL) {
        return;
    }

    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    GGStatus st = state->ctx->scale_repo->load_scale(state->ctx->scale_repo->context, &scale);
    if (st != GG_OK || scale.count == 0) {
        return;
    }

    GGQuickAddDialogState *dlg = (GGQuickAddDialogState *)calloc(1, sizeof(GGQuickAddDialogState));
    if (dlg == NULL) {
        return;
    }
    dlg->dashboard_state = state;
    memcpy(&dlg->scale, &scale, sizeof(scale));

    dlg->dialog_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dlg->dialog_window), "Quick-Add Course");
    gtk_window_set_modal(GTK_WINDOW(dlg->dialog_window), TRUE);
    if (state->ctx->main_window != NULL) {
        gtk_window_set_transient_for(GTK_WINDOW(dlg->dialog_window), state->ctx->main_window);
    }
    gtk_window_set_default_size(GTK_WINDOW(dlg->dialog_window), 420, 320);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    gtk_window_set_child(GTK_WINDOW(dlg->dialog_window), box);

    GtkWidget *title = gtk_label_new("Enter Course Details");
    gtk_widget_add_css_class(title, "title-3");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), title);

    dlg->sem_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(dlg->sem_entry), "Semester (e.g. Year 1 Sem 1)");
    gtk_box_append(GTK_BOX(box), dlg->sem_entry);

    dlg->code_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(dlg->code_entry), "Course Code (e.g. CSC101)");
    gtk_box_append(GTK_BOX(box), dlg->code_entry);

    dlg->unit_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(dlg->unit_entry), "Credit Units (1-10)");
    gtk_box_append(GTK_BOX(box), dlg->unit_entry);

    const char *symbols[17];
    for (size_t i = 0; i < scale.count; i++) {
        symbols[i] = scale.items[i].grade_symbol;
    }
    symbols[scale.count] = NULL;
    GtkStringList *slist = gtk_string_list_new(symbols);
    dlg->grade_dropdown = gtk_drop_down_new(G_LIST_MODEL(slist), NULL);
    gtk_box_append(GTK_BOX(box), dlg->grade_dropdown);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 12);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_quick_add_cancel), dlg);
    GtkWidget *add_btn = gtk_button_new_with_label("Add Course");
    gtk_widget_add_css_class(add_btn, "suggested-action");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_quick_add_submit), dlg);

    gtk_box_append(GTK_BOX(btn_box), cancel_btn);
    gtk_box_append(GTK_BOX(btn_box), add_btn);
    gtk_box_append(GTK_BOX(box), btn_box);

    g_object_set_data_full(G_OBJECT(dlg->dialog_window), "dlg_state", dlg, free);
    gtk_window_present(GTK_WINDOW(dlg->dialog_window));
}

static void on_edit_course_submit(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GGEditCourseDialogState *dlg = (GGEditCourseDialogState *)user_data;
    if (dlg == NULL || dlg->dashboard_state == NULL || dlg->dashboard_state->ctx == NULL) {
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

    GGStatus st = dlg->dashboard_state->ctx->course_repo->update_course(dlg->dashboard_state->ctx->course_repo->context, &dlg->course);
    if (st == GG_OK) {
        gg_backup_create_snapshot(dlg->dashboard_state->ctx->db_filepath, dlg->dashboard_state->ctx->backup_dir);
        gg_dashboard_widget_refresh(dlg->dashboard_state->container);
        gtk_window_destroy(GTK_WINDOW(dlg->dialog_window));
    }
}

static void on_edit_course_cancel(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GGEditCourseDialogState *dlg = (GGEditCourseDialogState *)user_data;
    if (dlg != NULL && dlg->dialog_window != NULL) {
        gtk_window_destroy(GTK_WINDOW(dlg->dialog_window));
    }
}

static void on_dashboard_edit_course(int64_t course_id, gpointer user_data) {
    GGDashboardState *state = (GGDashboardState *)user_data;
    if (state == NULL || state->ctx == NULL || state->ctx->course_repo == NULL || state->ctx->scale_repo == NULL) {
        return;
    }

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    GGStatus st = state->ctx->course_repo->get_course_by_id(state->ctx->course_repo->context, course_id, &entry);
    if (st != GG_OK) {
        return;
    }

    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    st = state->ctx->scale_repo->load_scale(state->ctx->scale_repo->context, &scale);
    if (st != GG_OK || scale.count == 0) {
        return;
    }

    GGEditCourseDialogState *dlg = (GGEditCourseDialogState *)calloc(1, sizeof(GGEditCourseDialogState));
    if (dlg == NULL) {
        return;
    }
    dlg->dashboard_state = state;
    memcpy(&dlg->course, &entry, sizeof(entry));
    memcpy(&dlg->scale, &scale, sizeof(scale));

    dlg->dialog_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dlg->dialog_window), "Edit Course Record");
    gtk_window_set_modal(GTK_WINDOW(dlg->dialog_window), TRUE);
    if (state->ctx->main_window != NULL) {
        gtk_window_set_transient_for(GTK_WINDOW(dlg->dialog_window), state->ctx->main_window);
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

    dlg->sem_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(dlg->sem_entry), entry.semester_label);
    gtk_box_append(GTK_BOX(box), dlg->sem_entry);

    dlg->code_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(dlg->code_entry), entry.course_label);
    gtk_box_append(GTK_BOX(box), dlg->code_entry);

    dlg->unit_entry = gtk_entry_new();
    char ubuf[16];
    snprintf(ubuf, sizeof(ubuf), "%u", entry.credit_unit);
    gtk_editable_set_text(GTK_EDITABLE(dlg->unit_entry), ubuf);
    gtk_box_append(GTK_BOX(box), dlg->unit_entry);

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

static void on_dashboard_delete_confirmed(bool confirmed, gpointer user_data) {
    GGDashboardDeleteContext *ctx = (GGDashboardDeleteContext *)user_data;
    if (confirmed && ctx != NULL && ctx->dashboard_state != NULL && ctx->dashboard_state->ctx != NULL) {
        ctx->dashboard_state->ctx->course_repo->delete_course(ctx->dashboard_state->ctx->course_repo->context, ctx->course_id);
        gg_backup_create_snapshot(ctx->dashboard_state->ctx->db_filepath, ctx->dashboard_state->ctx->backup_dir);
        gg_dashboard_widget_refresh(ctx->dashboard_state->container);
    }
    free(ctx);
}

static void on_dashboard_delete_course(int64_t course_id, gpointer user_data) {
    GGDashboardState *state = (GGDashboardState *)user_data;
    if (state == NULL || state->ctx == NULL) {
        return;
    }
    GGDashboardDeleteContext *del_ctx = (GGDashboardDeleteContext *)calloc(1, sizeof(GGDashboardDeleteContext));
    if (del_ctx == NULL) {
        return;
    }
    del_ctx->dashboard_state = state;
    del_ctx->course_id = course_id;
    gg_confirmation_dialog_show(
        state->ctx->main_window,
        "Delete Course Entry",
        "Delete this course record?",
        "This action cannot be undone and will immediately update your CGPA.",
        "Delete",
        "Cancel",
        true,
        on_dashboard_delete_confirmed,
        del_ctx
    );
}

void gg_dashboard_widget_refresh(GtkWidget *widget) {
    if (widget == NULL) {
        return;
    }
    GGDashboardState *state = (GGDashboardState *)g_object_get_data(G_OBJECT(widget), "state");
    if (state == NULL || state->ctx == NULL || state->ctx->course_repo == NULL) {
        return;
    }

    double tcp = 0.0;
    uint32_t tcu = 0;
    state->ctx->course_repo->get_live_totals(state->ctx->course_repo->context, &tcp, &tcu);

    double cgpa = 0.0;
    gg_cgpa_calculate(tcp, tcu, &cgpa);

    char cgpa_buf[32];
    snprintf(cgpa_buf, sizeof(cgpa_buf), "%.2f", cgpa);
    gtk_label_set_text(GTK_LABEL(state->cgpa_val_label), cgpa_buf);

    char tcp_buf[32];
    snprintf(tcp_buf, sizeof(tcp_buf), "%.2f", tcp);
    gtk_label_set_text(GTK_LABEL(state->tcp_val_label), tcp_buf);

    char tcu_buf[32];
    snprintf(tcu_buf, sizeof(tcu_buf), "%u", tcu);
    gtk_label_set_text(GTK_LABEL(state->tcu_val_label), tcu_buf);

    gtk_label_set_text(GTK_LABEL(state->standing_val_label), get_class_standing(cgpa));

    GtkWidget *child = gtk_widget_get_first_child(state->semesters_box);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(state->semesters_box), child);
        child = next;
    }

    GGCourseList *list = NULL;
    state->ctx->course_repo->list_all_courses(state->ctx->course_repo->context, &list);

    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    if (state->ctx->scale_repo != NULL) {
        state->ctx->scale_repo->load_scale(state->ctx->scale_repo->context, &scale);
    }

    if (list != NULL && list->count > 0) {
        gtk_widget_set_visible(state->empty_label, FALSE);

        char seen_semesters[32][32];
        size_t seen_count = 0;

        for (size_t i = 0; i < list->count; i++) {
            bool found = false;
            for (size_t s = 0; s < seen_count; s++) {
                if (strcmp(seen_semesters[s], list->entries[i].semester_label) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found && seen_count < 32) {
                snprintf(seen_semesters[seen_count], sizeof(seen_semesters[seen_count]), "%s", list->entries[i].semester_label);
                seen_count++;
            }
        }

        for (size_t s = 0; s < seen_count; s++) {
            GGSemesterListItem *item = gg_semester_list_item_create(
                seen_semesters[s],
                list,
                &scale,
                G_CALLBACK(on_dashboard_edit_course),
                G_CALLBACK(on_dashboard_delete_course),
                state
            );
            if (item != NULL) {
                gtk_box_append(GTK_BOX(state->semesters_box), item->container);
            }
        }
    } else {
        gtk_widget_set_visible(state->empty_label, TRUE);
    }

    if (list != NULL) {
        gg_course_list_destroy(list);
    }
}

static GtkWidget *create_metric_card(const char *title, GtkWidget **val_label_out, const char *accent_class) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(card, "card");
    gtk_widget_add_css_class(card, accent_class);
    gtk_widget_set_margin_start(card, 6);
    gtk_widget_set_margin_end(card, 6);
    gtk_widget_set_margin_top(card, 6);
    gtk_widget_set_margin_bottom(card, 6);
    gtk_widget_set_hexpand(card, TRUE);

    GtkWidget *lbl_title = gtk_label_new(title);
    gtk_widget_set_halign(lbl_title, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl_title, "caption");

    GtkWidget *lbl_val = gtk_label_new("0.00");
    gtk_widget_set_halign(lbl_val, GTK_ALIGN_START);
    gtk_widget_add_css_class(lbl_val, "title-1");

    gtk_box_append(GTK_BOX(card), lbl_title);
    gtk_box_append(GTK_BOX(card), lbl_val);

    *val_label_out = lbl_val;
    return card;
}

GtkWidget *gg_dashboard_widget_create(GGAppContext *ctx) {
    GGDashboardState *state = (GGDashboardState *)calloc(1, sizeof(GGDashboardState));
    if (state == NULL) {
        return NULL;
    }
    state->ctx = ctx;

    state->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(state->container, 20);
    gtk_widget_set_margin_bottom(state->container, 20);
    gtk_widget_set_margin_start(state->container, 24);
    gtk_widget_set_margin_end(state->container, 24);

    GtkWidget *screen_title = gtk_label_new("Dashboard");
    gtk_widget_add_css_class(screen_title, "screen-title");
    gtk_widget_set_halign(screen_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(state->container), screen_title);

    GtkWidget *metrics_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *card_cgpa = create_metric_card("CURRENT CGPA", &state->cgpa_val_label, "accent-cgpa");
    gtk_widget_remove_css_class(state->cgpa_val_label, "title-1");
    gtk_widget_add_css_class(state->cgpa_val_label, "cgpa-large-number");
    GtkWidget *card_tcp = create_metric_card("TOTAL POINTS (TCP)", &state->tcp_val_label, "accent-tcp");
    GtkWidget *card_tcu = create_metric_card("TOTAL UNITS (TCU)", &state->tcu_val_label, "accent-tcu");
    GtkWidget *card_standing = create_metric_card("CLASS STANDING", &state->standing_val_label, "accent-standing");

    gtk_box_append(GTK_BOX(metrics_box), card_cgpa);
    gtk_box_append(GTK_BOX(metrics_box), card_tcp);
    gtk_box_append(GTK_BOX(metrics_box), card_tcu);
    gtk_box_append(GTK_BOX(metrics_box), card_standing);
    gtk_box_append(GTK_BOX(state->container), metrics_box);

    GtkWidget *section_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *semesters_title = gtk_label_new("Academic History Overview");
    gtk_widget_add_css_class(semesters_title, "title-3");
    gtk_widget_set_halign(semesters_title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(semesters_title, TRUE);

    GtkWidget *quick_add_btn = gtk_button_new_with_label("+ Quick-Add Course");
    gtk_widget_add_css_class(quick_add_btn, "suggested-action");
    g_signal_connect(quick_add_btn, "clicked", G_CALLBACK(on_quick_add_course_clicked), state);

    gtk_box_append(GTK_BOX(section_header), semesters_title);
    gtk_box_append(GTK_BOX(section_header), quick_add_btn);
    gtk_box_append(GTK_BOX(state->container), section_header);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);

    GtkWidget *scroll_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    state->semesters_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_append(GTK_BOX(scroll_content), state->semesters_box);

    state->empty_label = gtk_label_new("No academic semesters recorded yet.\nNavigate to History or use Quick Add to enter your coursework.");
    gtk_widget_add_css_class(state->empty_label, "dim-label");
    gtk_widget_set_margin_top(state->empty_label, 40);
    gtk_box_append(GTK_BOX(scroll_content), state->empty_label);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), scroll_content);
    gtk_box_append(GTK_BOX(state->container), scroll);

    g_object_set_data_full(G_OBJECT(state->container), "state", state, free);

    gg_dashboard_widget_refresh(state->container);

    return state->container;
}
