#include "unity.h"
#include "scale_validator.h"
#include "course_list.h"
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

static void test_validate_scale_nominal_success(void) {
    GGGradingScale scale = create_standard_scale();
    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_scale(&scale));
}

static void test_validate_scale_null_pointer_rejected(void) {
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_scale_validator_validate_scale(NULL));
}

static void test_validate_scale_zero_count_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = 0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_validate_scale_excessive_count_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = 17;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_validate_scale_non_monotonic_points_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.items[1].grade_point = 2.0;
    scale.items[2].grade_point = 4.0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_validate_scale_duplicate_points_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.items[1].grade_point = 5.0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_validate_scale_negative_points_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.items[5].grade_point = -1.0;
    scale.min_point = -1.0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_validate_scale_duplicate_symbols_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    strncpy(scale.items[1].grade_symbol, "A", sizeof(scale.items[1].grade_symbol) - 1);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_validate_scale_empty_symbol_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.items[0].grade_symbol[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_validate_scale_inconsistent_extrema_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.max_point = 4.0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));

    scale = create_standard_scale();
    scale.min_point = 1.0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_scale(&scale));
}

static void test_validate_course_nominal_success(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.credit_unit = 3;
    strncpy(entry.semester_label, "Year 1 Semester 1", sizeof(entry.semester_label) - 1);
    strncpy(entry.course_label, "CSC101", sizeof(entry.course_label) - 1);
    strncpy(entry.grade_symbol, "A", sizeof(entry.grade_symbol) - 1);

    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_OK, ggvalidate_course_entry(&scale, &entry));
}

static void test_validate_course_null_arguments_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_scale_validator_validate_course(NULL, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_scale_validator_validate_course(&scale, NULL));
}

static void test_validate_course_zero_credit_unit_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.credit_unit = 0;
    strncpy(entry.semester_label, "Year 1 Semester 1", sizeof(entry.semester_label) - 1);
    strncpy(entry.grade_symbol, "A", sizeof(entry.grade_symbol) - 1);

    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
}

static void test_validate_course_empty_semester_label_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.credit_unit = 3;
    entry.semester_label[0] = '\0';
    strncpy(entry.grade_symbol, "A", sizeof(entry.grade_symbol) - 1);

    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
}

static void test_validate_course_empty_course_label_allowed(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.credit_unit = 3;
    strncpy(entry.semester_label, "Year 1 Semester 1", sizeof(entry.semester_label) - 1);
    entry.course_label[0] = '\0';
    strncpy(entry.grade_symbol, "B", sizeof(entry.grade_symbol) - 1);

    TEST_ASSERT_EQUAL(GG_OK, gg_scale_validator_validate_course(&scale, &entry));
}

static void test_validate_course_unlisted_grade_symbol_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.credit_unit = 3;
    strncpy(entry.semester_label, "Year 1 Semester 1", sizeof(entry.semester_label) - 1);
    strncpy(entry.grade_symbol, "Z", sizeof(entry.grade_symbol) - 1);

    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
}

static void test_validate_course_scale_zero_count_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = 0;
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.credit_unit = 3;
    strncpy(entry.semester_label, "Year 1 Semester 1", sizeof(entry.semester_label) - 1);
    strncpy(entry.course_label, "CSC101", sizeof(entry.course_label) - 1);
    strncpy(entry.grade_symbol, "A", sizeof(entry.grade_symbol) - 1);

    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_validate_course_scale_excessive_count_rejected(void) {
    GGGradingScale scale = create_standard_scale();
    scale.count = 17;
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.credit_unit = 3;
    strncpy(entry.semester_label, "Year 1 Semester 1", sizeof(entry.semester_label) - 1);
    strncpy(entry.course_label, "CSC101", sizeof(entry.course_label) - 1);
    strncpy(entry.grade_symbol, "A", sizeof(entry.grade_symbol) - 1);

    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, gg_scale_validator_validate_course(&scale, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, ggvalidate_course_entry(&scale, &entry));
}

static void test_course_list_lifecycle_and_growth(void) {
    GGCourseList *list = NULL;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_course_list_create(2, NULL));

    TEST_ASSERT_EQUAL(GG_OK, gg_course_list_create(0, &list));
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)list->count);
    TEST_ASSERT_EQUAL_UINT32(8, (uint32_t)list->capacity);
    gg_course_list_destroy(list);

    TEST_ASSERT_EQUAL(GG_OK, gg_course_list_create(2, &list));
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)list->count);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)list->capacity);

    GGCourseEntry entry1;
    memset(&entry1, 0, sizeof(entry1));
    entry1.credit_unit = 3;
    strncpy(entry1.course_label, "CS101", sizeof(entry1.course_label) - 1);

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_course_list_append(NULL, &entry1));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_course_list_append(list, NULL));

    TEST_ASSERT_EQUAL(GG_OK, gg_course_list_append(list, &entry1));
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)list->count);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)list->capacity);

    GGCourseEntry entry2;
    memset(&entry2, 0, sizeof(entry2));
    entry2.credit_unit = 4;
    strncpy(entry2.course_label, "CS102", sizeof(entry2.course_label) - 1);

    TEST_ASSERT_EQUAL(GG_OK, gg_course_list_append(list, &entry2));
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)list->count);
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)list->capacity);

    GGCourseEntry entry3;
    memset(&entry3, 0, sizeof(entry3));
    entry3.credit_unit = 2;
    strncpy(entry3.course_label, "CS103", sizeof(entry3.course_label) - 1);

    TEST_ASSERT_EQUAL(GG_OK, gg_course_list_append(list, &entry3));
    TEST_ASSERT_EQUAL_UINT32(3, (uint32_t)list->count);
    TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)list->capacity);

    TEST_ASSERT_EQUAL_STRING("CS101", list->entries[0].course_label);
    TEST_ASSERT_EQUAL_STRING("CS102", list->entries[1].course_label);
    TEST_ASSERT_EQUAL_STRING("CS103", list->entries[2].course_label);

    gg_course_list_destroy(list);
    gg_course_list_destroy(NULL);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_validate_scale_nominal_success);
    RUN_TEST(test_validate_scale_null_pointer_rejected);
    RUN_TEST(test_validate_scale_zero_count_rejected);
    RUN_TEST(test_validate_scale_excessive_count_rejected);
    RUN_TEST(test_validate_scale_non_monotonic_points_rejected);
    RUN_TEST(test_validate_scale_duplicate_points_rejected);
    RUN_TEST(test_validate_scale_negative_points_rejected);
    RUN_TEST(test_validate_scale_duplicate_symbols_rejected);
    RUN_TEST(test_validate_scale_empty_symbol_rejected);
    RUN_TEST(test_validate_scale_inconsistent_extrema_rejected);
    RUN_TEST(test_validate_course_nominal_success);
    RUN_TEST(test_validate_course_null_arguments_rejected);
    RUN_TEST(test_validate_course_zero_credit_unit_rejected);
    RUN_TEST(test_validate_course_empty_semester_label_rejected);
    RUN_TEST(test_validate_course_empty_course_label_allowed);
    RUN_TEST(test_validate_course_unlisted_grade_symbol_rejected);
    RUN_TEST(test_validate_course_scale_zero_count_rejected);
    RUN_TEST(test_validate_course_scale_excessive_count_rejected);
    RUN_TEST(test_course_list_lifecycle_and_growth);
    return UNITY_END();
}
