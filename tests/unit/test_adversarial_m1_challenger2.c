#include "course_list.h"
#include "scale_validator.h"
#include "unity.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

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

static GGCourseEntry create_standard_course(void) {
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.id = 1;
    entry.credit_unit = 3;
    entry.entry_date = 1700000000;
    strncpy(entry.semester_label, "Fall 2026", sizeof(entry.semester_label) - 1);
    strncpy(entry.course_label, "CS101", sizeof(entry.course_label) - 1);
    strncpy(entry.grade_symbol, "A", sizeof(entry.grade_symbol) - 1);
    return entry;
}

static void test_scale_null_pointer_rejected(void) {
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_scale_validator_validate_scale(NULL));
}

static void test_scale_zero_count_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = 0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_count_one_accepted(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 1;
    scale.max_point = 4.0;
    scale.min_point = 4.0;
    strncpy(scale.items[0].grade_symbol, "PASS", sizeof(scale.items[0].grade_symbol) - 1);
    scale.items[0].grade_point = 4.0;

    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_count_sixteen_max_accepted(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 16;
    scale.max_point = 15.0;
    scale.min_point = 0.0;

    for (size_t i = 0; i < 16; i++) {
        snprintf(scale.items[i].grade_symbol, sizeof(scale.items[i].grade_symbol), "G%zu", i);
        scale.items[i].grade_point = (double)(15 - i);
    }

    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_count_seventeen_exceeds_max_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = 17;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_count_large_overflow_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = 1000;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_duplicate_symbols_adjacent(void) {
    GGGradingScale scale = create_standard_scale();
    strncpy(scale.items[1].grade_symbol, "A", sizeof(scale.items[1].grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_duplicate_symbols_first_and_last(void) {
    GGGradingScale scale = create_standard_scale();
    strncpy(scale.items[5].grade_symbol, "A", sizeof(scale.items[5].grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_duplicate_symbols_middle(void) {
    GGGradingScale scale = create_standard_scale();
    strncpy(scale.items[4].grade_symbol, "C", sizeof(scale.items[4].grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_duplicate_symbols_multichar(void) {
    GGGradingScale scale = create_standard_scale();
    strncpy(scale.items[0].grade_symbol, "AB", sizeof(scale.items[0].grade_symbol) - 1);
    strncpy(scale.items[2].grade_symbol, "AB", sizeof(scale.items[2].grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_out_of_order_points_single_swap(void) {
    GGGradingScale scale = create_standard_scale();
    scale.items[2].grade_point = 2.0;
    scale.items[3].grade_point = 3.0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_out_of_order_points_strictly_ascending(void) {
    GGGradingScale scale = create_standard_scale();
    for (size_t i = 0; i < scale.count; i++) {
        scale.items[i].grade_point = (double)i;
    }
    scale.max_point = 5.0;
    scale.min_point = 0.0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_identical_adjacent_points_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.items[1].grade_point = 5.0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_negative_points_lowest_item(void) {
    GGGradingScale scale = create_standard_scale();
    scale.items[5].grade_point = -0.5;
    scale.min_point = -0.5;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_negative_points_middle_item(void) {
    GGGradingScale scale = create_standard_scale();
    scale.items[3].grade_point = -1.0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_empty_symbol_at_index_zero(void) {
    GGGradingScale scale = create_standard_scale();
    scale.items[0].grade_symbol[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_empty_symbol_at_last_index(void) {
    GGGradingScale scale = create_standard_scale();
    scale.items[5].grade_symbol[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_non_null_terminated_symbol_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    memset(scale.items[0].grade_symbol, 'X', sizeof(scale.items[0].grade_symbol));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_valid_seven_char_symbol(void) {
    GGGradingScale scale = create_standard_scale();
    strncpy(scale.items[0].grade_symbol, "ABCDEFG", sizeof(scale.items[0].grade_symbol) - 1);
    scale.items[0].grade_symbol[sizeof(scale.items[0].grade_symbol) - 1] = '\0';
    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_max_point_mismatch_higher(void) {
    GGGradingScale scale = create_standard_scale();
    scale.max_point = 5.5;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_max_point_mismatch_lower(void) {
    GGGradingScale scale = create_standard_scale();
    scale.max_point = 4.5;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_min_point_mismatch_higher(void) {
    GGGradingScale scale = create_standard_scale();
    scale.min_point = 0.5;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_scale_min_point_mismatch_lower(void) {
    GGGradingScale scale = create_standard_scale();
    scale.min_point = -0.5;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_course_null_scale_rejected(void) {
    GGCourseEntry entry = create_standard_course();
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_scale_validator_validate_course(NULL, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, ggvalidate_course_entry(NULL, &entry));
}

static void test_course_null_entry_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_scale_validator_validate_course(&scale, NULL));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, ggvalidate_course_entry(&scale, NULL));
}

static void test_course_both_pointers_null_rejected(void) {
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_scale_validator_validate_course(NULL, NULL));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, ggvalidate_course_entry(NULL, NULL));
}

static void test_course_zero_credit_units_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    entry.credit_unit = 0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_huge_credit_units_accepted(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    entry.credit_unit = 4294967295U;
    TEST_ASSERT_EQUAL(GG_OK, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_symbol_not_found_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    strncpy(entry.grade_symbol, "Z", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_symbol_case_sensitivity_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    strncpy(entry.grade_symbol, "a", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_symbol_partial_prefix_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    strncpy(scale.items[0].grade_symbol, "AA", sizeof(scale.items[0].grade_symbol) - 1);
    GGCourseEntry entry = create_standard_course();
    strncpy(entry.grade_symbol, "A", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_symbol_empty_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    entry.grade_symbol[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_symbol_non_null_terminated_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    memset(entry.grade_symbol, 'A', sizeof(entry.grade_symbol));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_semester_label_empty_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    entry.semester_label[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_semester_label_boundary_31_chars_accepted(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    memset(entry.semester_label, 'S', 31);
    entry.semester_label[31] = '\0';
    TEST_ASSERT_EQUAL(GG_OK, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_semester_label_boundary_32_chars_no_null_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    memset(entry.semester_label, 'S', sizeof(entry.semester_label));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_course_label_empty_accepted(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    entry.course_label[0] = '\0';
    TEST_ASSERT_EQUAL(GG_OK, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_course_label_boundary_31_chars_accepted(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    memset(entry.course_label, 'C', 31);
    entry.course_label[31] = '\0';
    TEST_ASSERT_EQUAL(GG_OK, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_course_label_boundary_32_chars_no_null_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry = create_standard_course();
    memset(entry.course_label, 'C', sizeof(entry.course_label));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_empty_scale_rejected(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 0;
    GGCourseEntry entry = create_standard_course();
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_scale_count_seventeen_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = 17;
    GGCourseEntry entry = create_standard_course();
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_scale_count_size_max_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = SIZE_MAX;
    GGCourseEntry entry = create_standard_course();
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_scale_count_large_overflow_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = 1000;
    GGCourseEntry entry = create_standard_course();
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_scale_count_one_boundary_accepted(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 1;
    scale.max_point = 4.0;
    scale.min_point = 4.0;
    strncpy(scale.items[0].grade_symbol, "PASS", sizeof(scale.items[0].grade_symbol) - 1);
    scale.items[0].grade_point = 4.0;

    GGCourseEntry entry = create_standard_course();
    strncpy(entry.grade_symbol, "PASS", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_OK, ggvalidate_course_entry(&scale, &entry));

    strncpy(entry.grade_symbol, "FAIL", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_scale_count_sixteen_max_boundary_accepted(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 16;
    scale.max_point = 15.0;
    scale.min_point = 0.0;

    for (size_t i = 0; i < 16; i++) {
        snprintf(scale.items[i].grade_symbol, sizeof(scale.items[i].grade_symbol), "G%zu", i);
        scale.items[i].grade_point = (double)(15 - i);
    }

    GGCourseEntry entry = create_standard_course();

    strncpy(entry.grade_symbol, "G0", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));

    strncpy(entry.grade_symbol, "G8", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));

    strncpy(entry.grade_symbol, "G15", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));

    strncpy(entry.grade_symbol, "G16", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
}

static void test_course_scale_all_valid_counts_from_one_to_sixteen(void) {
    for (size_t c = 1; c <= 16; c++) {
        GGGradingScale scale;
        memset(&scale, 0, sizeof(scale));
        scale.count = c;
        scale.max_point = (double)c;
        scale.min_point = 1.0;

        for (size_t i = 0; i < c; i++) {
            snprintf(scale.items[i].grade_symbol, sizeof(scale.items[i].grade_symbol), "S%zu", i);
            scale.items[i].grade_point = (double)(c - i);
        }

        GGCourseEntry entry = create_standard_course();

        strncpy(entry.grade_symbol, "S0", sizeof(entry.grade_symbol) - 1);
        TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));

        snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "S%zu", c - 1);
        TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));

        strncpy(entry.grade_symbol, "NOPE", sizeof(entry.grade_symbol) - 1);
        TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
    }
}

static void test_course_corrupted_scale_all_zeros_rejected(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    GGCourseEntry entry = create_standard_course();
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_corrupted_scale_all_ones_rejected(void) {
    GGGradingScale scale;
    memset(&scale, 0xFF, sizeof(scale));
    GGCourseEntry entry = create_standard_course();
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_scale_with_junk_past_count_safely_ignored(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = 6;
    for (size_t i = 6; i < 16; i++) {
        memset(scale.items[i].grade_symbol, (int)(0x50 + i), sizeof(scale.items[i].grade_symbol));
        scale.items[i].grade_point = -999.0;
    }

    GGCourseEntry entry = create_standard_course();
    strncpy(entry.grade_symbol, "A", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));

    strncpy(entry.grade_symbol, "F", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));

    strncpy(entry.grade_symbol, "Z", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
}

static void test_course_scale_items_non_null_terminated_safely_handled(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 2;
    scale.max_point = 2.0;
    scale.min_point = 1.0;
    memset(scale.items[0].grade_symbol, 'X', sizeof(scale.items[0].grade_symbol));
    scale.items[0].grade_point = 2.0;
    strncpy(scale.items[1].grade_symbol, "Y", sizeof(scale.items[1].grade_symbol) - 1);
    scale.items[1].grade_point = 1.0;

    GGCourseEntry entry = create_standard_course();
    strncpy(entry.grade_symbol, "Y", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));

    strncpy(entry.grade_symbol, "Z", sizeof(entry.grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
}

static void test_course_list_null_arguments(void) {
    GGCourseList *list = NULL;
    GGCourseEntry entry = create_standard_course();

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_course_list_create(5, NULL));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_course_list_append(NULL, &entry));

    TEST_ASSERT_EQUAL(GG_OK, gg_course_list_create(5, &list));
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_course_list_append(list, NULL));

    gg_course_list_destroy(list);
    gg_course_list_destroy(NULL);
}

static void test_course_list_destroy_empty_immediate(void) {
    GGCourseList *list = NULL;
    TEST_ASSERT_EQUAL(GG_OK, gg_course_list_create(0, &list));
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)list->count);
    TEST_ASSERT_EQUAL_UINT32(8, (uint32_t)list->capacity);
    gg_course_list_destroy(list);
}

static void test_course_list_destroy_capacity_one(void) {
    GGCourseList *list = NULL;
    TEST_ASSERT_EQUAL(GG_OK, gg_course_list_create(1, &list));
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)list->count);
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)list->capacity);
    gg_course_list_destroy(list);
}

static void test_course_list_stress_1500_entries_reallocation(void) {
    GGCourseList *list = NULL;
    TEST_ASSERT_EQUAL(GG_OK, gg_course_list_create(1, &list));
    TEST_ASSERT_NOT_NULL(list);

    const size_t total_entries = 1500;
    for (size_t i = 0; i < total_entries; i++) {
        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.id = (int64_t)(i + 1);
        entry.credit_unit = (uint32_t)((i % 6) + 1);
        entry.entry_date = 1700000000 + (int64_t)i;
        snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem_%zu", i % 8);
        snprintf(entry.course_label, sizeof(entry.course_label), "CRS_%04zu", i);
        snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");

        GGStatus status = gg_course_list_append(list, &entry);
        TEST_ASSERT_EQUAL(GG_OK, status);
    }

    TEST_ASSERT_EQUAL_UINT32((uint32_t)total_entries, (uint32_t)list->count);
    TEST_ASSERT_EQUAL_UINT32(2048, (uint32_t)list->capacity);

    for (size_t i = 0; i < total_entries; i++) {
        char expected_label[32];
        snprintf(expected_label, sizeof(expected_label), "CRS_%04zu", i);
        TEST_ASSERT_EQUAL_INT64((int64_t)(i + 1), list->entries[i].id);
        TEST_ASSERT_EQUAL_STRING(expected_label, list->entries[i].course_label);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)((i % 6) + 1), list->entries[i].credit_unit);
    }

    gg_course_list_destroy(list);
}

static void test_course_list_rapid_cycling(void) {
    for (size_t cycle = 0; cycle < 200; cycle++) {
        GGCourseList *list = NULL;
        TEST_ASSERT_EQUAL(GG_OK, gg_course_list_create(2, &list));
        TEST_ASSERT_NOT_NULL(list);

        for (size_t i = 0; i < 5; i++) {
            GGCourseEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.id = (int64_t)i;
            entry.credit_unit = 3;
            snprintf(entry.semester_label, sizeof(entry.semester_label), "S%zu", cycle);
            snprintf(entry.course_label, sizeof(entry.course_label), "C%zu", i);
            strncpy(entry.grade_symbol, "A", sizeof(entry.grade_symbol) - 1);

            TEST_ASSERT_EQUAL(GG_OK, gg_course_list_append(list, &entry));
        }

        TEST_ASSERT_EQUAL_UINT32(5, (uint32_t)list->count);
        TEST_ASSERT_EQUAL_UINT32(8, (uint32_t)list->capacity);
        gg_course_list_destroy(list);
    }
}

static void test_course_list_large_initial_capacity(void) {
    GGCourseList *list = NULL;
    const size_t initial_cap = 2500;
    TEST_ASSERT_EQUAL(GG_OK, gg_course_list_create(initial_cap, &list));
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)initial_cap, (uint32_t)list->capacity);

    for (size_t i = 0; i < initial_cap; i++) {
        GGCourseEntry entry = create_standard_course();
        entry.id = (int64_t)i;
        TEST_ASSERT_EQUAL(GG_OK, gg_course_list_append(list, &entry));
    }

    TEST_ASSERT_EQUAL_UINT32((uint32_t)initial_cap, (uint32_t)list->count);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)initial_cap, (uint32_t)list->capacity);

    gg_course_list_destroy(list);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_scale_null_pointer_rejected);
    RUN_TEST(test_scale_zero_count_rejected);
    RUN_TEST(test_scale_count_one_accepted);
    RUN_TEST(test_scale_count_sixteen_max_accepted);
    RUN_TEST(test_scale_count_seventeen_exceeds_max_rejected);
    RUN_TEST(test_scale_count_large_overflow_rejected);
    RUN_TEST(test_scale_duplicate_symbols_adjacent);
    RUN_TEST(test_scale_duplicate_symbols_first_and_last);
    RUN_TEST(test_scale_duplicate_symbols_middle);
    RUN_TEST(test_scale_duplicate_symbols_multichar);
    RUN_TEST(test_scale_out_of_order_points_single_swap);
    RUN_TEST(test_scale_out_of_order_points_strictly_ascending);
    RUN_TEST(test_scale_identical_adjacent_points_rejected);
    RUN_TEST(test_scale_negative_points_lowest_item);
    RUN_TEST(test_scale_negative_points_middle_item);
    RUN_TEST(test_scale_empty_symbol_at_index_zero);
    RUN_TEST(test_scale_empty_symbol_at_last_index);
    RUN_TEST(test_scale_non_null_terminated_symbol_rejected);
    RUN_TEST(test_scale_valid_seven_char_symbol);
    RUN_TEST(test_scale_max_point_mismatch_higher);
    RUN_TEST(test_scale_max_point_mismatch_lower);
    RUN_TEST(test_scale_min_point_mismatch_higher);
    RUN_TEST(test_scale_min_point_mismatch_lower);

    RUN_TEST(test_course_null_scale_rejected);
    RUN_TEST(test_course_null_entry_rejected);
    RUN_TEST(test_course_both_pointers_null_rejected);
    RUN_TEST(test_course_zero_credit_units_rejected);
    RUN_TEST(test_course_huge_credit_units_accepted);
    RUN_TEST(test_course_symbol_not_found_rejected);
    RUN_TEST(test_course_symbol_case_sensitivity_rejected);
    RUN_TEST(test_course_symbol_partial_prefix_rejected);
    RUN_TEST(test_course_symbol_empty_rejected);
    RUN_TEST(test_course_symbol_non_null_terminated_rejected);
    RUN_TEST(test_course_semester_label_empty_rejected);
    RUN_TEST(test_course_semester_label_boundary_31_chars_accepted);
    RUN_TEST(test_course_semester_label_boundary_32_chars_no_null_rejected);
    RUN_TEST(test_course_course_label_empty_accepted);
    RUN_TEST(test_course_course_label_boundary_31_chars_accepted);
    RUN_TEST(test_course_course_label_boundary_32_chars_no_null_rejected);
    RUN_TEST(test_course_empty_scale_rejected);
    RUN_TEST(test_course_scale_count_seventeen_rejected);
    RUN_TEST(test_course_scale_count_size_max_rejected);
    RUN_TEST(test_course_scale_count_large_overflow_rejected);
    RUN_TEST(test_course_scale_count_one_boundary_accepted);
    RUN_TEST(test_course_scale_count_sixteen_max_boundary_accepted);
    RUN_TEST(test_course_scale_all_valid_counts_from_one_to_sixteen);
    RUN_TEST(test_course_corrupted_scale_all_zeros_rejected);
    RUN_TEST(test_course_corrupted_scale_all_ones_rejected);
    RUN_TEST(test_course_scale_with_junk_past_count_safely_ignored);
    RUN_TEST(test_course_scale_items_non_null_terminated_safely_handled);

    RUN_TEST(test_course_list_null_arguments);
    RUN_TEST(test_course_list_destroy_empty_immediate);
    RUN_TEST(test_course_list_destroy_capacity_one);
    RUN_TEST(test_course_list_stress_1500_entries_reallocation);
    RUN_TEST(test_course_list_rapid_cycling);
    RUN_TEST(test_course_list_large_initial_capacity);

    return UNITY_END();
}
