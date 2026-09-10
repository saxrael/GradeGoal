#include "unity.h"
#include "target_solver.h"
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

static GGGradingScale create_standard_scale(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 6;
    scale.max_point = 5.0;
    scale.min_point = 0.0;

    strncpy(scale.items[0].grade_symbol, "A", sizeof(scale.items[0].grade_symbol) - 1);
    scale.items[0].grade_point = 5.0;

    strncpy(scale.items[1].grade_symbol, "B", sizeof(scale.items[1].grade_symbol) - 1);
    scale.items[1].grade_point = 4.0;

    strncpy(scale.items[2].grade_symbol, "C", sizeof(scale.items[2].grade_symbol) - 1);
    scale.items[2].grade_point = 3.0;

    strncpy(scale.items[3].grade_symbol, "D", sizeof(scale.items[3].grade_symbol) - 1);
    scale.items[3].grade_point = 2.0;

    strncpy(scale.items[4].grade_symbol, "E", sizeof(scale.items[4].grade_symbol) - 1);
    scale.items[4].grade_point = 1.0;

    strncpy(scale.items[5].grade_symbol, "F", sizeof(scale.items[5].grade_symbol) - 1);
    scale.items[5].grade_point = 0.0;

    return scale;
}

static void test_target_solver_branch_1_unreachable(void) {
    GGGradingScale scale = create_standard_scale();
    GGUpcomingCourse upcoming[3];
    memset(upcoming, 0, sizeof(upcoming));

    strncpy(upcoming[0].course_label, "Course 1", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 1;

    strncpy(upcoming[1].course_label, "Course 2", sizeof(upcoming[1].course_label) - 1);
    upcoming[1].credit_unit = 1;

    strncpy(upcoming[2].course_label, "Course 3", sizeof(upcoming[2].course_label) - 1);
    upcoming[2].credit_unit = 1;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 36.0, 9, 4.50, upcoming, 3, &result);

    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_UNREACHABLE, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.25, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)result.assignment_count);
}

static void test_target_solver_branch_2_already_guaranteed(void) {
    GGGradingScale scale = create_standard_scale();
    GGUpcomingCourse upcoming[3];
    memset(upcoming, 0, sizeof(upcoming));

    strncpy(upcoming[0].course_label, "Course 1", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 1;

    strncpy(upcoming[1].course_label, "Course 2", sizeof(upcoming[1].course_label) - 1);
    upcoming[1].credit_unit = 1;

    strncpy(upcoming[2].course_label, "Course 3", sizeof(upcoming[2].course_label) - 1);
    upcoming[2].credit_unit = 1;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 36.0, 9, 2.50, upcoming, 3, &result);

    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_ALREADY_GUARANTEED, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.25, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(3, (uint32_t)result.assignment_count);

    TEST_ASSERT_EQUAL_STRING("F", result.assignments[0].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, result.assignments[0].grade_point);
    TEST_ASSERT_EQUAL_STRING("F", result.assignments[1].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, result.assignments[1].grade_point);
    TEST_ASSERT_EQUAL_STRING("F", result.assignments[2].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, result.assignments[2].grade_point);
}

static void test_target_solver_branch_3_reachable_with_effort_equal_units(void) {
    GGGradingScale scale = create_standard_scale();
    GGUpcomingCourse upcoming[3];
    memset(upcoming, 0, sizeof(upcoming));

    strncpy(upcoming[0].course_label, "Course 1", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 1;

    strncpy(upcoming[1].course_label, "Course 2", sizeof(upcoming[1].course_label) - 1);
    upcoming[1].credit_unit = 1;

    strncpy(upcoming[2].course_label, "Course 3", sizeof(upcoming[2].course_label) - 1);
    upcoming[2].credit_unit = 1;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 36.0, 9, 4.10, upcoming, 3, &result);

    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.25, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.40, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(3, (uint32_t)result.assignment_count);

    TEST_ASSERT_EQUAL_STRING("A", result.assignments[0].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 5.0, result.assignments[0].grade_point);
    TEST_ASSERT_EQUAL_STRING("A", result.assignments[1].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 5.0, result.assignments[1].grade_point);
    TEST_ASSERT_EQUAL_STRING("B", result.assignments[2].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.0, result.assignments[2].grade_point);
}

static void test_target_solver_branch_3_reachable_with_effort_varying_units(void) {
    GGGradingScale scale = create_standard_scale();
    GGUpcomingCourse upcoming[3];
    memset(upcoming, 0, sizeof(upcoming));

    strncpy(upcoming[0].course_label, "Course 1", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 4;

    strncpy(upcoming[1].course_label, "Course 2", sizeof(upcoming[1].course_label) - 1);
    upcoming[1].credit_unit = 3;

    strncpy(upcoming[2].course_label, "Course 3", sizeof(upcoming[2].course_label) - 1);
    upcoming[2].credit_unit = 3;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 82.0, 21, 4.10, upcoming, 3, &result);

    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.258, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.51, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(3, (uint32_t)result.assignment_count);

    TEST_ASSERT_EQUAL_STRING("A", result.assignments[0].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 5.0, result.assignments[0].grade_point);
    TEST_ASSERT_EQUAL_STRING("A", result.assignments[1].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 5.0, result.assignments[1].grade_point);
    TEST_ASSERT_EQUAL_STRING("B", result.assignments[2].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.0, result.assignments[2].grade_point);
}

static void test_target_solver_invalid_arguments(void) {
    GGGradingScale scale = create_standard_scale();
    GGUpcomingCourse upcoming[1];
    memset(upcoming, 0, sizeof(upcoming));
    upcoming[0].credit_unit = 3;
    GGSolverResult result;

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(NULL, 36.0, 9, 4.10, upcoming, 1, &result));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 36.0, 9, 4.10, upcoming, 1, NULL));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, -1.0, 9, 4.10, upcoming, 1, &result));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 36.0, 9, 0.0, upcoming, 1, &result));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 36.0, 9, -1.0, upcoming, 1, &result));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 36.0, 9, 4.10, NULL, 1, &result));

    upcoming[0].credit_unit = 0;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 36.0, 9, 4.10, upcoming, 1, &result));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 0.0, 0, 4.10, NULL, 0, &result));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_target_solver_branch_1_unreachable);
    RUN_TEST(test_target_solver_branch_2_already_guaranteed);
    RUN_TEST(test_target_solver_branch_3_reachable_with_effort_equal_units);
    RUN_TEST(test_target_solver_branch_3_reachable_with_effort_varying_units);
    RUN_TEST(test_target_solver_invalid_arguments);
    return UNITY_END();
}
