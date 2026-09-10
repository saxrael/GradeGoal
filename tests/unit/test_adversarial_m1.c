#include "unity.h"
#include "target_solver.h"
#include "cgpa_calculator.h"
#include "scale_validator.h"
#include <string.h>
#include <stdint.h>

void setUp(void) {
}

void tearDown(void) {
}

static GGGradingScale create_scale_5_point(void) {
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

static void test_adversarial_extreme_scale_10_point(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 11;
    scale.max_point = 10.0;
    scale.min_point = 0.0;

    const char *symbols[11] = {"10", "9", "8", "7", "6", "5", "4", "3", "2", "1", "0"};
    for (size_t i = 0; i < 11; i++) {
        strncpy(scale.items[i].grade_symbol, symbols[i], sizeof(scale.items[i].grade_symbol) - 1);
        scale.items[i].grade_point = (double)(10 - i);
    }

    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_scale(&scale));

    GGUpcomingCourse upcoming[4];
    memset(upcoming, 0, sizeof(upcoming));
    strncpy(upcoming[0].course_label, "CS101", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 4;
    strncpy(upcoming[1].course_label, "CS102", sizeof(upcoming[1].course_label) - 1);
    upcoming[1].credit_unit = 3;
    strncpy(upcoming[2].course_label, "CS103", sizeof(upcoming[2].course_label) - 1);
    upcoming[2].credit_unit = 2;
    strncpy(upcoming[3].course_label, "CS104", sizeof(upcoming[3].course_label) - 1);
    upcoming[3].credit_unit = 1;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 70.0, 10, 8.5, upcoming, 4, &result);

    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 8.5, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 10.0, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)result.assignment_count);

    for (size_t i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_STRING("10", result.assignments[i].assigned_grade);
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, 10.0, result.assignments[i].grade_point);
    }
}

static void test_adversarial_single_tier_scale(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 1;
    scale.max_point = 4.0;
    scale.min_point = 4.0;
    strncpy(scale.items[0].grade_symbol, "PASS", sizeof(scale.items[0].grade_symbol) - 1);
    scale.items[0].grade_point = 4.0;

    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_scale(&scale));

    GGCourseEntry valid_entry;
    memset(&valid_entry, 0, sizeof(valid_entry));
    strncpy(valid_entry.semester_label, "Y1S1", sizeof(valid_entry.semester_label) - 1);
    strncpy(valid_entry.course_label, "PASS101", sizeof(valid_entry.course_label) - 1);
    valid_entry.credit_unit = 3;
    strncpy(valid_entry.grade_symbol, "PASS", sizeof(valid_entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_OK, ggvalidate_course_entry(&scale, &valid_entry));

    GGCourseEntry invalid_entry = valid_entry;
    strncpy(invalid_entry.grade_symbol, "FAIL", sizeof(invalid_entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &invalid_entry));

    GGUpcomingCourse upcoming[1];
    memset(upcoming, 0, sizeof(upcoming));
    strncpy(upcoming[0].course_label, "PCOURSE", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 3;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 8.0, 2, 4.0, upcoming, 1, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_ALREADY_GUARANTEED, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, result.max_achievable_cgpa);
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)result.assignment_count);
    TEST_ASSERT_EQUAL_STRING("PASS", result.assignments[0].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, result.assignments[0].grade_point);

    status = gg_target_solver_solve(&scale, 8.0, 2, 4.01, upcoming, 1, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_UNREACHABLE, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, result.max_achievable_cgpa);

    status = gg_target_solver_solve(&scale, 8.0, 2, 3.5, upcoming, 1, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_ALREADY_GUARANTEED, result.branch);
}

static void test_adversarial_non_standard_symbols_16_tiers(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 16;
    scale.max_point = 15.0;
    scale.min_point = 0.0;

    const char *labels[16] = {
        "A+", "A", "A-", "B+", "B", "B-", "C+", "C",
        "C-", "D+", "D", "D-", "E+", "E", "E-", "F"
    };

    for (size_t i = 0; i < 16; i++) {
        strncpy(scale.items[i].grade_symbol, labels[i], sizeof(scale.items[i].grade_symbol) - 1);
        scale.items[i].grade_point = (double)(15 - i);
    }

    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_scale(&scale));

    GGUpcomingCourse upcoming[32];
    memset(upcoming, 0, sizeof(upcoming));
    for (size_t i = 0; i < 32; i++) {
        upcoming[i].credit_unit = 1;
        upcoming[i].course_label[0] = 'U';
        upcoming[i].course_label[1] = (char)('0' + (i / 10));
        upcoming[i].course_label[2] = (char)('0' + (i % 10));
        upcoming[i].course_label[3] = '\0';
    }

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 0.0, 0, 7.5, upcoming, 32, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 15.0, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 7.5, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(32, (uint32_t)result.assignment_count);

    double total_points = 0.0;
    for (size_t i = 0; i < 32; i++) {
        total_points += (double)result.assignments[i].credit_unit * result.assignments[i].grade_point;
    }
    TEST_ASSERT_TRUE(total_points >= 240.0 - 1e-6);
}

static void test_adversarial_boundary_target_exact_max(void) {
    GGGradingScale scale = create_scale_5_point();
    GGUpcomingCourse upcoming[3];
    memset(upcoming, 0, sizeof(upcoming));
    for (size_t i = 0; i < 3; i++) {
        upcoming[i].credit_unit = 1;
        upcoming[i].course_label[0] = 'C';
        upcoming[i].course_label[1] = (char)('1' + i);
        upcoming[i].course_label[2] = '\0';
    }

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 36.0, 9, 4.25, upcoming, 3, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.25, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(3, (uint32_t)result.assignment_count);

    for (size_t i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_STRING("A", result.assignments[i].assigned_grade);
        TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, result.assignments[i].grade_point);
    }
}

static void test_adversarial_boundary_target_epsilon_transitions(void) {
    GGGradingScale scale = create_scale_5_point();
    GGUpcomingCourse upcoming[3];
    memset(upcoming, 0, sizeof(upcoming));
    for (size_t i = 0; i < 3; i++) {
        upcoming[i].credit_unit = 1;
        upcoming[i].course_label[0] = 'C';
        upcoming[i].course_label[1] = (char)('1' + i);
        upcoming[i].course_label[2] = '\0';
    }

    GGSolverResult result;
    double target_inside = 4.25 + 5e-11;
    GGStatus status = gg_target_solver_solve(&scale, 36.0, 9, target_inside, upcoming, 3, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);

    double target_outside = 4.25 + 2e-10;
    status = gg_target_solver_solve(&scale, 36.0, 9, target_outside, upcoming, 3, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_UNREACHABLE, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.25, result.max_achievable_cgpa);

    status = gg_target_solver_solve(&scale, 36.0, 9, 3.00, upcoming, 3, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_ALREADY_GUARANTEED, result.branch);

    double target_min_inside = 3.00 + 5e-11;
    status = gg_target_solver_solve(&scale, 36.0, 9, target_min_inside, upcoming, 3, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_ALREADY_GUARANTEED, result.branch);

    double target_min_outside = 3.00 + 2e-10;
    status = gg_target_solver_solve(&scale, 36.0, 9, target_min_outside, upcoming, 3, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_EQUAL_STRING("E", result.assignments[0].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, result.assignments[0].grade_point);
    TEST_ASSERT_EQUAL_STRING("F", result.assignments[1].assigned_grade);
    TEST_ASSERT_EQUAL_STRING("F", result.assignments[2].assigned_grade);
}

static void test_adversarial_boundary_target_negative_and_zero(void) {
    GGGradingScale scale = create_scale_5_point();
    GGUpcomingCourse upcoming[1];
    memset(upcoming, 0, sizeof(upcoming));
    upcoming[0].credit_unit = 3;
    strncpy(upcoming[0].course_label, "C1", sizeof(upcoming[0].course_label) - 1);

    GGSolverResult result;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 36.0, 9, 0.0, upcoming, 1, &result));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 36.0, 9, -1.0, upcoming, 1, &result));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 36.0, 9, -1e-9, upcoming, 1, &result));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, -0.01, 9, 3.5, upcoming, 1, &result));
}

static void test_adversarial_edge_case_32_upcoming_courses(void) {
    GGGradingScale scale = create_scale_5_point();
    GGUpcomingCourse upcoming[33];
    memset(upcoming, 0, sizeof(upcoming));

    for (size_t i = 0; i < 32; i++) {
        upcoming[i].credit_unit = (uint32_t)(4 - (i % 4));
        upcoming[i].course_label[0] = 'C';
        upcoming[i].course_label[1] = (char)('0' + (i / 10));
        upcoming[i].course_label[2] = (char)('0' + (i % 10));
        upcoming[i].course_label[3] = '\0';
    }

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 120.0, 40, 3.65, upcoming, 32, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_EQUAL_UINT32(32, (uint32_t)result.assignment_count);

    double total_points = 0.0;
    for (size_t i = 0; i < 32; i++) {
        total_points += (double)result.assignments[i].credit_unit * result.assignments[i].grade_point;
    }
    TEST_ASSERT_TRUE(total_points >= 318.0 - 1e-6);

    for (size_t i = 1; i < 32; i++) {
        TEST_ASSERT_TRUE(result.assignments[i - 1].credit_unit >= result.assignments[i].credit_unit);
    }

    upcoming[32].credit_unit = 3;
    strncpy(upcoming[32].course_label, "C33", sizeof(upcoming[32].course_label) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 120.0, 40, 3.65, upcoming, 33, &result));
}

static void test_adversarial_edge_case_zero_upcoming_courses(void) {
    GGGradingScale scale = create_scale_5_point();
    GGSolverResult result;

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 0.0, 0, 3.5, NULL, 0, &result));

    GGStatus status = gg_target_solver_solve(&scale, 35.0, 10, 3.0, NULL, 0, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_ALREADY_GUARANTEED, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.5, result.max_achievable_cgpa);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)result.assignment_count);

    status = gg_target_solver_solve(&scale, 35.0, 10, 4.0, NULL, 0, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_UNREACHABLE, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.5, result.max_achievable_cgpa);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)result.assignment_count);

    status = gg_target_solver_solve(&scale, 35.0, 10, 3.5, NULL, 0, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_ALREADY_GUARANTEED, result.branch);
}

static void test_adversarial_edge_case_single_course_high_credit(void) {
    GGGradingScale scale = create_scale_5_point();
    GGUpcomingCourse upcoming[1];
    memset(upcoming, 0, sizeof(upcoming));
    strncpy(upcoming[0].course_label, "MASSIVE", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 1000;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 0.0, 0, 3.20, upcoming, 1, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.20, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)result.assignment_count);
    TEST_ASSERT_EQUAL_STRING("B", result.assignments[0].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, result.assignments[0].grade_point);
}

static void test_adversarial_upcoming_label_not_null_terminated(void) {
    GGGradingScale scale = create_scale_5_point();
    GGUpcomingCourse upcoming[1];
    memset(upcoming[0].course_label, 'X', sizeof(upcoming[0].course_label));
    upcoming[0].credit_unit = 3;

    GGSolverResult result;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_target_solver_solve(&scale, 36.0, 9, 3.5, upcoming, 1, &result));
}

static void test_adversarial_cgpa_large_values_no_overflow(void) {
    double out_cgpa = 0.0;
    uint32_t max_tcu = UINT32_MAX;
    double max_tcp = (double)max_tcu * 4.5;

    GGStatus status = gg_cgpa_calculate(max_tcp, max_tcu, &out_cgpa);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.5, out_cgpa);

    uint32_t large_tcu = 1000000000;
    double large_tcp = 3700000000.0;
    status = gg_cgpa_calculate(large_tcp, large_tcu, &out_cgpa);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.70, out_cgpa);

    double combined_tcp = 0.0;
    uint32_t combined_tcu = 0;
    status = gg_cgpa_combine_standing(1800000000.0, 500000000, 1900000000.0, 500000000, &out_cgpa, &combined_tcp, &combined_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3700000000.0, combined_tcp);
    TEST_ASSERT_EQUAL_UINT32(1000000000, combined_tcu);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.70, out_cgpa);
}

static void test_adversarial_zero_division_and_negatives(void) {
    double out_cgpa = 123.45;
    TEST_ASSERT_EQUAL(GG_OK, gg_cgpa_calculate(0.0, 0, &out_cgpa));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, out_cgpa);

    TEST_ASSERT_EQUAL(GG_OK, gg_cgpa_calculate(50.0, 0, &out_cgpa));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, out_cgpa);

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_calculate(-0.01, 10, &out_cgpa));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_calculate(10.0, 10, NULL));

    double comb_tcp = 0.0;
    uint32_t comb_tcu = 0;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_combine_standing(10.0, 5, 10.0, 5, NULL, &comb_tcp, &comb_tcu));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_combine_standing(10.0, 5, 10.0, 5, &out_cgpa, NULL, &comb_tcu));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_combine_standing(10.0, 5, 10.0, 5, &out_cgpa, &comb_tcp, NULL));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_combine_standing(-1.0, 5, 10.0, 5, &out_cgpa, &comb_tcp, &comb_tcu));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_combine_standing(10.0, 5, -1.0, 5, &out_cgpa, &comb_tcp, &comb_tcu));
}

static void test_adversarial_course_validator_edges(void) {
    GGGradingScale scale = create_scale_5_point();
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.semester_label, "Semester 1", sizeof(entry.semester_label) - 1);
    strncpy(entry.course_label, "CSC101", sizeof(entry.course_label) - 1);
    entry.credit_unit = 3;
    strncpy(entry.grade_symbol, "A", sizeof(entry.grade_symbol) - 1);

    TEST_ASSERT_EQUAL(GG_OK, ggvalidate_course_entry(&scale, &entry));

    entry.credit_unit = 0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
    entry.credit_unit = 3;

    entry.semester_label[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
    strncpy(entry.semester_label, "Semester 1", sizeof(entry.semester_label) - 1);

    entry.grade_symbol[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));

    strncpy(entry.grade_symbol, "Z", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));

    memset(entry.course_label, 'X', sizeof(entry.course_label));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_copy_string_replicated(char *dest, size_t dest_size, const char *src) {
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

static void test_adversarial_string_copy_boundary_conditions(void) {
    test_copy_string_replicated(NULL, 10, "test");

    char buf[12];
    memset(buf, 0, sizeof(buf));
    strncpy(buf, "orig", sizeof(buf) - 1);
    test_copy_string_replicated(buf, 0, "test");
    TEST_ASSERT_EQUAL_STRING("orig", buf);

    test_copy_string_replicated(buf, sizeof(buf), NULL);
    TEST_ASSERT_EQUAL_STRING("", buf);

    strncpy(buf, "orig", sizeof(buf) - 1);
    test_copy_string_replicated(buf, sizeof(buf), "");
    TEST_ASSERT_EQUAL_STRING("", buf);

    char single_char_buf[2] = {'X', 'Y'};
    test_copy_string_replicated(single_char_buf, 1, "hello");
    TEST_ASSERT_EQUAL_CHAR('\0', single_char_buf[0]);
    TEST_ASSERT_EQUAL_CHAR('Y', single_char_buf[1]);

    char canary_buf[10];
    memset(canary_buf, 0x55, sizeof(canary_buf));
    test_copy_string_replicated(canary_buf + 1, 8, "ABCDEFG");
    TEST_ASSERT_EQUAL_HEX8((uint8_t)0x55, (uint8_t)canary_buf[0]);
    TEST_ASSERT_EQUAL_STRING("ABCDEFG", canary_buf + 1);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)0x55, (uint8_t)canary_buf[9]);

    memset(canary_buf, 0x55, sizeof(canary_buf));
    test_copy_string_replicated(canary_buf + 1, 8, "ABCDEFGH");
    TEST_ASSERT_EQUAL_HEX8((uint8_t)0x55, (uint8_t)canary_buf[0]);
    TEST_ASSERT_EQUAL_STRING("ABCDEFG", canary_buf + 1);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)0x55, (uint8_t)canary_buf[9]);

    memset(canary_buf, 0x55, sizeof(canary_buf));
    test_copy_string_replicated(canary_buf + 1, 8, "12345678901234567890");
    TEST_ASSERT_EQUAL_HEX8((uint8_t)0x55, (uint8_t)canary_buf[0]);
    TEST_ASSERT_EQUAL_STRING("1234567", canary_buf + 1);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)0x55, (uint8_t)canary_buf[9]);

    char label_buf[34];
    memset(label_buf, 0x66, sizeof(label_buf));
    test_copy_string_replicated(label_buf + 1, 32, "1234567890123456789012345678901");
    TEST_ASSERT_EQUAL_HEX8((uint8_t)0x66, (uint8_t)label_buf[0]);
    TEST_ASSERT_EQUAL_STRING("1234567890123456789012345678901", label_buf + 1);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)0x66, (uint8_t)label_buf[33]);
}

static void test_adversarial_solver_string_max_symbol_and_label_bounds(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 16;
    scale.max_point = 15.0;
    scale.min_point = 0.0;

    const char *symbols[16] = {
        "GRADE01", "GRADE02", "GRADE03", "GRADE04",
        "GRADE05", "GRADE06", "GRADE07", "GRADE08",
        "GRADE09", "GRADE10", "GRADE11", "GRADE12",
        "GRADE13", "GRADE14", "GRADE15", "GRADE16"
    };

    for (size_t i = 0; i < 16; i++) {
        strncpy(scale.items[i].grade_symbol, symbols[i], sizeof(scale.items[i].grade_symbol) - 1);
        scale.items[i].grade_point = (double)(15 - i);
    }

    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_scale(&scale));

    GGUpcomingCourse upcoming[4];
    memset(upcoming, 0, sizeof(upcoming));

    memcpy(upcoming[0].course_label, "0123456789012345678901234567890", 31);
    upcoming[0].course_label[31] = '\0';
    upcoming[0].credit_unit = 4;

    upcoming[1].course_label[0] = '\0';
    upcoming[1].credit_unit = 3;

    strncpy(upcoming[2].course_label, "A", sizeof(upcoming[2].course_label) - 1);
    upcoming[2].credit_unit = 2;

    strncpy(upcoming[3].course_label, "NORMAL_LABEL", sizeof(upcoming[3].course_label) - 1);
    upcoming[3].credit_unit = 1;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 150.0, 10, 2.0, upcoming, 4, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_ALREADY_GUARANTEED, result.branch);
    TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)result.assignment_count);

    for (size_t i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_STRING("GRADE16", result.assignments[i].assigned_grade);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, result.assignments[i].grade_point);
    }
    TEST_ASSERT_EQUAL_STRING("0123456789012345678901234567890", result.assignments[0].course_label);
    TEST_ASSERT_EQUAL_STRING("", result.assignments[1].course_label);
    TEST_ASSERT_EQUAL_STRING("A", result.assignments[2].course_label);
    TEST_ASSERT_EQUAL_STRING("NORMAL_LABEL", result.assignments[3].course_label);

    status = gg_target_solver_solve(&scale, 0.0, 0, 14.5, upcoming, 4, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)result.assignment_count);

    TEST_ASSERT_EQUAL_STRING("GRADE01", result.assignments[0].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 15.0, result.assignments[0].grade_point);

    TEST_ASSERT_EQUAL_STRING("GRADE01", result.assignments[1].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 15.0, result.assignments[1].grade_point);

    TEST_ASSERT_EQUAL_STRING("GRADE01", result.assignments[2].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 15.0, result.assignments[2].grade_point);

    TEST_ASSERT_EQUAL_STRING("GRADE06", result.assignments[3].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 10.0, result.assignments[3].grade_point);

    TEST_ASSERT_EQUAL_STRING("0123456789012345678901234567890", result.assignments[0].course_label);
    TEST_ASSERT_EQUAL_STRING("", result.assignments[1].course_label);
    TEST_ASSERT_EQUAL_STRING("A", result.assignments[2].course_label);
    TEST_ASSERT_EQUAL_STRING("NORMAL_LABEL", result.assignments[3].course_label);
}

static void test_adversarial_solver_branch_1_unreachable_precision(void) {
    GGGradingScale scale = create_scale_5_point();
    GGUpcomingCourse upcoming[2];
    memset(upcoming, 0, sizeof(upcoming));
    strncpy(upcoming[0].course_label, "CS301", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 3;
    strncpy(upcoming[1].course_label, "CS302", sizeof(upcoming[1].course_label) - 1);
    upcoming[1].credit_unit = 2;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 50.0, 15, 3.76, upcoming, 2, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_UNREACHABLE, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.75, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)result.assignment_count);
}

static void test_adversarial_solver_branch_2_guaranteed_precision(void) {
    GGGradingScale scale = create_scale_5_point();
    GGUpcomingCourse upcoming[2];
    memset(upcoming, 0, sizeof(upcoming));
    strncpy(upcoming[0].course_label, "CS301", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 3;
    strncpy(upcoming[1].course_label, "CS302", sizeof(upcoming[1].course_label) - 1);
    upcoming[1].credit_unit = 2;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 70.0, 15, 3.40, upcoming, 2, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_ALREADY_GUARANTEED, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.75, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)result.assignment_count);
    TEST_ASSERT_EQUAL_STRING("F", result.assignments[0].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, result.assignments[0].grade_point);
    TEST_ASSERT_EQUAL_STRING("F", result.assignments[1].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, result.assignments[1].grade_point);
}

static void test_adversarial_solver_branch_3_effort_hand_calculated(void) {
    GGGradingScale scale = create_scale_5_point();
    GGUpcomingCourse upcoming[4];
    memset(upcoming, 0, sizeof(upcoming));
    strncpy(upcoming[0].course_label, "C1", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 4;
    strncpy(upcoming[1].course_label, "C2", sizeof(upcoming[1].course_label) - 1);
    upcoming[1].credit_unit = 3;
    strncpy(upcoming[2].course_label, "C3", sizeof(upcoming[2].course_label) - 1);
    upcoming[2].credit_unit = 2;
    strncpy(upcoming[3].course_label, "C4", sizeof(upcoming[3].course_label) - 1);
    upcoming[3].credit_unit = 1;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 40.0, 12, 3.50, upcoming, 4, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, (40.0 + 50.0) / 22.0, result.max_achievable_cgpa);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.70, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)result.assignment_count);

    TEST_ASSERT_EQUAL_STRING("A", result.assignments[0].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, result.assignments[0].grade_point);
    TEST_ASSERT_EQUAL_UINT32(4, result.assignments[0].credit_unit);

    TEST_ASSERT_EQUAL_STRING("A", result.assignments[1].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, result.assignments[1].grade_point);
    TEST_ASSERT_EQUAL_UINT32(3, result.assignments[1].credit_unit);

    TEST_ASSERT_EQUAL_STRING("E", result.assignments[2].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, result.assignments[2].grade_point);
    TEST_ASSERT_EQUAL_UINT32(2, result.assignments[2].credit_unit);

    TEST_ASSERT_EQUAL_STRING("F", result.assignments[3].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, result.assignments[3].grade_point);
    TEST_ASSERT_EQUAL_UINT32(1, result.assignments[3].credit_unit);

    double total_pts = 0.0;
    for (size_t i = 0; i < 4; i++) {
        total_pts += (double)result.assignments[i].credit_unit * result.assignments[i].grade_point;
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 37.0, total_pts);
}

static void test_adversarial_solver_non_uniform_step_scale(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 4;
    scale.max_point = 4.5;
    scale.min_point = 0.0;

    strncpy(scale.items[0].grade_symbol, "DIST", sizeof(scale.items[0].grade_symbol) - 1);
    scale.items[0].grade_point = 4.5;

    strncpy(scale.items[1].grade_symbol, "CRED", sizeof(scale.items[1].grade_symbol) - 1);
    scale.items[1].grade_point = 3.0;

    strncpy(scale.items[2].grade_symbol, "PASS", sizeof(scale.items[2].grade_symbol) - 1);
    scale.items[2].grade_point = 1.0;

    strncpy(scale.items[3].grade_symbol, "FAIL", sizeof(scale.items[3].grade_symbol) - 1);
    scale.items[3].grade_point = 0.0;

    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_scale(&scale));

    GGUpcomingCourse upcoming[2];
    memset(upcoming, 0, sizeof(upcoming));
    strncpy(upcoming[0].course_label, "MAJOR", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 3;
    strncpy(upcoming[1].course_label, "MINOR", sizeof(upcoming[1].course_label) - 1);
    upcoming[1].credit_unit = 2;

    GGSolverResult result;
    GGStatus status = gg_target_solver_solve(&scale, 10.0, 5, 2.50, upcoming, 2, &result);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_SOLVER_REACHABLE_WITH_EFFORT, result.branch);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.00, result.min_required_average);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)result.assignment_count);

    TEST_ASSERT_EQUAL_STRING("DIST", result.assignments[0].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.5, result.assignments[0].grade_point);
    TEST_ASSERT_EQUAL_STRING("PASS", result.assignments[1].assigned_grade);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, result.assignments[1].grade_point);

    double total_pts = (double)result.assignments[0].credit_unit * result.assignments[0].grade_point +
                       (double)result.assignments[1].credit_unit * result.assignments[1].grade_point;
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 15.5, total_pts);
    TEST_ASSERT_TRUE(total_pts >= 15.0);
}

static void test_adversarial_cgpa_precise_hand_calculated_fractionals(void) {
    double out_cgpa = -1.0;
    GGStatus status = gg_cgpa_calculate(13.5, 4, &out_cgpa);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.375, out_cgpa);

    double comb_tcp = 0.0;
    uint32_t comb_tcu = 0;
    status = gg_cgpa_combine_standing(10.25, 3, 15.5, 5, &out_cgpa, &comb_tcp, &comb_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 25.75, comb_tcp);
    TEST_ASSERT_EQUAL_UINT32(8, comb_tcu);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.21875, out_cgpa);

    status = gg_cgpa_calculate(0.0, 0, &out_cgpa);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, out_cgpa);

    status = gg_cgpa_calculate(1000000000.0, 250000000, &out_cgpa);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, out_cgpa);
}

static void test_adversarial_solver_memory_canaries(void) {
    GGGradingScale scale = create_scale_5_point();
    GGUpcomingCourse upcoming[2];
    memset(upcoming, 0, sizeof(upcoming));
    strncpy(upcoming[0].course_label, "C1", sizeof(upcoming[0].course_label) - 1);
    upcoming[0].credit_unit = 3;
    strncpy(upcoming[1].course_label, "C2", sizeof(upcoming[1].course_label) - 1);
    upcoming[1].credit_unit = 2;

    struct {
        uint8_t canary_before[32];
        GGSolverResult result;
        uint8_t canary_after[32];
    } frame;

    memset(frame.canary_before, 0xDE, sizeof(frame.canary_before));
    memset(frame.canary_after, 0xDE, sizeof(frame.canary_after));

    GGStatus status = gg_target_solver_solve(&scale, 30.0, 10, 3.5, upcoming, 2, &frame.result);
    TEST_ASSERT_EQUAL(GG_OK, status);

    for (size_t i = 0; i < sizeof(frame.canary_before); i++) {
        TEST_ASSERT_EQUAL_HEX8((uint8_t)0xDE, frame.canary_before[i]);
    }
    for (size_t i = 0; i < sizeof(frame.canary_after); i++) {
        TEST_ASSERT_EQUAL_HEX8((uint8_t)0xDE, frame.canary_after[i]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_adversarial_extreme_scale_10_point);
    RUN_TEST(test_adversarial_single_tier_scale);
    RUN_TEST(test_adversarial_non_standard_symbols_16_tiers);
    RUN_TEST(test_adversarial_boundary_target_exact_max);
    RUN_TEST(test_adversarial_boundary_target_epsilon_transitions);
    RUN_TEST(test_adversarial_boundary_target_negative_and_zero);
    RUN_TEST(test_adversarial_edge_case_32_upcoming_courses);
    RUN_TEST(test_adversarial_edge_case_zero_upcoming_courses);
    RUN_TEST(test_adversarial_edge_case_single_course_high_credit);
    RUN_TEST(test_adversarial_upcoming_label_not_null_terminated);
    RUN_TEST(test_adversarial_cgpa_large_values_no_overflow);
    RUN_TEST(test_adversarial_zero_division_and_negatives);
    RUN_TEST(test_adversarial_course_validator_edges);
    RUN_TEST(test_adversarial_string_copy_boundary_conditions);
    RUN_TEST(test_adversarial_solver_string_max_symbol_and_label_bounds);
    RUN_TEST(test_adversarial_solver_branch_1_unreachable_precision);
    RUN_TEST(test_adversarial_solver_branch_2_guaranteed_precision);
    RUN_TEST(test_adversarial_solver_branch_3_effort_hand_calculated);
    RUN_TEST(test_adversarial_solver_non_uniform_step_scale);
    RUN_TEST(test_adversarial_cgpa_precise_hand_calculated_fractionals);
    RUN_TEST(test_adversarial_solver_memory_canaries);
    return UNITY_END();
}
