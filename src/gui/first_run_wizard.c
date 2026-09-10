#include "first_run_wizard.h"
#include "gg_grade_row.h"
#include "../core/scale_validator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *grade_box;
    GtkWidget *next_button;
    GtkWidget *add_grade_btn;
    GtkWidget *prior_tcp_entry;
    GtkWidget *prior_tcu_entry;
    GtkWidget *zero_check;
    GtkWidget *finish_button;
    GGGradeRow *rows[16];
    size_t row_count;
    GGAppContext *ctx;
    GCallback on_finished;
    gpointer user_data;
} GGWizardState;

static void update_wizard_scale_validation(GGWizardState *state);

static void on_wizard_grade_changed(GtkEditable *editable, gpointer user_data) {
    (void)editable;
    GGWizardState *state = (GGWizardState *)user_data;
    update_wizard_scale_validation(state);
}

static void on_wizard_grade_removed(gpointer data) {
    GGGradeRow *target_row = (GGGradeRow *)data;
    if (target_row == NULL || target_row->container == NULL) {
        return;
    }
    GGWizardState *state = (GGWizardState *)g_object_get_data(G_OBJECT(target_row->container), "wizard_state");
    if (state == NULL || state->row_count <= 2) {
        return;
    }
    size_t target_idx = state->row_count;
    for (size_t i = 0; i < state->row_count; i++) {
        if (state->rows[i] == target_row) {
            target_idx = i;
            break;
        }
    }
    if (target_idx < state->row_count) {
        gtk_box_remove(GTK_BOX(state->grade_box), target_row->container);
        gg_grade_row_destroy(target_row);
        for (size_t i = target_idx; i + 1 < state->row_count; i++) {
            state->rows[i] = state->rows[i + 1];
        }
        state->rows[state->row_count - 1] = NULL;
        state->row_count--;
        update_wizard_scale_validation(state);
    }
}

static void on_wizard_add_grade_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGWizardState *state = (GGWizardState *)user_data;
    if (state == NULL || state->row_count >= 16) {
        return;
    }
    GGGradeItem itm;
    memset(&itm, 0, sizeof(itm));
    itm.grade_symbol[0] = '\0';
    itm.grade_point = 0.0;
    GGGradeRow *new_row = gg_grade_row_create(&itm, G_CALLBACK(on_wizard_grade_changed), G_CALLBACK(on_wizard_grade_removed), state);
    if (new_row != NULL) {
        g_object_set_data(G_OBJECT(new_row->container), "wizard_state", state);
        state->rows[state->row_count] = new_row;
        state->row_count++;
        gtk_box_append(GTK_BOX(state->grade_box), new_row->container);
        update_wizard_scale_validation(state);
    }
}

static void update_wizard_scale_validation(GGWizardState *state) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = state->row_count;
    for (size_t i = 0; i < state->row_count; i++) {
        gg_grade_row_get_item(state->rows[i], &scale.items[i]);
    }
    if (scale.count > 0) {
        scale.max_point = scale.items[0].grade_point;
        scale.min_point = scale.items[scale.count - 1].grade_point;
    }
    GGStatus status = gg_scale_validator_validate_scale(&scale);
    gtk_widget_set_sensitive(state->next_button, status == GG_OK);
}

static void on_wizard_next_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGWizardState *state = (GGWizardState *)user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "page_prior");
}

static void on_wizard_back_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGWizardState *state = (GGWizardState *)user_data;
    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "page_scale");
}

static void on_wizard_zero_toggled(GtkCheckButton *check, gpointer user_data) {
    GGWizardState *state = (GGWizardState *)user_data;
    gboolean active = gtk_check_button_get_active(check);
    if (active) {
        gtk_editable_set_text(GTK_EDITABLE(state->prior_tcp_entry), "0.00");
        gtk_editable_set_text(GTK_EDITABLE(state->prior_tcu_entry), "0");
        gtk_widget_set_sensitive(state->prior_tcp_entry, FALSE);
        gtk_widget_set_sensitive(state->prior_tcu_entry, FALSE);
    } else {
        gtk_widget_set_sensitive(state->prior_tcp_entry, TRUE);
        gtk_widget_set_sensitive(state->prior_tcu_entry, TRUE);
    }
}

static void on_wizard_cancel_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGWizardState *state = (GGWizardState *)user_data;
    gtk_window_destroy(GTK_WINDOW(state->window));
}

static void on_wizard_finish_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGWizardState *state = (GGWizardState *)user_data;

    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = state->row_count;
    for (size_t i = 0; i < state->row_count; i++) {
        gg_grade_row_get_item(state->rows[i], &scale.items[i]);
    }
    if (scale.count > 0) {
        scale.max_point = scale.items[0].grade_point;
        scale.min_point = scale.items[scale.count - 1].grade_point;
    }

    if (state->ctx != NULL && state->ctx->scale_repo != NULL) {
        state->ctx->scale_repo->save_scale(state->ctx->scale_repo->context, &scale);
    }

    gboolean is_zero = gtk_check_button_get_active(GTK_CHECK_BUTTON(state->zero_check));
    if (!is_zero && state->ctx != NULL && state->ctx->course_repo != NULL) {
        const char *tcp_str = gtk_editable_get_text(GTK_EDITABLE(state->prior_tcp_entry));
        const char *tcu_str = gtk_editable_get_text(GTK_EDITABLE(state->prior_tcu_entry));
        double prior_tcp = tcp_str != NULL ? strtod(tcp_str, NULL) : 0.0;
        uint32_t prior_tcu = tcu_str != NULL ? (uint32_t)strtoul(tcu_str, NULL, 10) : 0;

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
                state->ctx->course_repo->insert_course(state->ctx->course_repo->context, &entry_a, &id_out);
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
                state->ctx->course_repo->insert_course(state->ctx->course_repo->context, &entry_b, &id_out);
            }
        }
    }

    if (state->on_finished != NULL) {
        ((void (*)(gpointer))state->on_finished)(state->user_data);
    }

    gtk_window_destroy(GTK_WINDOW(state->window));
}

GtkWidget *gg_first_run_wizard_create(GtkWindow *parent, GGAppContext *ctx, GCallback on_finished, gpointer user_data) {
    GGWizardState *state = (GGWizardState *)calloc(1, sizeof(GGWizardState));
    if (state == NULL) {
        return NULL;
    }
    state->ctx = ctx;
    state->on_finished = on_finished;
    state->user_data = user_data;

    state->window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(state->window), "GradeGoal Setup Wizard");
    gtk_window_set_modal(GTK_WINDOW(state->window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(state->window), parent);
    gtk_window_set_default_size(GTK_WINDOW(state->window), 520, 520);

    GtkWidget *root_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(root_box, 16);
    gtk_widget_set_margin_bottom(root_box, 16);
    gtk_widget_set_margin_start(root_box, 20);
    gtk_widget_set_margin_end(root_box, 20);
    gtk_window_set_child(GTK_WINDOW(state->window), root_box);

    state->stack = gtk_stack_new();
    gtk_box_append(GTK_BOX(root_box), state->stack);
    gtk_widget_set_vexpand(state->stack, TRUE);

    GtkWidget *page_scale = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *scale_title = gtk_label_new("Step 1: Configure Grading Scale");
    gtk_widget_add_css_class(scale_title, "title-2");
    gtk_widget_set_halign(scale_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page_scale), scale_title);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    state->grade_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), state->grade_box);
    gtk_box_append(GTK_BOX(page_scale), scroll);

    const char *default_symbols[] = { "A", "B", "C", "D", "E", "F" };
    const double default_points[] = { 5.0, 4.0, 3.0, 2.0, 1.0, 0.0 };
    for (size_t i = 0; i < 6; i++) {
        GGGradeItem itm;
        snprintf(itm.grade_symbol, sizeof(itm.grade_symbol), "%s", default_symbols[i]);
        itm.grade_point = default_points[i];
        state->rows[i] = gg_grade_row_create(&itm, G_CALLBACK(on_wizard_grade_changed), G_CALLBACK(on_wizard_grade_removed), state);
        g_object_set_data(G_OBJECT(state->rows[i]->container), "wizard_state", state);
        gtk_box_append(GTK_BOX(state->grade_box), state->rows[i]->container);
    }
    state->row_count = 6;

    GtkWidget *scale_action_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    state->add_grade_btn = gtk_button_new_with_label("+ Add Grade");
    g_signal_connect(state->add_grade_btn, "clicked", G_CALLBACK(on_wizard_add_grade_clicked), state);
    gtk_box_append(GTK_BOX(scale_action_bar), state->add_grade_btn);

    GtkWidget *scale_buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(scale_buttons_box, GTK_ALIGN_END);
    gtk_widget_set_hexpand(scale_buttons_box, TRUE);
    GtkWidget *cancel_btn1 = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_btn1, "clicked", G_CALLBACK(on_wizard_cancel_clicked), state);
    state->next_button = gtk_button_new_with_label("Next");
    gtk_widget_add_css_class(state->next_button, "suggested-action");
    g_signal_connect(state->next_button, "clicked", G_CALLBACK(on_wizard_next_clicked), state);
    gtk_box_append(GTK_BOX(scale_buttons_box), cancel_btn1);
    gtk_box_append(GTK_BOX(scale_buttons_box), state->next_button);

    gtk_box_append(GTK_BOX(scale_action_bar), scale_buttons_box);
    gtk_box_append(GTK_BOX(page_scale), scale_action_bar);

    GtkWidget *page_prior = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *prior_title = gtk_label_new("Step 2: Enter Prior Standing");
    gtk_widget_add_css_class(prior_title, "title-2");
    gtk_widget_set_halign(prior_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(page_prior), prior_title);

    GtkWidget *tcp_label = gtk_label_new("Prior Total Credit Points (TCP):");
    gtk_widget_set_halign(tcp_label, GTK_ALIGN_START);
    state->prior_tcp_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(state->prior_tcp_entry), "0.00");
    gtk_box_append(GTK_BOX(page_prior), tcp_label);
    gtk_box_append(GTK_BOX(page_prior), state->prior_tcp_entry);

    GtkWidget *tcu_label = gtk_label_new("Prior Total Credit Units (TCU):");
    gtk_widget_set_halign(tcu_label, GTK_ALIGN_START);
    state->prior_tcu_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(state->prior_tcu_entry), "0");
    gtk_box_append(GTK_BOX(page_prior), tcu_label);
    gtk_box_append(GTK_BOX(page_prior), state->prior_tcu_entry);

    state->zero_check = gtk_check_button_new_with_label("Start from zero (no prior history)");
    g_signal_connect(state->zero_check, "toggled", G_CALLBACK(on_wizard_zero_toggled), state);
    gtk_box_append(GTK_BOX(page_prior), state->zero_check);

    GtkWidget *prior_buttons_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(prior_buttons_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(prior_buttons_box, 16);
    GtkWidget *back_btn = gtk_button_new_with_label("Back");
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_wizard_back_clicked), state);
    state->finish_button = gtk_button_new_with_label("Finish");
    gtk_widget_add_css_class(state->finish_button, "suggested-action");
    g_signal_connect(state->finish_button, "clicked", G_CALLBACK(on_wizard_finish_clicked), state);
    gtk_box_append(GTK_BOX(prior_buttons_box), back_btn);
    gtk_box_append(GTK_BOX(prior_buttons_box), state->finish_button);
    gtk_box_append(GTK_BOX(page_prior), prior_buttons_box);

    gtk_stack_add_named(GTK_STACK(state->stack), page_scale, "page_scale");
    gtk_stack_add_named(GTK_STACK(state->stack), page_prior, "page_prior");

    update_wizard_scale_validation(state);

    return state->window;
}
