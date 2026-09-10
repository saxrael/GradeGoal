#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite_connection.h"
#include "../../src/persistence/scale_repository.h"
#include "../../src/persistence/course_repository.h"
#include "../../src/io/xlsx_exporter.h"

static GGDbConnection *db_conn = NULL;
static GGScaleRepository *scale_repo = NULL;
static GGCourseRepository *course_repo = NULL;
static const char *test_export_path = "test_output.xlsx";

void setUp(void) {
    GGStatus status = gg_db_connect(":memory:", &db_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_db_bootstrap_schema(db_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_sqlite_scale_repository_create(db_conn, &scale_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_sqlite_course_repository_create(db_conn, &course_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 2;
    scale.max_point = 5.0;
    scale.min_point = 0.0;
    snprintf(scale.items[0].grade_symbol, sizeof(scale.items[0].grade_symbol), "A");
    scale.items[0].grade_point = 5.0;
    snprintf(scale.items[1].grade_symbol, sizeof(scale.items[1].grade_symbol), "F");
    scale.items[1].grade_point = 0.0;
    status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1 Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "CS101");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    entry.entry_date = 1700000000;
    int64_t out_id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &out_id);
    TEST_ASSERT_EQUAL(GG_OK, status);
}

void tearDown(void) {
    if (course_repo != NULL) {
        course_repo->destroy(course_repo);
        course_repo = NULL;
    }
    if (scale_repo != NULL) {
        scale_repo->destroy(scale_repo);
        scale_repo = NULL;
    }
    if (db_conn != NULL) {
        gg_db_disconnect(db_conn);
        db_conn = NULL;
    }
    remove(test_export_path);
}

static void test_export_null_args(void) {
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_xlsx_export(NULL, scale_repo, course_repo));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_xlsx_export(test_export_path, NULL, course_repo));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_xlsx_export(test_export_path, scale_repo, NULL));
}

static void test_export_success(void) {
    GGStatus status = gg_xlsx_export(test_export_path, scale_repo, course_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);

    FILE *fp = fopen(test_export_path, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    TEST_ASSERT_TRUE(size > 0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_export_null_args);
    RUN_TEST(test_export_success);
    return UNITY_END();
}
