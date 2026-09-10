#include "target_calculator_widget.h"
#include "gg_course_entry_row.h"
#include "../core/target_solver.h"
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

static void on_remove_upcoming_row(gpointer data) {
    GGCourseEntryRow *target_row = (GGCourseEntryRow *)data;
    if (target_row == NULL || target_row->container == NULL) {
        return;
    }
    GGTargetCalculatorState *state = (GGTargetCalculatorState *)g_object_get_data(G_OBJECT(target_row->container), "calculator_state");
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

    GGCourseEntryRow *row = gg_course_entry_row_create(NULL, &initial, false, NULL, G_CALLBACK(on_remove_upcoming_row), state);
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
            snprintf(upcoming[valid_upcoming_count].course_label, sizeof(upcoming[valid_upcoming_count].course_label), "%s", entry.course_label);
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
        snprintf(detail, sizeof(detail), "The target CGPA of %.2f cannot be reached with the entered courses.\nMaximum achievable CGPA: %.2f", target_cgpa, result.max_achievable_cgpa);
        gtk_label_set_text(GTK_LABEL(state->result_detail_label), detail);
    } else if (result.branch == GG_SOLVER_ALREADY_GUARANTEED) {
        gtk_widget_add_css_class(state->result_card, "accent-success");
        gtk_label_set_text(GTK_LABEL(state->result_title_label), "Target Already Guaranteed");
        char detail[128];
        snprintf(detail, sizeof(detail), "Your target CGPA of %.2f is already assured regardless of upcoming grades!\nFloor outcome (all lowest grades) shown below.", target_cgpa);
        gtk_label_set_text(GTK_LABEL(state->result_detail_label), detail);

        for (size_t i = 0; i < result.assignment_count; i++) {
            char row_str[128];
            snprintf(row_str, sizeof(row_str), "• %s (%u units)  —  Assigned Grade: %s (%.1f)",
                     result.assignments[i].course_label[0] != '\0' ? result.assignments[i].course_label : "Course",
                     result.assignments[i].credit_unit,
                     result.assignments[i].assigned_grade,
                     result.assignments[i].grade_point);
            GtkWidget *lbl = gtk_label_new(row_str);
            gtk_widget_set_halign(lbl, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(state->assignments_box), lbl);
        }
    } else if (result.branch == GG_SOLVER_REACHABLE_WITH_EFFORT) {
        gtk_widget_add_css_class(state->result_card, "accent-info");
        gtk_label_set_text(GTK_LABEL(state->result_title_label), "Target Reachable with Effort");
        char detail[128];
        snprintf(detail, sizeof(detail), "Minimum required average grade point: %.2f\nRequired grade distribution across upcoming courses:", result.min_required_average);
        gtk_label_set_text(GTK_LABEL(state->result_detail_label), detail);

        for (size_t i = 0; i < result.assignment_count; i++) {
            char row_str[128];
            snprintf(row_str, sizeof(row_str), "• %s (%u units)  —  Needed Grade: %s (%.1f)",
                     result.assignments[i].course_label[0] != '\0' ? result.assignments[i].course_label : "Course",
                     result.assignments[i].credit_unit,
                     result.assignments[i].assigned_grade,
                     result.assignments[i].grade_point);
            GtkWidget *lbl = gtk_label_new(row_str);
            gtk_widget_set_halign(lbl, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(state->assignments_box), lbl);
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

    GtkWidget *target_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(target_box, "card");

    GtkWidget *target_prompt = gtk_label_new("Target CGPA:");
    state->target_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->target_entry), "e.g. 4.20");
    gtk_widget_set_size_request(state->target_entry, 120, -1);

    GtkWidget *calc_btn = gtk_button_new_with_label("Calculate Required Grades");
    gtk_widget_add_css_class(calc_btn, "suggested-action");
    g_signal_connect(calc_btn, "clicked", G_CALLBACK(on_calculate_clicked), state);

    gtk_box_append(GTK_BOX(target_box), target_prompt);
    gtk_box_append(GTK_BOX(target_box), state->target_entry);
    gtk_box_append(GTK_BOX(target_box), calc_btn);
    gtk_box_append(GTK_BOX(state->container), target_box);

    GtkWidget *section_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *courses_title = gtk_label_new("Upcoming Courses to Take");
    gtk_widget_add_css_class(courses_title, "title-3");
    gtk_widget_set_halign(courses_title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(courses_title, TRUE);

    GtkWidget *add_btn = gtk_button_new_with_label("+ Add Course");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_upcoming_clicked), state);

    gtk_box_append(GTK_BOX(section_header), courses_title);
    gtk_box_append(GTK_BOX(section_header), add_btn);
    gtk_box_append(GTK_BOX(state->container), section_header);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(scroll, -1, 180);
    state->courses_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), state->courses_box);
    gtk_box_append(GTK_BOX(state->container), scroll);

    for (int i = 0; i < 3; i++) {
        GGCourseEntry initial;
        memset(&initial, 0, sizeof(initial));
        initial.credit_unit = 3;
        state->rows[state->row_count] = gg_course_entry_row_create(NULL, &initial, false, NULL, G_CALLBACK(on_remove_upcoming_row), state);
        g_object_set_data(G_OBJECT(state->rows[state->row_count]->container), "calculator_state", state);
        gtk_box_append(GTK_BOX(state->courses_box), state->rows[state->row_count]->container);
        state->row_count++;
    }

    state->result_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(state->result_card, "card");
    gtk_widget_set_visible(state->result_card, FALSE);

    state->result_title_label = gtk_label_new("");
    gtk_widget_add_css_class(state->result_title_label, "title-3");
    gtk_widget_set_halign(state->result_title_label, GTK_ALIGN_START);

    state->result_detail_label = gtk_label_new("");
    gtk_widget_set_halign(state->result_detail_label, GTK_ALIGN_START);

    state->assignments_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(state->assignments_box, 16);

    gtk_box_append(GTK_BOX(state->result_card), state->result_title_label);
    gtk_box_append(GTK_BOX(state->result_card), state->result_detail_label);
    gtk_box_append(GTK_BOX(state->result_card), state->assignments_box);

    gtk_box_append(GTK_BOX(state->container), state->result_card);

    g_object_set_data_full(G_OBJECT(state->container), "state", state, free);

    return state->container;
}
