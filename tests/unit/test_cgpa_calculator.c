#include "cgpa_calculator.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

static void test_cgpa_calculate_vector_1_fresh_start(void) {
    double cgpa = -1.0;
    GGStatus status = gg_cgpa_calculate(37.0, 9, &cgpa);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.1111, cgpa);
}

static void test_cgpa_combine_standing_vector_1(void) {
    double cgpa = -1.0;
    double combined_tcp = -1.0;
    uint32_t combined_tcu = 0;

    GGStatus status = gg_cgpa_combine_standing(0.0, 0, 37.0, 9, &cgpa, &combined_tcp, &combined_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.1111, cgpa);
    TEST_ASSERT_EQUAL_UINT32(9, combined_tcu);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 37.0, combined_tcp);
}

static void test_cgpa_combine_standing_vector_2_with_prior_history(void) {
    double cgpa = -1.0;
    double combined_tcp = -1.0;
    uint32_t combined_tcu = 0;

    GGStatus status = gg_cgpa_combine_standing(45.0, 12, 37.0, 9, &cgpa, &combined_tcp, &combined_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 3.9048, cgpa);
    TEST_ASSERT_EQUAL_UINT32(21, combined_tcu);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 82.0, combined_tcp);
}

static void test_cgpa_calculate_vector_3_zero_division_guard(void) {
    double cgpa = -1.0;
    GGStatus status = gg_cgpa_calculate(0.0, 0, &cgpa);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, cgpa);
}

static void test_cgpa_combine_standing_zero_division_guard(void) {
    double cgpa = -1.0;
    double combined_tcp = -1.0;
    uint32_t combined_tcu = 999;

    GGStatus status = gg_cgpa_combine_standing(0.0, 0, 0.0, 0, &cgpa, &combined_tcp, &combined_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, cgpa);
    TEST_ASSERT_EQUAL_UINT32(0, combined_tcu);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, combined_tcp);
}

static void test_cgpa_calculate_null_output_rejected(void) {
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_calculate(37.0, 9, NULL));
}

static void test_cgpa_calculate_negative_tcp_rejected(void) {
    double cgpa = 0.0;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_calculate(-1.0, 9, &cgpa));
}

static void test_cgpa_combine_standing_null_outputs_rejected(void) {
    double cgpa = 0.0;
    double tcp = 0.0;
    uint32_t tcu = 0;

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_combine_standing(0.0, 0, 37.0, 9, NULL, &tcp, &tcu));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_combine_standing(0.0, 0, 37.0, 9, &cgpa, NULL, &tcu));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_combine_standing(0.0, 0, 37.0, 9, &cgpa, &tcp, NULL));
}

static void test_cgpa_combine_standing_negative_inputs_rejected(void) {
    double cgpa = 0.0;
    double tcp = 0.0;
    uint32_t tcu = 0;

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_combine_standing(-1.0, 0, 37.0, 9, &cgpa, &tcp, &tcu));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_cgpa_combine_standing(0.0, 0, -1.0, 9, &cgpa, &tcp, &tcu));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cgpa_calculate_vector_1_fresh_start);
    RUN_TEST(test_cgpa_combine_standing_vector_1);
    RUN_TEST(test_cgpa_combine_standing_vector_2_with_prior_history);
    RUN_TEST(test_cgpa_calculate_vector_3_zero_division_guard);
    RUN_TEST(test_cgpa_combine_standing_zero_division_guard);
    RUN_TEST(test_cgpa_calculate_null_output_rejected);
    RUN_TEST(test_cgpa_calculate_negative_tcp_rejected);
    RUN_TEST(test_cgpa_combine_standing_null_outputs_rejected);
    RUN_TEST(test_cgpa_combine_standing_negative_inputs_rejected);
    return UNITY_END();
}
