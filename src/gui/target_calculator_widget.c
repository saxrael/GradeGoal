#include "target_calculator_widget.h"
#include "../core/target_solver.h"
#include "gg_course_entry_row.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    GGAppContext *ctx;
    GtkWidget *container;
    GtkWidget *target_entry;
    GtkWidget *courses_box;
    GtkWidget *result_card;
    GtkWidget *result_title_label;
    GtkWidget *result_detail_label;
    GtkWidget *assignments_box;
    GGCourseEntryRow *rows[32];
    size_t row_count;
} GGTargetCalculatorState;

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

static void on_remove_upcoming_row(gpointer data) {
    GGCourseEntryRow *target_row = (GGCourseEntryRow *)data;
    if (target_row == NULL || target_row->container == NULL) {
        return;
    }
    GGTargetCalculatorState *state =
        (GGTargetCalculatorState *)g_object_get_data(G_OBJECT(target_row->container), "calculator_state");
    if (state == NULL || state->row_count <= 1) {
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
        gtk_box_remove(GTK_BOX(state->courses_box), target_row->container);
        gg_course_entry_row_destroy(target_row);
        for (size_t i = target_idx; i + 1 < state->row_count; i++) {
            state->rows[i] = state->rows[i + 1];
        }
        state->rows[state->row_count - 1] = NULL;
        state->row_count--;
    }
}

static void on_add_upcoming_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGTargetCalculatorState *state = (GGTargetCalculatorState *)user_data;
    if (state->row_count >= 32) {
        return;
    }

    GGCourseEntry initial;
    memset(&initial, 0, sizeof(initial));
    initial.credit_unit = 3;

    GGCourseEntryRow *row =
        gg_course_entry_row_create(NULL, &initial, false, NULL, G_CALLBACK(on_remove_upcoming_row), state);
    if (row != NULL) {
        g_object_set_data(G_OBJECT(row->container), "calculator_state", state);
        state->rows[state->row_count] = row;
        state->row_count++;
        gtk_box_append(GTK_BOX(state->courses_box), row->container);
    }
}

static void on_calculate_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GGTargetCalculatorState *state = (GGTargetCalculatorState *)user_data;
    if (state == NULL || state->ctx == NULL || state->ctx->scale_repo == NULL || state->ctx->course_repo == NULL) {
        return;
    }

    const char *target_str = gtk_editable_get_text(GTK_EDITABLE(state->target_entry));
    if (target_str == NULL || target_str[0] == '\0') {
        return;
    }
    double target_cgpa = strtod(target_str, NULL);

    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    GGStatus status = state->ctx->scale_repo->load_scale(state->ctx->scale_repo->context, &scale);
    if (status != GG_OK) {
        return;
    }

    double tcp = 0.0;
    uint32_t tcu = 0;
    status = state->ctx->course_repo->get_live_totals(state->ctx->course_repo->context, &tcp, &tcu);
    if (status != GG_OK) {
        return;
    }

    GGUpcomingCourse upcoming[32];
    size_t valid_upcoming_count = 0;

    for (size_t i = 0; i < state->row_count; i++) {
        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        gg_course_entry_row_get_entry(state->rows[i], &entry);
        if (entry.credit_unit > 0) {
            snprintf(upcoming[valid_upcoming_count].course_label, sizeof(upcoming[valid_upcoming_count].course_label),
                     "%s", entry.course_label);
            upcoming[valid_upcoming_count].credit_unit = entry.credit_unit;
            valid_upcoming_count++;
        }
    }

    if (valid_upcoming_count == 0) {
        return;
    }

    GGSolverResult result;
    memset(&result, 0, sizeof(result));
    status = gg_target_solver_solve(&scale, tcp, tcu, target_cgpa, upcoming, valid_upcoming_count, &result);
    if (status != GG_OK) {
        return;
    }

    GtkWidget *child = gtk_widget_get_first_child(state->assignments_box);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(state->assignments_box), child);
        child = next;
    }

    gtk_widget_remove_css_class(state->result_card, "accent-success");
    gtk_widget_remove_css_class(state->result_card, "accent-warning");
    gtk_widget_remove_css_class(state->result_card, "accent-info");

    if (result.branch == GG_SOLVER_UNREACHABLE) {
        gtk_widget_add_css_class(state->result_card, "accent-warning");
        gtk_label_set_text(GTK_LABEL(state->result_title_label), "Target Not Reachable");
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "The target CGPA of %.2f cannot be reached with the entered courses.\nMaximum achievable CGPA: %.2f",
                 target_cgpa, result.max_achievable_cgpa);
        gtk_label_set_text(GTK_LABEL(state->result_detail_label), detail);
    } else if (result.branch == GG_SOLVER_ALREADY_GUARANTEED) {
        gtk_widget_add_css_class(state->result_card, "accent-success");
        gtk_label_set_text(GTK_LABEL(state->result_title_label), "Target Already Guaranteed");

        char detail[128];
        snprintf(detail, sizeof(detail),
                 "Your target CGPA of %.2f is already assured regardless of upcoming grades!\nFloor outcome (all "
                 "lowest grades) shown below.",
                 target_cgpa);
        gtk_label_set_text(GTK_LABEL(state->result_detail_label), detail);

        for (size_t i = 0; i < result.assignment_count; i++) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_margin_top(row, 2);
            gtk_widget_set_margin_bottom(row, 2);

            GtkWidget *lbl_course = gtk_label_new(
                result.assignments[i].course_label[0] != '\0' ? result.assignments[i].course_label : "Upcoming Course");
            gtk_widget_set_halign(lbl_course, GTK_ALIGN_START);
            gtk_widget_set_hexpand(lbl_course, TRUE);

            char u_buf[32];
            snprintf(u_buf, sizeof(u_buf), "%u units", result.assignments[i].credit_unit);
            GtkWidget *lbl_u = gtk_label_new(u_buf);
            gtk_widget_add_css_class(lbl_u, "dim-label");

            char pt_buf[32];
            snprintf(pt_buf, sizeof(pt_buf), "%.1f pts", result.assignments[i].grade_point);
            GtkWidget *lbl_pt = gtk_label_new(pt_buf);
            gtk_widget_add_css_class(lbl_pt, "dim-label");

            GtkWidget *lbl_grd = gtk_label_new(result.assignments[i].assigned_grade);
            gtk_widget_add_css_class(lbl_grd, get_grade_badge_class(result.assignments[i].assigned_grade));

            gtk_box_append(GTK_BOX(row), lbl_course);
            gtk_box_append(GTK_BOX(row), lbl_u);
            gtk_box_append(GTK_BOX(row), lbl_pt);
            gtk_box_append(GTK_BOX(row), lbl_grd);

            gtk_box_append(GTK_BOX(state->assignments_box), row);
        }
    } else if (result.branch == GG_SOLVER_REACHABLE_WITH_EFFORT) {
        gtk_widget_add_css_class(state->result_card, "accent-info");
        gtk_label_set_text(GTK_LABEL(state->result_title_label), "Target Reachable with Effort");
        char detail[128];
        snprintf(detail, sizeof(detail),
                 "Minimum required average grade point: %.2f\nRequired grade distribution across upcoming courses:",
                 result.min_required_average);
        gtk_label_set_text(GTK_LABEL(state->result_detail_label), detail);

        for (size_t i = 0; i < result.assignment_count; i++) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_margin_top(row, 2);
            gtk_widget_set_margin_bottom(row, 2);

            GtkWidget *lbl_course = gtk_label_new(
                result.assignments[i].course_label[0] != '\0' ? result.assignments[i].course_label : "Upcoming Course");
            gtk_widget_set_halign(lbl_course, GTK_ALIGN_START);
            gtk_widget_set_hexpand(lbl_course, TRUE);

            char u_buf[32];
            snprintf(u_buf, sizeof(u_buf), "%u units", result.assignments[i].credit_unit);
            GtkWidget *lbl_u = gtk_label_new(u_buf);
            gtk_widget_add_css_class(lbl_u, "dim-label");

            char pt_buf[32];
            snprintf(pt_buf, sizeof(pt_buf), "%.1f pts", result.assignments[i].grade_point);
            GtkWidget *lbl_pt = gtk_label_new(pt_buf);
            gtk_widget_add_css_class(lbl_pt, "dim-label");

            GtkWidget *lbl_grd = gtk_label_new(result.assignments[i].assigned_grade);
            gtk_widget_add_css_class(lbl_grd, get_grade_badge_class(result.assignments[i].assigned_grade));

            gtk_box_append(GTK_BOX(row), lbl_course);
            gtk_box_append(GTK_BOX(row), lbl_u);
            gtk_box_append(GTK_BOX(row), lbl_pt);
            gtk_box_append(GTK_BOX(row), lbl_grd);

            gtk_box_append(GTK_BOX(state->assignments_box), row);
        }
    }

    gtk_widget_set_visible(state->result_card, TRUE);
}

GtkWidget *gg_target_calculator_widget_create(GGAppContext *ctx) {
    GGTargetCalculatorState *state = (GGTargetCalculatorState *)calloc(1, sizeof(GGTargetCalculatorState));
    if (state == NULL) {
        return NULL;
    }
    state->ctx = ctx;

    state->container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_top(state->container, 20);
    gtk_widget_set_margin_bottom(state->container, 20);
    gtk_widget_set_margin_start(state->container, 24);
    gtk_widget_set_margin_end(state->container, 24);

    GtkWidget *title = gtk_label_new("Target Calculator");
    gtk_widget_add_css_class(title, "screen-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(state->container), title);

    GtkWidget *target_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(target_card, "card");

    GtkWidget *target_header = gtk_label_new("Target CGPA Goal");
    gtk_widget_add_css_class(target_header, "title-3");
    gtk_widget_set_halign(target_header, GTK_ALIGN_START);

    GtkWidget *target_subtitle = gtk_label_new("Enter your desired cumulative grade point average (Scale 5.00 Max)");
    gtk_widget_add_css_class(target_subtitle, "card-subtitle");
    gtk_widget_set_halign(target_subtitle, GTK_ALIGN_START);

    GtkWidget *target_input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(target_input_row, 4);

    GtkWidget *target_field_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *target_lbl = gtk_label_new("Target CGPA");
    gtk_widget_add_css_class(target_lbl, "form-label");
    gtk_widget_set_halign(target_lbl, GTK_ALIGN_START);

    state->target_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->target_entry), "e.g. 4.20");
    gtk_widget_set_size_request(state->target_entry, 160, -1);

    gtk_box_append(GTK_BOX(target_field_box), target_lbl);
    gtk_box_append(GTK_BOX(target_field_box), state->target_entry);

    GtkWidget *calc_btn = gtk_button_new_with_label("Calculate Required Grades");
    gtk_widget_add_css_class(calc_btn, "suggested-action");
    gtk_widget_set_valign(calc_btn, GTK_ALIGN_END);
    g_signal_connect(calc_btn, "clicked", G_CALLBACK(on_calculate_clicked), state);

    gtk_box_append(GTK_BOX(target_input_row), target_field_box);
    gtk_box_append(GTK_BOX(target_input_row), calc_btn);

    gtk_box_append(GTK_BOX(target_card), target_header);
    gtk_box_append(GTK_BOX(target_card), target_subtitle);
    gtk_box_append(GTK_BOX(target_card), target_input_row);
    gtk_box_append(GTK_BOX(state->container), target_card);

    GtkWidget *courses_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(courses_card, "card");

    GtkWidget *section_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *courses_title_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(courses_title_box, TRUE);

    GtkWidget *courses_title = gtk_label_new("Upcoming Courses to Take");
    gtk_widget_add_css_class(courses_title, "title-3");
    gtk_widget_set_halign(courses_title, GTK_ALIGN_START);

    GtkWidget *courses_sub =
        gtk_label_new("Add the courses and credit units you plan to register for in upcoming semesters");
    gtk_widget_add_css_class(courses_sub, "card-subtitle");
    gtk_widget_set_halign(courses_sub, GTK_ALIGN_START);

    gtk_box_append(GTK_BOX(courses_title_box), courses_title);
    gtk_box_append(GTK_BOX(courses_title_box), courses_sub);

    GtkWidget *add_btn = gtk_button_new_with_label("+ Add Course");
    gtk_widget_set_valign(add_btn, GTK_ALIGN_START);
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_upcoming_clicked), state);

    gtk_box_append(GTK_BOX(section_header), courses_title_box);
    gtk_box_append(GTK_BOX(section_header), add_btn);
    gtk_box_append(GTK_BOX(courses_card), section_header);

    GtkWidget *table_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(table_header, "data-table-header");

    GtkWidget *th_code = gtk_label_new("COURSE CODE / LABEL");
    gtk_widget_set_halign(th_code, GTK_ALIGN_START);
    gtk_widget_set_hexpand(th_code, TRUE);

    GtkWidget *th_units = gtk_label_new("UNITS");
    gtk_widget_set_halign(th_units, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(th_units, 100, -1);

    GtkWidget *th_act = gtk_label_new("ACTIONS");
    gtk_widget_set_halign(th_act, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(th_act, 80, -1);

    gtk_box_append(GTK_BOX(table_header), th_code);
    gtk_box_append(GTK_BOX(table_header), th_units);
    gtk_box_append(GTK_BOX(table_header), th_act);
    gtk_box_append(GTK_BOX(courses_card), table_header);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(scroll, -1, 180);
    state->courses_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), state->courses_box);
    gtk_box_append(GTK_BOX(courses_card), scroll);

    for (int i = 0; i < 3; i++) {
        GGCourseEntry initial;
        memset(&initial, 0, sizeof(initial));
        initial.credit_unit = 3;
        state->rows[state->row_count] =
            gg_course_entry_row_create(NULL, &initial, false, NULL, G_CALLBACK(on_remove_upcoming_row), state);
        g_object_set_data(G_OBJECT(state->rows[state->row_count]->container), "calculator_state", state);
        gtk_box_append(GTK_BOX(state->courses_box), state->rows[state->row_count]->container);
        state->row_count++;
    }

    gtk_box_append(GTK_BOX(state->container), courses_card);

    state->result_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(state->result_card, "card");
    gtk_widget_set_visible(state->result_card, FALSE);

    state->result_title_label = gtk_label_new("");
    gtk_widget_add_css_class(state->result_title_label, "title-3");
    gtk_widget_set_halign(state->result_title_label, GTK_ALIGN_START);

    state->result_detail_label = gtk_label_new("");
    gtk_widget_add_css_class(state->result_detail_label, "card-subtitle");
    gtk_widget_set_halign(state->result_detail_label, GTK_ALIGN_START);

    state->assignments_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(state->assignments_box, 8);

    gtk_box_append(GTK_BOX(state->result_card), state->result_title_label);
    gtk_box_append(GTK_BOX(state->result_card), state->result_detail_label);
    gtk_box_append(GTK_BOX(state->result_card), state->assignments_box);

    gtk_box_append(GTK_BOX(state->container), state->result_card);

    g_object_set_data_full(G_OBJECT(state->container), "state", state, free);

    return state->container;
}
