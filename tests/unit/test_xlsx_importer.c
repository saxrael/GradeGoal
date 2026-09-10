#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite_connection.h"
#include "../../src/persistence/scale_repository.h"
#include "../../src/persistence/course_repository.h"
#include "../../src/io/xlsx_exporter.h"
#include "../../src/io/xlsx_importer.h"

static GGDbConnection *db_conn = NULL;
static GGScaleRepository *scale_repo = NULL;
static GGCourseRepository *course_repo = NULL;
static const char *test_wb_path = "test_import_wb.xlsx";
static const char *test_corrupt_path = "test_corrupt_wb.xlsx";

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

    status = gg_xlsx_export(test_wb_path, scale_repo, course_repo);
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
    remove(test_wb_path);
    remove(test_corrupt_path);
}

static void test_import_null_args(void) {
    GGImportSummary *summary = NULL;
    gg_import_summary_create(&summary);
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_xlsx_import(NULL, scale_repo, course_repo, false, summary));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_xlsx_import(test_wb_path, NULL, course_repo, false, summary));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_xlsx_import(test_wb_path, scale_repo, NULL, false, summary));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_xlsx_import(test_wb_path, scale_repo, course_repo, false, NULL));
    gg_import_summary_destroy(summary);
}

static void test_import_roundtrip(void) {
    GGDbConnection *new_db = NULL;
    GGScaleRepository *new_scale_repo = NULL;
    GGCourseRepository *new_course_repo = NULL;

    GGStatus status = gg_db_connect(":memory:", &new_db);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_db_bootstrap_schema(new_db);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_sqlite_scale_repository_create(new_db, &new_scale_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_sqlite_course_repository_create(new_db, &new_course_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGImportSummary *summary = NULL;
    gg_import_summary_create(&summary);
    status = gg_xlsx_import(test_wb_path, new_scale_repo, new_course_repo, false, summary);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(1, summary->valid_rows_imported);
    TEST_ASSERT_EQUAL_UINT(0, summary->count);

    double tcp = 0.0;
    uint32_t tcu = 0;
    status = new_course_repo->get_live_totals(new_course_repo->context, &tcp, &tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(3, tcu);
    TEST_ASSERT_EQUAL_DOUBLE(15.0, tcp);

    gg_import_summary_destroy(summary);
    new_course_repo->destroy(new_course_repo);
    new_scale_repo->destroy(new_scale_repo);
    gg_db_disconnect(new_db);
}

static void test_import_dry_run(void) {
    GGDbConnection *new_db = NULL;
    GGScaleRepository *new_scale_repo = NULL;
    GGCourseRepository *new_course_repo = NULL;

    GGStatus status = gg_db_connect(":memory:", &new_db);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_db_bootstrap_schema(new_db);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_sqlite_scale_repository_create(new_db, &new_scale_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_sqlite_course_repository_create(new_db, &new_course_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGImportSummary *summary = NULL;
    gg_import_summary_create(&summary);
    status = gg_xlsx_import(test_wb_path, new_scale_repo, new_course_repo, true, summary);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(1, summary->valid_rows_imported);

    double tcp = 0.0;
    uint32_t tcu = 0;
    status = new_course_repo->get_live_totals(new_course_repo->context, &tcp, &tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(0, tcu);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, tcp);

    gg_import_summary_destroy(summary);
    new_course_repo->destroy(new_course_repo);
    new_scale_repo->destroy(new_scale_repo);
    gg_db_disconnect(new_db);
}

static void test_import_corrupted_missing_sheet(void) {
    FILE *fp = fopen(test_corrupt_path, "wb");
    TEST_ASSERT_NOT_NULL(fp);
    fputs("GRADEGOAL_WORKBOOK_V1\n[SHEET:Scale]\ngrade_symbol\tgrade_point\nA\t5.0\n", fp);
    fclose(fp);

    GGImportSummary *summary = NULL;
    gg_import_summary_create(&summary);
    GGStatus status = gg_xlsx_import(test_corrupt_path, scale_repo, course_repo, false, summary);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, status);
    TEST_ASSERT_EQUAL_UINT(1, summary->count);
    gg_import_summary_destroy(summary);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_import_null_args);
    RUN_TEST(test_import_roundtrip);
    RUN_TEST(test_import_dry_run);
    RUN_TEST(test_import_corrupted_missing_sheet);
    return UNITY_END();
}
