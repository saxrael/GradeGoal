#include "settings_widget.h"
#include "../core/cgpa_calculator.h"
#include "../core/course_list.h"
#include "../core/scale_validator.h"
#include "../io/backup_manager.h"
#include "../io/pdf_exporter.h"
#include "../io/xlsx_exporter.h"
#include "../io/xlsx_importer.h"
#include "gg_confirmation_dialog.h"
#include "gg_grade_row.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    GGAppContext *ctx;
    GtkWidget *container;
    GtkWidget *scale_box;
    GtkWidget *save_scale_btn;
    GtkWidget *prior_status_label;
    GtkWidget *backup_status_label;
    GGGradeRow *rows[16];
    size_t row_count;
    GGGradingScale original_scale;
    GGGradingScale proposed_scale;
} GGSettingsState;

typedef struct {
    GGSettingsState *settings_state;
    GtkWidget *dialog_window;
    GtkWidget *tcp_entry;
    GtkWidget *tcu_entry;
    GtkWidget *zero_check;
    GtkWidget *preview_label;
} GGPriorStandingDialogState;

static void on_scale_save_confirmed(bool confirmed, gpointer user_data) {
    GGSettingsState *state = (GGSettingsState *)user_data;
    if (confirmed && state != NULL && state->ctx != NULL && state->ctx->scale_repo != NULL) {
        state->ctx->scale_repo->save_scale(state->ctx->scale_repo->context, &state->proposed_scale);
        memcpy(&state->original_scale, &state->proposed_scale, sizeof(GGGradingScale));
        gg_backup_create_snapshot(state->ctx->db_filepath, state->ctx->backup_dir);
        gg_settings_widget_refresh(state->container);
    }
}

static void on_save_scale_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGSettingsState *state = (GGSettingsState *)user_data;
    if (state == NULL || state->ctx == NULL || state->ctx->scale_repo == NULL || state->ctx->course_repo == NULL) {
        return;
    }

    memset(&state->proposed_scale, 0, sizeof(state->proposed_scale));
    state->proposed_scale.count = state->row_count;
    for (size_t i = 0; i < state->row_count; i++) {
        gg_grade_row_get_item(state->rows[i], &state->proposed_scale.items[i]);
    }
    if (state->proposed_scale.count > 0) {
        state->proposed_scale.max_point = state->proposed_scale.items[0].grade_point;
        state->proposed_scale.min_point = state->proposed_scale.items[state->proposed_scale.count - 1].grade_point;
    }

    GGStatus status = gg_scale_validator_validate_scale(&state->proposed_scale);
    if (status != GG_OK) {
        return;
    }

    bool points_changed = false;
    if (state->original_scale.count != state->proposed_scale.count) {
        points_changed = true;
    } else {
        for (size_t i = 0; i < state->original_scale.count; i++) {
            if (state->original_scale.items[i].grade_point != state->proposed_scale.items[i].grade_point) {
                points_changed = true;
                break;
            }
        }
    }

    if (!points_changed) {
        state->ctx->scale_repo->save_scale(state->ctx->scale_repo->context, &state->proposed_scale);
        memcpy(&state->original_scale, &state->proposed_scale, sizeof(GGGradingScale));
        gg_backup_create_snapshot(state->ctx->db_filepath, state->ctx->backup_dir);
        gg_settings_widget_refresh(state->container);
        return;
    }

    double before_tcp = 0.0;
    uint32_t tcu = 0;
    state->ctx->course_repo->get_live_totals(state->ctx->course_repo->context, &before_tcp, &tcu);
    double before_cgpa = 0.0;
    gg_cgpa_calculate(before_tcp, tcu, &before_cgpa);

    GGCourseList *courses = NULL;
    state->ctx->course_repo->list_all_courses(state->ctx->course_repo->context, &courses);

    double after_tcp = 0.0;
    if (courses != NULL) {
        for (size_t i = 0; i < courses->count; i++) {
            for (size_t s = 0; s < state->proposed_scale.count; s++) {
                if (strcmp(courses->entries[i].grade_symbol, state->proposed_scale.items[s].grade_symbol) == 0) {
                    after_tcp += (double)courses->entries[i].credit_unit * state->proposed_scale.items[s].grade_point;
                    break;
                }
            }
        }
        gg_course_list_destroy(courses);
    }

    double after_cgpa = 0.0;
    gg_cgpa_calculate(after_tcp, tcu, &after_cgpa);

    char detail_msg[256];
    snprintf(detail_msg, sizeof(detail_msg),
             "Your CGPA will change from %.2f to %.2f if you save this.\nDo you want to proceed?", before_cgpa,
             after_cgpa);

    gg_confirmation_dialog_show(state->ctx->main_window, "Confirm Grading Scale Edit",
                                "Modifying grading scale points affects all prior courses", detail_msg,
                                "Save and Recalculate", "Cancel", false, on_scale_save_confirmed, state);
}

static void get_current_prior_standing(GGSettingsState *state, double *out_tcp, uint32_t *out_tcu) {
    *out_tcp = 0.0;
    *out_tcu = 0;
    if (state == NULL || state->ctx == NULL || state->ctx->course_repo == NULL) {
        return;
    }
    GGCourseList *courses = NULL;
    GGStatus st = state->ctx->course_repo->list_courses_by_semester(state->ctx->course_repo->context,
                                                                    "Initial Standing", &courses);
    if (st == GG_OK && courses != NULL) {
        for (size_t i = 0; i < courses->count; i++) {
            *out_tcu += courses->entries[i].credit_unit;
            for (size_t s = 0; s < state->original_scale.count; s++) {
                if (strcmp(courses->entries[i].grade_symbol, state->original_scale.items[s].grade_symbol) == 0) {
                    *out_tcp += (double)courses->entries[i].credit_unit * state->original_scale.items[s].grade_point;
                    break;
                }
            }
        }
        gg_course_list_destroy(courses);
    }
}

static void on_prior_dialog_changed(GtkEditable *editable, gpointer user_data) {
    (void)editable;
    GGPriorStandingDialogState *dlg = (GGPriorStandingDialogState *)user_data;
    if (dlg == NULL || dlg->preview_label == NULL) {
        return;
    }
    gboolean is_zero = gtk_check_button_get_active(GTK_CHECK_BUTTON(dlg->zero_check));
    if (is_zero) {
        gtk_label_set_text(GTK_LABEL(dlg->preview_label), "Resulting Baseline CGPA: 0.00 (No prior standing)");
        return;
    }
    const char *tcp_str = gtk_editable_get_text(GTK_EDITABLE(dlg->tcp_entry));
    const char *tcu_str = gtk_editable_get_text(GTK_EDITABLE(dlg->tcu_entry));
    double tcp = tcp_str != NULL ? strtod(tcp_str, NULL) : 0.0;
    uint32_t tcu = tcu_str != NULL ? (uint32_t)strtoul(tcu_str, NULL, 10) : 0;
    double cgpa = 0.0;
    gg_cgpa_calculate(tcp, tcu, &cgpa);
    char buf[128];
    snprintf(buf, sizeof(buf), "Resulting Baseline CGPA: %.2f (from %.2f TCP / %u TCU)", cgpa, tcp, tcu);
    gtk_label_set_text(GTK_LABEL(dlg->preview_label), buf);
}

static void on_prior_zero_toggled(GtkCheckButton *check, gpointer user_data) {
    GGPriorStandingDialogState *dlg = (GGPriorStandingDialogState *)user_data;
    if (dlg == NULL) {
        return;
    }
    gboolean active = gtk_check_button_get_active(check);
    if (active) {
        gtk_editable_set_text(GTK_EDITABLE(dlg->tcp_entry), "0.00");
        gtk_editable_set_text(GTK_EDITABLE(dlg->tcu_entry), "0");
        gtk_widget_set_sensitive(dlg->tcp_entry, FALSE);
        gtk_widget_set_sensitive(dlg->tcu_entry, FALSE);
    } else {
        gtk_widget_set_sensitive(dlg->tcp_entry, TRUE);
        gtk_widget_set_sensitive(dlg->tcu_entry, TRUE);
    }
    on_prior_dialog_changed(NULL, dlg);
}

static void on_prior_dialog_cancel(GtkButton *button, gpointer user_data) {
    (void)button;
    GGPriorStandingDialogState *dlg = (GGPriorStandingDialogState *)user_data;
    if (dlg != NULL) {
        gtk_window_destroy(GTK_WINDOW(dlg->dialog_window));
    }
}

static void on_prior_standing_submit(GtkButton *button, gpointer user_data) {
    (void)button;
    GGPriorStandingDialogState *dlg = (GGPriorStandingDialogState *)user_data;
    if (dlg == NULL || dlg->settings_state == NULL || dlg->settings_state->ctx == NULL ||
        dlg->settings_state->ctx->course_repo == NULL) {
        return;
    }
    GGAppContext *ctx = dlg->settings_state->ctx;

    GGCourseList *initial_courses = NULL;
    GGStatus st =
        ctx->course_repo->list_courses_by_semester(ctx->course_repo->context, "Initial Standing", &initial_courses);
    if (st == GG_OK && initial_courses != NULL) {
        for (size_t i = 0; i < initial_courses->count; i++) {
            ctx->course_repo->delete_course(ctx->course_repo->context, initial_courses->entries[i].id);
        }
        gg_course_list_destroy(initial_courses);
    }

    gboolean is_zero = gtk_check_button_get_active(GTK_CHECK_BUTTON(dlg->zero_check));
    if (!is_zero) {
        const char *tcp_str = gtk_editable_get_text(GTK_EDITABLE(dlg->tcp_entry));
        const char *tcu_str = gtk_editable_get_text(GTK_EDITABLE(dlg->tcu_entry));
        double prior_tcp = tcp_str != NULL ? strtod(tcp_str, NULL) : 0.0;
        uint32_t prior_tcu = tcu_str != NULL ? (uint32_t)strtoul(tcu_str, NULL, 10) : 0;

        GGGradingScale scale;
        memset(&scale, 0, sizeof(scale));
        if (ctx->scale_repo != NULL) {
            ctx->scale_repo->load_scale(ctx->scale_repo->context, &scale);
        }

        if (prior_tcu > 0 && scale.count > 0) {
            double max_achievable = (double)prior_tcu * scale.max_point;
            double min_achievable = (double)prior_tcu * scale.min_point;
            if (prior_tcp > max_achievable) {
                prior_tcp = max_achievable;
            }
            if (prior_tcp < min_achievable) {
                prior_tcp = min_achievable;
            }

            double avg = prior_tcp / (double)prior_tcu;
            size_t idx_a = 0;
            size_t idx_b = scale.count - 1;
            for (size_t i = 0; i + 1 < scale.count; i++) {
                if (scale.items[i].grade_point >= avg && scale.items[i + 1].grade_point <= avg) {
                    idx_a = i;
                    idx_b = i + 1;
                    break;
                }
            }

            double pa = scale.items[idx_a].grade_point;
            double pb = scale.items[idx_b].grade_point;
            uint32_t ua = 0;
            uint32_t ub = 0;

            if (pa > pb) {
                double calc_ua = ((prior_tcp - (double)prior_tcu * pb) / (pa - pb)) + 0.5;
                if (calc_ua < 0.0) {
                    calc_ua = 0.0;
                }
                if (calc_ua > (double)prior_tcu) {
                    calc_ua = (double)prior_tcu;
                }
                ua = (uint32_t)calc_ua;
                ub = prior_tcu - ua;
            } else {
                ua = prior_tcu;
                ub = 0;
            }

            int64_t entry_time = (int64_t)time(NULL);
            if (ua > 0) {
                GGCourseEntry entry_a;
                memset(&entry_a, 0, sizeof(entry_a));
                snprintf(entry_a.semester_label, sizeof(entry_a.semester_label), "Initial Standing");
                snprintf(entry_a.course_label, sizeof(entry_a.course_label), "Prior Transfer Credits");
                entry_a.credit_unit = ua;
                snprintf(entry_a.grade_symbol, sizeof(entry_a.grade_symbol), "%s", scale.items[idx_a].grade_symbol);
                entry_a.entry_date = entry_time;
                int64_t id_out = 0;
                ctx->course_repo->insert_course(ctx->course_repo->context, &entry_a, &id_out);
            }

            if (ub > 0) {
                GGCourseEntry entry_b;
                memset(&entry_b, 0, sizeof(entry_b));
                snprintf(entry_b.semester_label, sizeof(entry_b.semester_label), "Initial Standing");
                snprintf(entry_b.course_label, sizeof(entry_b.course_label), "Prior Transfer Credits");
                entry_b.credit_unit = ub;
                snprintf(entry_b.grade_symbol, sizeof(entry_b.grade_symbol), "%s", scale.items[idx_b].grade_symbol);
                entry_b.entry_date = entry_time;
                int64_t id_out = 0;
                ctx->course_repo->insert_course(ctx->course_repo->context, &entry_b, &id_out);
            }
        }
    }

    gg_backup_create_snapshot(ctx->db_filepath, ctx->backup_dir);
    gg_settings_widget_refresh(dlg->settings_state->container);
    gtk_window_destroy(GTK_WINDOW(dlg->dialog_window));
}

static void on_update_prior_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGSettingsState *state = (GGSettingsState *)user_data;
    if (state == NULL || state->ctx == NULL) {
        return;
    }

    GGPriorStandingDialogState *dlg = (GGPriorStandingDialogState *)calloc(1, sizeof(GGPriorStandingDialogState));
    if (dlg == NULL) {
        return;
    }
    dlg->settings_state = state;

    dlg->dialog_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dlg->dialog_window), "Update Prior Academic Standing");
    gtk_window_set_transient_for(GTK_WINDOW(dlg->dialog_window), state->ctx->main_window);
    gtk_window_set_modal(GTK_WINDOW(dlg->dialog_window), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dlg->dialog_window), 500, 380);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_widget_set_margin_start(vbox, 24);
    gtk_widget_set_margin_end(vbox, 24);

    GtkWidget *dlg_title = gtk_label_new("Update Prior Academic Standing");
    gtk_widget_add_css_class(dlg_title, "title-3");
    gtk_widget_set_halign(dlg_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), dlg_title);

    GtkWidget *dlg_sub = gtk_label_new(
        "Update baseline transfer credits or prior totals. This recalculates your CGPA without affecting semester "
        "course records.");
    gtk_widget_add_css_class(dlg_sub, "card-subtitle");
    gtk_label_set_wrap(GTK_LABEL(dlg_sub), TRUE);
    gtk_widget_set_halign(dlg_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), dlg_sub);

    dlg->zero_check =
        gtk_check_button_new_with_label("Reset prior standing to zero (fresh start / no transfer credits)");
    g_signal_connect(dlg->zero_check, "toggled", G_CALLBACK(on_prior_zero_toggled), dlg);
    gtk_box_append(GTK_BOX(vbox), dlg->zero_check);

    GtkWidget *tcp_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *tcp_lbl = gtk_label_new("Prior Total Credit Points (TCP):");
    gtk_widget_add_css_class(tcp_lbl, "form-label");
    gtk_widget_set_halign(tcp_lbl, GTK_ALIGN_START);
    dlg->tcp_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(dlg->tcp_entry), "e.g. 60.00");
    g_signal_connect(dlg->tcp_entry, "changed", G_CALLBACK(on_prior_dialog_changed), dlg);
    gtk_box_append(GTK_BOX(tcp_box), tcp_lbl);
    gtk_box_append(GTK_BOX(tcp_box), dlg->tcp_entry);
    gtk_box_append(GTK_BOX(vbox), tcp_box);

    GtkWidget *tcu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *tcu_lbl = gtk_label_new("Prior Total Credit Units (TCU):");
    gtk_widget_add_css_class(tcu_lbl, "form-label");
    gtk_widget_set_halign(tcu_lbl, GTK_ALIGN_START);
    dlg->tcu_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(dlg->tcu_entry), "e.g. 15");
    g_signal_connect(dlg->tcu_entry, "changed", G_CALLBACK(on_prior_dialog_changed), dlg);
    gtk_box_append(GTK_BOX(tcu_box), tcu_lbl);
    gtk_box_append(GTK_BOX(tcu_box), dlg->tcu_entry);
    gtk_box_append(GTK_BOX(vbox), tcu_box);

    double cur_tcp = 0.0;
    uint32_t cur_tcu = 0;
    get_current_prior_standing(state, &cur_tcp, &cur_tcu);

    if (cur_tcu > 0) {
        char tcp_buf[32];
        snprintf(tcp_buf, sizeof(tcp_buf), "%.2f", cur_tcp);
        gtk_editable_set_text(GTK_EDITABLE(dlg->tcp_entry), tcp_buf);

        char tcu_buf[32];
        snprintf(tcu_buf, sizeof(tcu_buf), "%u", cur_tcu);
        gtk_editable_set_text(GTK_EDITABLE(dlg->tcu_entry), tcu_buf);
    } else {
        gtk_editable_set_text(GTK_EDITABLE(dlg->tcp_entry), "0.00");
        gtk_editable_set_text(GTK_EDITABLE(dlg->tcu_entry), "0");
        gtk_check_button_set_active(GTK_CHECK_BUTTON(dlg->zero_check), TRUE);
        gtk_widget_set_sensitive(dlg->tcp_entry, FALSE);
        gtk_widget_set_sensitive(dlg->tcu_entry, FALSE);
    }

    dlg->preview_label = gtk_label_new("");
    gtk_widget_add_css_class(dlg->preview_label, "card-subtitle");
    gtk_widget_set_halign(dlg->preview_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), dlg->preview_label);
    on_prior_dialog_changed(NULL, dlg);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 8);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_prior_dialog_cancel), dlg);

    GtkWidget *save_btn = gtk_button_new_with_label("Save Changes");
    gtk_widget_add_css_class(save_btn, "suggested-action");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_prior_standing_submit), dlg);

    gtk_box_append(GTK_BOX(btn_box), cancel_btn);
    gtk_box_append(GTK_BOX(btn_box), save_btn);
    gtk_box_append(GTK_BOX(vbox), btn_box);

    g_object_set_data_full(G_OBJECT(dlg->dialog_window), "dialog_state", dlg, free);
    gtk_window_set_child(GTK_WINDOW(dlg->dialog_window), vbox);
    gtk_window_present(GTK_WINDOW(dlg->dialog_window));
}

static void on_export_xlsx_finish(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GGSettingsState *state = (GGSettingsState *)user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(dialog, res, &error);
    if (file == NULL) {
        if (error != NULL) {
            g_error_free(error);
        }
        return;
    }
    char *path = g_file_get_path(file);
    g_object_unref(file);
    if (path != NULL && state != NULL && state->ctx != NULL) {
        gg_xlsx_export(path, state->ctx->scale_repo, state->ctx->course_repo);
        g_free(path);
    }
}

static void on_export_xlsx_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGSettingsState *state = (GGSettingsState *)user_data;
    if (state == NULL || state->ctx == NULL) {
        return;
    }

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Export Spreadsheet");

    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char default_name[128];
    if (tm_info != NULL) {
        snprintf(default_name, sizeof(default_name), "GradeGoal_Export_%04d%02d%02d.xlsx", tm_info->tm_year + 1900,
                 tm_info->tm_mon + 1, tm_info->tm_mday);
    } else {
        snprintf(default_name, sizeof(default_name), "GradeGoal_Export.xlsx");
    }
    gtk_file_dialog_set_initial_name(dialog, default_name);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Excel Spreadsheets (*.xlsx)");
    gtk_file_filter_add_pattern(filter, "*.xlsx");
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    g_object_unref(filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);

    gtk_file_dialog_save(dialog, state->ctx->main_window, NULL, on_export_xlsx_finish, state);
}

static void on_export_pdf_finish(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GGSettingsState *state = (GGSettingsState *)user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(dialog, res, &error);
    if (file == NULL) {
        if (error != NULL) {
            g_error_free(error);
        }
        return;
    }
    char *path = g_file_get_path(file);
    g_object_unref(file);
    if (path != NULL && state != NULL && state->ctx != NULL) {
        gg_pdf_export(path, state->ctx->scale_repo, state->ctx->course_repo);
        g_free(path);
    }
}

static void on_export_pdf_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGSettingsState *state = (GGSettingsState *)user_data;
    if (state == NULL || state->ctx == NULL) {
        return;
    }

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Export PDF Report");

    time_t now = time(NULL);
    struct tm *tm_info = gmtime(&now);
    char default_name[128];
    if (tm_info != NULL) {
        snprintf(default_name, sizeof(default_name), "GradeGoal_Report_%04d%02d%02d.pdf", tm_info->tm_year + 1900,
                 tm_info->tm_mon + 1, tm_info->tm_mday);
    } else {
        snprintf(default_name, sizeof(default_name), "GradeGoal_Report.pdf");
    }
    gtk_file_dialog_set_initial_name(dialog, default_name);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "PDF Documents (*.pdf)");
    gtk_file_filter_add_pattern(filter, "*.pdf");
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    g_object_unref(filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);

    gtk_file_dialog_save(dialog, state->ctx->main_window, NULL, on_export_pdf_finish, state);
}

static void on_import_close_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWidget *win = GTK_WIDGET(user_data);
    if (win != NULL) {
        gtk_window_destroy(GTK_WINDOW(win));
    }
}

static void show_import_summary_modal(GtkWindow *parent, const GGImportSummary *summary) {
    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "Import Backup Summary");
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);
    if (parent != NULL) {
        gtk_window_set_transient_for(GTK_WINDOW(win), parent);
    }
    gtk_window_set_default_size(GTK_WINDOW(win), 560, 420);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(root, 16);
    gtk_widget_set_margin_bottom(root, 16);
    gtk_widget_set_margin_start(root, 20);
    gtk_widget_set_margin_end(root, 20);
    gtk_window_set_child(GTK_WINDOW(win), root);

    GtkWidget *hdr = gtk_label_new("Spreadsheet Import Results");
    gtk_widget_add_css_class(hdr, "title-2");
    gtk_widget_set_halign(hdr, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(root), hdr);

    char stat_buf[256];
    snprintf(stat_buf, sizeof(stat_buf), "Total Rows: %zu  |  Valid Rows Imported: %zu  |  Rejections: %zu",
             summary->total_rows_read, summary->valid_rows_imported, summary->count);
    GtkWidget *stat_lbl = gtk_label_new(stat_buf);
    gtk_widget_add_css_class(stat_lbl, "card");
    gtk_widget_set_halign(stat_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(root), stat_lbl);

    if (summary->count > 0) {
        GtkWidget *rej_title = gtk_label_new("Itemized Row Rejections:");
        gtk_widget_add_css_class(rej_title, "title-3");
        gtk_widget_set_halign(rej_title, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(root), rej_title);

        GtkWidget *scroll = gtk_scrolled_window_new();
        gtk_widget_set_vexpand(scroll, TRUE);
        GtkWidget *rej_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), rej_box);

        for (size_t i = 0; i < summary->count; i++) {
            char row_str[256];
            snprintf(row_str, sizeof(row_str), "Row %zu: Field '%s' value '%s' — %s", summary->rejections[i].row_number,
                     summary->rejections[i].field_name, summary->rejections[i].rejected_value,
                     summary->rejections[i].reason);
            GtkWidget *rej_lbl = gtk_label_new(row_str);
            gtk_widget_set_halign(rej_lbl, GTK_ALIGN_START);
            gtk_widget_add_css_class(rej_lbl, "error");
            gtk_box_append(GTK_BOX(rej_box), rej_lbl);
        }
        gtk_box_append(GTK_BOX(root), scroll);
    }

    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    gtk_widget_add_css_class(close_btn, "suggested-action");
    gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_import_close_clicked), win);
    gtk_box_append(GTK_BOX(root), close_btn);

    gtk_window_present(GTK_WINDOW(win));
}

static void on_import_backup_finish(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GGSettingsState *state = (GGSettingsState *)user_data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(dialog, res, &error);
    if (file == NULL) {
        if (error != NULL) {
            g_error_free(error);
        }
        return;
    }
    char *path = g_file_get_path(file);
    g_object_unref(file);
    if (path == NULL) {
        return;
    }

    if (state != NULL && state->ctx != NULL && state->ctx->scale_repo != NULL && state->ctx->course_repo != NULL) {
        GGImportSummary *summary = NULL;
        GGStatus sum_st = gg_import_summary_create(&summary);
        if (sum_st == GG_OK && summary != NULL) {
            GGStatus st = gg_xlsx_import(path, state->ctx->scale_repo, state->ctx->course_repo, false, summary);
            if (st == GG_OK) {
                gg_backup_create_snapshot(state->ctx->db_filepath, state->ctx->backup_dir);
                gg_settings_widget_refresh(state->container);
            }
            show_import_summary_modal(state->ctx->main_window, summary);
            gg_import_summary_destroy(summary);
        }
    }
    g_free(path);
}

static void on_import_backup_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGSettingsState *state = (GGSettingsState *)user_data;
    if (state == NULL || state->ctx == NULL) {
        return;
    }

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Import GradeGoal Spreadsheet Backup");

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Excel Spreadsheets (*.xlsx)");
    gtk_file_filter_add_pattern(filter, "*.xlsx");
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    g_object_unref(filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);

    gtk_file_dialog_open(dialog, state->ctx->main_window, NULL, on_import_backup_finish, state);
}

void gg_settings_widget_refresh(GtkWidget *widget) {
    if (widget == NULL) {
        return;
    }
    GGSettingsState *state = (GGSettingsState *)g_object_get_data(G_OBJECT(widget), "state");
    if (state == NULL || state->ctx == NULL) {
        return;
    }

    double prior_tcp = 0.0;
    uint32_t prior_tcu = 0;
    get_current_prior_standing(state, &prior_tcp, &prior_tcu);
    if (state->prior_status_label != NULL) {
        if (prior_tcu > 0) {
            double prior_cgpa = 0.0;
            gg_cgpa_calculate(prior_tcp, prior_tcu, &prior_cgpa);
            char pbuf[160];
            snprintf(pbuf, sizeof(pbuf), "Current Baseline: %.2f TCP  |  %u TCU  |  %.2f Baseline CGPA", prior_tcp,
                     prior_tcu, prior_cgpa);
            gtk_label_set_text(GTK_LABEL(state->prior_status_label), pbuf);
        } else {
            gtk_label_set_text(GTK_LABEL(state->prior_status_label),
                               "Current Baseline: 0.00 TCP  |  0 TCU  (No prior standing recorded)");
        }
    }

    time_t last_backup = 0;
    const char *bdir = state->ctx->backup_dir != NULL ? state->ctx->backup_dir : "backups";
    GGStatus st = gg_backup_get_last_timestamp(bdir, &last_backup);
    if (st == GG_OK && last_backup > 0) {
        struct tm *tm_info = gmtime(&last_backup);
        if (tm_info != NULL) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Last Automatic Backup: %04d-%02d-%02d %02d:%02d:%02d UTC (Retaining 5 FIFO)",
                     tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_hour, tm_info->tm_min,
                     tm_info->tm_sec);
            gtk_label_set_text(GTK_LABEL(state->backup_status_label), buf);
        }
    } else {
        gtk_label_set_text(GTK_LABEL(state->backup_status_label),
                           "Last Automatic Backup: None recorded yet (Triggered on write)");
    }
}

GtkWidget *gg_settings_widget_create(GGAppContext *ctx) {
    GGSettingsState *state = (GGSettingsState *)calloc(1, sizeof(GGSettingsState));
    if (state == NULL) {
        return NULL;
    }
    state->ctx = ctx;

    state->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(state->container, 20);
    gtk_widget_set_margin_bottom(state->container, 20);
    gtk_widget_set_margin_start(state->container, 24);
    gtk_widget_set_margin_end(state->container, 24);

    GtkWidget *title = gtk_label_new("Settings");
    gtk_widget_add_css_class(title, "screen-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(state->container), title);

    GtkWidget *scale_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(scale_card, "card");
    GtkWidget *scale_title = gtk_label_new("Grading Scale Configuration");
    gtk_widget_add_css_class(scale_title, "title-3");
    gtk_widget_set_halign(scale_title, GTK_ALIGN_START);
    GtkWidget *scale_sub =
        gtk_label_new("Define letter grade symbols and their numerical point values (e.g. A = 5.00)");
    gtk_widget_add_css_class(scale_sub, "card-subtitle");
    gtk_widget_set_halign(scale_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(scale_card), scale_title);
    gtk_box_append(GTK_BOX(scale_card), scale_sub);

    GtkWidget *scale_table_hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(scale_table_hdr, "data-table-header");
    GtkWidget *th_sym = gtk_label_new("GRADE SYMBOL");
    gtk_widget_set_halign(th_sym, GTK_ALIGN_START);
    gtk_widget_set_hexpand(th_sym, TRUE);
    GtkWidget *th_pts = gtk_label_new("POINT VALUE");
    gtk_widget_set_halign(th_pts, GTK_ALIGN_START);
    gtk_widget_set_hexpand(th_pts, TRUE);
    GtkWidget *th_act = gtk_label_new("ACTIONS");
    gtk_widget_set_halign(th_act, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(th_act, 80, -1);
    gtk_box_append(GTK_BOX(scale_table_hdr), th_sym);
    gtk_box_append(GTK_BOX(scale_table_hdr), th_pts);
    gtk_box_append(GTK_BOX(scale_table_hdr), th_act);
    gtk_box_append(GTK_BOX(scale_card), scale_table_hdr);

    state->scale_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_append(GTK_BOX(scale_card), state->scale_box);

    if (ctx != NULL && ctx->scale_repo != NULL) {
        ctx->scale_repo->load_scale(ctx->scale_repo->context, &state->original_scale);
        for (size_t i = 0; i < state->original_scale.count; i++) {
            state->rows[i] = gg_grade_row_create(&state->original_scale.items[i], NULL, NULL, state);
            gtk_box_append(GTK_BOX(state->scale_box), state->rows[i]->container);
            state->row_count++;
        }
    }

    state->save_scale_btn = gtk_button_new_with_label("Save Scale Modifications");
    gtk_widget_add_css_class(state->save_scale_btn, "suggested-action");
    gtk_widget_set_halign(state->save_scale_btn, GTK_ALIGN_START);
    g_signal_connect(state->save_scale_btn, "clicked", G_CALLBACK(on_save_scale_clicked), state);
    gtk_box_append(GTK_BOX(scale_card), state->save_scale_btn);
    gtk_box_append(GTK_BOX(state->container), scale_card);

    GtkWidget *prior_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(prior_card, "card");
    GtkWidget *prior_title = gtk_label_new("Prior Academic Standing (Baseline / Transfer)");
    gtk_widget_add_css_class(prior_title, "title-3");
    gtk_widget_set_halign(prior_title, GTK_ALIGN_START);
    GtkWidget *prior_sub = gtk_label_new(
        "Manage baseline Cumulative Grade Points (TCP) and Total Credit Units (TCU) transferred or earned before "
        "GradeGoal");
    gtk_widget_add_css_class(prior_sub, "card-subtitle");
    gtk_widget_set_halign(prior_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(prior_card), prior_title);
    gtk_box_append(GTK_BOX(prior_card), prior_sub);

    state->prior_status_label = gtk_label_new("Loading prior standing...");
    gtk_widget_set_halign(state->prior_status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(prior_card), state->prior_status_label);

    GtkWidget *update_prior_btn = gtk_button_new_with_label("Update Prior Standing");
    gtk_widget_add_css_class(update_prior_btn, "suggested-action");
    gtk_widget_set_halign(update_prior_btn, GTK_ALIGN_START);
    g_signal_connect(update_prior_btn, "clicked", G_CALLBACK(on_update_prior_clicked), state);
    gtk_box_append(GTK_BOX(prior_card), update_prior_btn);
    gtk_box_append(GTK_BOX(state->container), prior_card);

    GtkWidget *export_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(export_card, "card");
    GtkWidget *export_title = gtk_label_new("Data Export & Archive");
    gtk_widget_add_css_class(export_title, "title-3");
    gtk_widget_set_halign(export_title, GTK_ALIGN_START);
    GtkWidget *export_sub =
        gtk_label_new("Generate portable spreadsheet archives or official printable PDF transcripts");
    gtk_widget_add_css_class(export_sub, "card-subtitle");
    gtk_widget_set_halign(export_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(export_card), export_title);
    gtk_box_append(GTK_BOX(export_card), export_sub);

    GtkWidget *export_btns_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *export_xlsx_btn = gtk_button_new_with_label("Export Spreadsheet (.xlsx)");
    g_signal_connect(export_xlsx_btn, "clicked", G_CALLBACK(on_export_xlsx_clicked), state);
    GtkWidget *export_pdf_btn = gtk_button_new_with_label("Export PDF Document (.pdf)");
    g_signal_connect(export_pdf_btn, "clicked", G_CALLBACK(on_export_pdf_clicked), state);
    gtk_box_append(GTK_BOX(export_btns_box), export_xlsx_btn);
    gtk_box_append(GTK_BOX(export_btns_box), export_pdf_btn);
    gtk_box_append(GTK_BOX(export_card), export_btns_box);
    gtk_box_append(GTK_BOX(state->container), export_card);

    GtkWidget *import_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(import_card, "card");
    GtkWidget *import_title = gtk_label_new("Data Import & Restore");
    gtk_widget_add_css_class(import_title, "title-3");
    gtk_widget_set_halign(import_title, GTK_ALIGN_START);
    GtkWidget *import_sub =
        gtk_label_new("Restore course records and grading scales from an existing GradeGoal Excel workbook");
    gtk_widget_add_css_class(import_sub, "card-subtitle");
    gtk_widget_set_halign(import_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(import_card), import_title);
    gtk_box_append(GTK_BOX(import_card), import_sub);

    GtkWidget *import_btn = gtk_button_new_with_label("Import Backup Spreadsheet (.xlsx)");
    gtk_widget_add_css_class(import_btn, "suggested-action");
    gtk_widget_set_halign(import_btn, GTK_ALIGN_START);
    g_signal_connect(import_btn, "clicked", G_CALLBACK(on_import_backup_clicked), state);
    gtk_box_append(GTK_BOX(import_card), import_btn);
    gtk_box_append(GTK_BOX(state->container), import_card);

    GtkWidget *backup_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(backup_card, "card");
    GtkWidget *backup_title = gtk_label_new("Automatic Rolling Backup Status");
    gtk_widget_add_css_class(backup_title, "title-3");
    gtk_widget_set_halign(backup_title, GTK_ALIGN_START);
    GtkWidget *backup_sub = gtk_label_new(
        "Local SQLite snapshots are automatically captured before every mutation (retaining 5 FIFO files)");
    gtk_widget_add_css_class(backup_sub, "card-subtitle");
    gtk_widget_set_halign(backup_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(backup_card), backup_title);
    gtk_box_append(GTK_BOX(backup_card), backup_sub);

    state->backup_status_label = gtk_label_new("Checking backup status...");
    gtk_widget_set_halign(state->backup_status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(backup_card), state->backup_status_label);
    gtk_box_append(GTK_BOX(state->container), backup_card);

    g_object_set_data_full(G_OBJECT(state->container), "state", state, free);

    gg_settings_widget_refresh(state->container);

    return state->container;
}
