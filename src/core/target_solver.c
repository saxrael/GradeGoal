#include "target_solver.h"
#include <string.h>

static void gg_copy_string(char *dest, size_t dest_size, const char *src) {
    if (dest == NULL || dest_size == 0) {
        return;
    }
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }
    size_t i = 0;
    while (i + 1 < dest_size && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

typedef struct {
    char course_label[32];
    uint32_t credit_unit;
    size_t grade_index;
} GGSolverWorkingItem;

GGStatus gg_target_solver_solve(
    const GGGradingScale *scale,
    double current_tcp,
    uint32_t current_tcu,
    double target_cgpa,
    const GGUpcomingCourse *upcoming,
    size_t upcoming_count,
    GGSolverResult *out_result
) {
    if (scale == NULL || out_result == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    if (scale->count == 0 || scale->count > 16) {
        return GG_ERR_INVALID_ARG;
    }

    if (current_tcp < 0.0 || target_cgpa <= 0.0) {
        return GG_ERR_INVALID_ARG;
    }

    if (upcoming_count > 32) {
        return GG_ERR_INVALID_ARG;
    }

    if (upcoming_count > 0 && upcoming == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    uint32_t delta_tcu = 0;
    for (size_t i = 0; i < upcoming_count; i++) {
        if (upcoming[i].credit_unit == 0) {
            return GG_ERR_INVALID_ARG;
        }
        if (memchr(upcoming[i].course_label, '\0', sizeof(upcoming[i].course_label)) == NULL) {
            return GG_ERR_INVALID_ARG;
        }
        delta_tcu += upcoming[i].credit_unit;
    }

    uint32_t total_projected_tcu = current_tcu + delta_tcu;
    if (total_projected_tcu == 0) {
        return GG_ERR_INVALID_ARG;
    }

    memset(out_result, 0, sizeof(GGSolverResult));

    double required_points = target_cgpa * (double)total_projected_tcu - current_tcp;
    double max_achievable_points = (double)delta_tcu * scale->max_point;
    double min_achievable_points = (double)delta_tcu * scale->min_point;
    double max_achievable_cgpa = (current_tcp + max_achievable_points) / (double)total_projected_tcu;

    const double epsilon = 1e-9;

    if (required_points > max_achievable_points + epsilon) {
        out_result->branch = GG_SOLVER_UNREACHABLE;
        out_result->max_achievable_cgpa = max_achievable_cgpa;
        out_result->min_required_average = 0.0;
        out_result->assignment_count = 0;
        return GG_OK;
    }

    if (required_points <= min_achievable_points + epsilon) {
        out_result->branch = GG_SOLVER_ALREADY_GUARANTEED;
        out_result->max_achievable_cgpa = max_achievable_cgpa;
        out_result->min_required_average = 0.0;
        out_result->assignment_count = upcoming_count;
        for (size_t i = 0; i < upcoming_count; i++) {
            out_result->assignments[i].credit_unit = upcoming[i].credit_unit;
            gg_copy_string(out_result->assignments[i].course_label, sizeof(out_result->assignments[i].course_label), upcoming[i].course_label);
            gg_copy_string(out_result->assignments[i].assigned_grade, sizeof(out_result->assignments[i].assigned_grade), scale->items[scale->count - 1].grade_symbol);
            out_result->assignments[i].grade_point = scale->min_point;
        }
        return GG_OK;
    }

    out_result->branch = GG_SOLVER_REACHABLE_WITH_EFFORT;
    out_result->max_achievable_cgpa = max_achievable_cgpa;
    out_result->min_required_average = (delta_tcu > 0) ? (required_points / (double)delta_tcu) : 0.0;
    out_result->assignment_count = upcoming_count;

    GGSolverWorkingItem working[32];
    for (size_t i = 0; i < upcoming_count; i++) {
        working[i].credit_unit = upcoming[i].credit_unit;
        gg_copy_string(working[i].course_label, sizeof(working[i].course_label), upcoming[i].course_label);
        working[i].grade_index = scale->count - 1;
    }

    for (size_t i = 1; i < upcoming_count; i++) {
        GGSolverWorkingItem key = working[i];
        size_t j = i;
        while (j > 0 && working[j - 1].credit_unit < key.credit_unit) {
            working[j] = working[j - 1];
            j--;
        }
        working[j] = key;
    }

    double current_points = (double)delta_tcu * scale->min_point;
    while (current_points < required_points - epsilon) {
        size_t target_idx = upcoming_count;
        for (size_t i = 0; i < upcoming_count; i++) {
            if (working[i].grade_index > 0) {
                target_idx = i;
                break;
            }
        }

        if (target_idx >= upcoming_count) {
            break;
        }

        size_t cur_k = working[target_idx].grade_index;
        double gain = (double)working[target_idx].credit_unit * (scale->items[cur_k - 1].grade_point - scale->items[cur_k].grade_point);
        working[target_idx].grade_index = cur_k - 1;
        current_points += gain;
    }

    for (size_t i = 0; i < upcoming_count; i++) {
        size_t k = working[i].grade_index;
        out_result->assignments[i].credit_unit = working[i].credit_unit;
        gg_copy_string(out_result->assignments[i].course_label, sizeof(out_result->assignments[i].course_label), working[i].course_label);
        gg_copy_string(out_result->assignments[i].assigned_grade, sizeof(out_result->assignments[i].assigned_grade), scale->items[k].grade_symbol);
        out_result->assignments[i].grade_point = scale->items[k].grade_point;
    }

    return GG_OK;
}
