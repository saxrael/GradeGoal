#ifndef GG_TARGET_SOLVER_H
#define GG_TARGET_SOLVER_H

#include "gg_types.h"

typedef struct {
    char course_label[32];
    uint32_t credit_unit;
} GGUpcomingCourse;

typedef enum {
    GG_SOLVER_UNREACHABLE = 1,
    GG_SOLVER_ALREADY_GUARANTEED = 2,
    GG_SOLVER_REACHABLE_WITH_EFFORT = 3
} GGSolverBranch;

typedef struct {
    char course_label[32];
    uint32_t credit_unit;
    char assigned_grade[8];
    double grade_point;
} GGCourseAssignment;

typedef struct {
    GGSolverBranch branch;
    double max_achievable_cgpa;
    double min_required_average;
    GGCourseAssignment assignments[32];
    size_t assignment_count;
} GGSolverResult;

GGStatus gg_target_solver_solve(
    const GGGradingScale *scale,
    double current_tcp,
    uint32_t current_tcu,
    double target_cgpa,
    const GGUpcomingCourse *upcoming,
    size_t upcoming_count,
    GGSolverResult *out_result
);

#endif
