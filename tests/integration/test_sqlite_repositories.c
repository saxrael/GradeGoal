#include "unity.h"
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include "sqlite_connection.h"
#include "scale_repository.h"
#include "course_repository.h"
#include "../core/course_list.h"

static GGDbConnection *db_conn = NULL;
static GGScaleRepository *scale_repo = NULL;
static GGCourseRepository *course_repo = NULL;

void setUp(void) {
    GGStatus status = gg_db_connect(":memory:", &db_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_db_bootstrap_schema(db_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_sqlite_scale_repository_create(db_conn, &scale_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_sqlite_course_repository_create(db_conn, &course_repo);
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
}

static GGGradingScale create_5point_scale(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 6;
    scale.max_point = 5.0;
    scale.min_point = 0.0;

    snprintf(scale.items[0].grade_symbol, sizeof(scale.items[0].grade_symbol), "A");
    scale.items[0].grade_point = 5.0;

    snprintf(scale.items[1].grade_symbol, sizeof(scale.items[1].grade_symbol), "B");
    scale.items[1].grade_point = 4.0;

    snprintf(scale.items[2].grade_symbol, sizeof(scale.items[2].grade_symbol), "C");
    scale.items[2].grade_point = 3.0;

    snprintf(scale.items[3].grade_symbol, sizeof(scale.items[3].grade_symbol), "D");
    scale.items[3].grade_point = 2.0;

    snprintf(scale.items[4].grade_symbol, sizeof(scale.items[4].grade_symbol), "E");
    scale.items[4].grade_point = 1.0;

    snprintf(scale.items[5].grade_symbol, sizeof(scale.items[5].grade_symbol), "F");
    scale.items[5].grade_point = 0.0;

    return scale;
}

static void test_sqlite_connection_pragmas(void) {
    sqlite3 *db = gg_db_get_handle(db_conn);
    TEST_ASSERT_NOT_NULL(db);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, "PRAGMA foreign_keys;", -1, &stmt, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    TEST_ASSERT_EQUAL(SQLITE_ROW, rc);
    int fk = sqlite3_column_int(stmt, 0);
    TEST_ASSERT_EQUAL(1, fk);
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(db, "PRAGMA synchronous;", -1, &stmt, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    TEST_ASSERT_EQUAL(SQLITE_ROW, rc);
    int sync_mode = sqlite3_column_int(stmt, 0);
    TEST_ASSERT_EQUAL(1, sync_mode);
    sqlite3_finalize(stmt);
}

static void test_sqlite_scale_repository_save_and_load(void) {
    GGGradingScale empty_scale;
    GGStatus status = scale_repo->load_scale(scale_repo->context, &empty_scale);
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, status);

    GGGradingScale scale = create_5point_scale();
    status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale loaded;
    status = scale_repo->load_scale(scale_repo->context, &loaded);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(6, loaded.count);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 5.0, loaded.max_point);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, loaded.min_point);
    TEST_ASSERT_EQUAL_STRING("A", loaded.items[0].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 5.0, loaded.items[0].grade_point);
    TEST_ASSERT_EQUAL_STRING("B", loaded.items[1].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 4.0, loaded.items[1].grade_point);
    TEST_ASSERT_EQUAL_STRING("C", loaded.items[2].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 3.0, loaded.items[2].grade_point);
    TEST_ASSERT_EQUAL_STRING("D", loaded.items[3].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 2.0, loaded.items[3].grade_point);
    TEST_ASSERT_EQUAL_STRING("E", loaded.items[4].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 1.0, loaded.items[4].grade_point);
    TEST_ASSERT_EQUAL_STRING("F", loaded.items[5].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, loaded.items[5].grade_point);
}

static void test_sqlite_scale_repository_invalid_args(void) {
    GGGradingScale scale = create_5point_scale();
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, scale_repo->save_scale(NULL, &scale));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, scale_repo->save_scale(scale_repo->context, NULL));

    GGGradingScale bad_scale = scale;
    bad_scale.count = 0;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, scale_repo->save_scale(scale_repo->context, &bad_scale));

    bad_scale.count = 17;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, scale_repo->save_scale(scale_repo->context, &bad_scale));

    bad_scale.count = 1;
    bad_scale.items[0].grade_symbol[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, scale_repo->save_scale(scale_repo->context, &bad_scale));

    snprintf(bad_scale.items[0].grade_symbol, sizeof(bad_scale.items[0].grade_symbol), "X");
    bad_scale.items[0].grade_point = -0.5;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, scale_repo->save_scale(scale_repo->context, &bad_scale));

    GGGradingScale loaded;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, scale_repo->load_scale(NULL, &loaded));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, scale_repo->load_scale(scale_repo->context, NULL));
}

static void test_sqlite_scale_repository_foreign_key_constraint(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1 Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "CSC 101");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "Z");
    entry.entry_date = 1000;

    int64_t out_id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &out_id);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, status);
}

static void test_sqlite_scale_repository_delete_restrict(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1 Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "CSC 101");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    entry.entry_date = 1000;

    int64_t out_id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &out_id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale restricted_scale;
    memset(&restricted_scale, 0, sizeof(restricted_scale));
    restricted_scale.count = 1;
    snprintf(restricted_scale.items[0].grade_symbol, sizeof(restricted_scale.items[0].grade_symbol), "B");
    restricted_scale.items[0].grade_point = 4.0;
    restricted_scale.max_point = 4.0;
    restricted_scale.min_point = 4.0;

    status = scale_repo->save_scale(scale_repo->context, &restricted_scale);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, status);
}

static void test_sqlite_scale_repository_cascade_update(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1 Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "CSC 101");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "B");
    entry.entry_date = 1000;

    int64_t out_id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &out_id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    double tcp = 0.0;
    uint32_t tcu = 0;
    status = course_repo->get_live_totals(course_repo->context, &tcp, &tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 12.0, tcp);
    TEST_ASSERT_EQUAL_UINT32(3, tcu);

    scale.items[1].grade_point = 4.5;
    status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = course_repo->get_live_totals(course_repo->context, &tcp, &tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 13.5, tcp);
    TEST_ASSERT_EQUAL_UINT32(3, tcu);
}

static void test_sqlite_course_repository_insert_and_get_by_id(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1 Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "CSC 101");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    entry.entry_date = 12345678;

    int64_t out_id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &out_id);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_TRUE(out_id > 0);

    GGCourseEntry retrieved;
    status = course_repo->get_course_by_id(course_repo->context, out_id, &retrieved);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_INT64(out_id, retrieved.id);
    TEST_ASSERT_EQUAL_STRING("Year 1 Sem 1", retrieved.semester_label);
    TEST_ASSERT_EQUAL_STRING("CSC 101", retrieved.course_label);
    TEST_ASSERT_EQUAL_UINT32(3, retrieved.credit_unit);
    TEST_ASSERT_EQUAL_STRING("A", retrieved.grade_symbol);
    TEST_ASSERT_EQUAL_INT64(12345678, retrieved.entry_date);
}

static void test_sqlite_course_repository_insert_with_null_course_label(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1 Sem 1");
    entry.course_label[0] = '\0';
    entry.credit_unit = 2;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "B");
    entry.entry_date = 5555;

    int64_t out_id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &out_id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry retrieved;
    status = course_repo->get_course_by_id(course_repo->context, out_id, &retrieved);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_STRING("", retrieved.course_label);
    TEST_ASSERT_EQUAL_UINT32(2, retrieved.credit_unit);
}

static void test_sqlite_course_repository_validation_checks(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "CSC");
    entry.credit_unit = 0;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    int64_t out_id = 0;
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, course_repo->insert_course(course_repo->context, &entry, &out_id));

    entry.credit_unit = 3;
    entry.semester_label[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, course_repo->insert_course(course_repo->context, &entry, &out_id));

    snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem 1");
    entry.grade_symbol[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, course_repo->insert_course(course_repo->context, &entry, &out_id));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->insert_course(NULL, &entry, &out_id));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->insert_course(course_repo->context, NULL, &out_id));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->insert_course(course_repo->context, &entry, NULL));

    GGCourseEntry retrieved;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_course_by_id(course_repo->context, 0, &retrieved));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_course_by_id(course_repo->context, -1, &retrieved));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_course_by_id(course_repo->context, 1, NULL));
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, course_repo->get_course_by_id(course_repo->context, 999999, &retrieved));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->delete_course(course_repo->context, 0));
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, course_repo->delete_course(course_repo->context, 999999));

    entry.id = 999999;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, course_repo->update_course(course_repo->context, &entry));
}

static void test_sqlite_course_repository_update(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1 Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "CSC 101");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    entry.entry_date = 1000;

    int64_t id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    entry.id = id;
    snprintf(entry.course_label, sizeof(entry.course_label), "CSC 102");
    entry.credit_unit = 4;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "B");
    entry.entry_date = 2000;

    status = course_repo->update_course(course_repo->context, &entry);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry retrieved;
    status = course_repo->get_course_by_id(course_repo->context, id, &retrieved);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_STRING("CSC 102", retrieved.course_label);
    TEST_ASSERT_EQUAL_UINT32(4, retrieved.credit_unit);
    TEST_ASSERT_EQUAL_STRING("B", retrieved.grade_symbol);
    TEST_ASSERT_EQUAL_INT64(2000, retrieved.entry_date);
}

static void test_sqlite_course_repository_delete(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1 Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "CSC 101");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");

    int64_t id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = course_repo->delete_course(course_repo->context, id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry retrieved;
    status = course_repo->get_course_by_id(course_repo->context, id, &retrieved);
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, status);
}

static void test_sqlite_course_repository_list_all(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseList *list = NULL;
    status = course_repo->list_all_courses(course_repo->context, &list);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT(0, list->count);
    gg_course_list_destroy(list);
    list = NULL;

    GGCourseEntry c1;
    memset(&c1, 0, sizeof(c1));
    snprintf(c1.semester_label, sizeof(c1.semester_label), "Sem 1");
    snprintf(c1.course_label, sizeof(c1.course_label), "Course A");
    c1.credit_unit = 3;
    snprintf(c1.grade_symbol, sizeof(c1.grade_symbol), "A");
    c1.entry_date = 100;
    int64_t id1 = 0;
    course_repo->insert_course(course_repo->context, &c1, &id1);

    GGCourseEntry c2;
    memset(&c2, 0, sizeof(c2));
    snprintf(c2.semester_label, sizeof(c2.semester_label), "Sem 1");
    snprintf(c2.course_label, sizeof(c2.course_label), "Course B");
    c2.credit_unit = 4;
    snprintf(c2.grade_symbol, sizeof(c2.grade_symbol), "B");
    c2.entry_date = 200;
    int64_t id2 = 0;
    course_repo->insert_course(course_repo->context, &c2, &id2);

    status = course_repo->list_all_courses(course_repo->context, &list);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT(2, list->count);
    TEST_ASSERT_EQUAL_STRING("Course A", list->entries[0].course_label);
    TEST_ASSERT_EQUAL_STRING("Course B", list->entries[1].course_label);
    gg_course_list_destroy(list);
}

static void test_sqlite_course_repository_list_by_semester(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry c1;
    memset(&c1, 0, sizeof(c1));
    snprintf(c1.semester_label, sizeof(c1.semester_label), "Fall 2024");
    snprintf(c1.course_label, sizeof(c1.course_label), "C1");
    c1.credit_unit = 3;
    snprintf(c1.grade_symbol, sizeof(c1.grade_symbol), "A");
    c1.entry_date = 100;
    int64_t id1 = 0;
    course_repo->insert_course(course_repo->context, &c1, &id1);

    GGCourseEntry c2;
    memset(&c2, 0, sizeof(c2));
    snprintf(c2.semester_label, sizeof(c2.semester_label), "Spring 2025");
    snprintf(c2.course_label, sizeof(c2.course_label), "C2");
    c2.credit_unit = 4;
    snprintf(c2.grade_symbol, sizeof(c2.grade_symbol), "B");
    c2.entry_date = 200;
    int64_t id2 = 0;
    course_repo->insert_course(course_repo->context, &c2, &id2);

    GGCourseList *list = NULL;
    status = course_repo->list_courses_by_semester(course_repo->context, "Fall 2024", &list);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT(1, list->count);
    TEST_ASSERT_EQUAL_STRING("C1", list->entries[0].course_label);
    gg_course_list_destroy(list);

    status = course_repo->list_courses_by_semester(course_repo->context, "Summer 2025", &list);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT(0, list->count);
    gg_course_list_destroy(list);
}

static void test_sqlite_live_totals_empty_database(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    double tcp = -1.0;
    uint32_t tcu = 999;
    status = course_repo->get_live_totals(course_repo->context, &tcp, &tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, tcp);
    TEST_ASSERT_EQUAL_UINT32(0, tcu);
}

static void test_sqlite_live_totals_hand_calculated_vector(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry c1;
    memset(&c1, 0, sizeof(c1));
    snprintf(c1.semester_label, sizeof(c1.semester_label), "Sem 1");
    snprintf(c1.course_label, sizeof(c1.course_label), "CS101");
    c1.credit_unit = 3;
    snprintf(c1.grade_symbol, sizeof(c1.grade_symbol), "A");
    c1.entry_date = 1;
    int64_t id1 = 0;
    status = course_repo->insert_course(course_repo->context, &c1, &id1);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry c2;
    memset(&c2, 0, sizeof(c2));
    snprintf(c2.semester_label, sizeof(c2.semester_label), "Sem 1");
    snprintf(c2.course_label, sizeof(c2.course_label), "MTH101");
    c2.credit_unit = 4;
    snprintf(c2.grade_symbol, sizeof(c2.grade_symbol), "B");
    c2.entry_date = 2;
    int64_t id2 = 0;
    status = course_repo->insert_course(course_repo->context, &c2, &id2);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry c3;
    memset(&c3, 0, sizeof(c3));
    snprintf(c3.semester_label, sizeof(c3.semester_label), "Sem 1");
    snprintf(c3.course_label, sizeof(c3.course_label), "PHY101");
    c3.credit_unit = 2;
    snprintf(c3.grade_symbol, sizeof(c3.grade_symbol), "C");
    c3.entry_date = 3;
    int64_t id3 = 0;
    status = course_repo->insert_course(course_repo->context, &c3, &id3);
    TEST_ASSERT_EQUAL(GG_OK, status);

    double tcp = 0.0;
    uint32_t tcu = 0;
    status = course_repo->get_live_totals(course_repo->context, &tcp, &tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 37.0, tcp);
    TEST_ASSERT_EQUAL_UINT32(9, tcu);
}

static void test_sqlite_live_totals_after_course_mutation(void) {
    GGGradingScale scale = create_5point_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry c1;
    memset(&c1, 0, sizeof(c1));
    snprintf(c1.semester_label, sizeof(c1.semester_label), "Sem 1");
    snprintf(c1.course_label, sizeof(c1.course_label), "CS101");
    c1.credit_unit = 3;
    snprintf(c1.grade_symbol, sizeof(c1.grade_symbol), "A");
    c1.entry_date = 1;
    int64_t id1 = 0;
    course_repo->insert_course(course_repo->context, &c1, &id1);

    GGCourseEntry c2;
    memset(&c2, 0, sizeof(c2));
    snprintf(c2.semester_label, sizeof(c2.semester_label), "Sem 1");
    snprintf(c2.course_label, sizeof(c2.course_label), "MTH101");
    c2.credit_unit = 4;
    snprintf(c2.grade_symbol, sizeof(c2.grade_symbol), "B");
    c2.entry_date = 2;
    int64_t id2 = 0;
    course_repo->insert_course(course_repo->context, &c2, &id2);

    GGCourseEntry c3;
    memset(&c3, 0, sizeof(c3));
    snprintf(c3.semester_label, sizeof(c3.semester_label), "Sem 1");
    snprintf(c3.course_label, sizeof(c3.course_label), "PHY101");
    c3.credit_unit = 2;
    snprintf(c3.grade_symbol, sizeof(c3.grade_symbol), "C");
    c3.entry_date = 3;
    int64_t id3 = 0;
    course_repo->insert_course(course_repo->context, &c3, &id3);

    c3.id = id3;
    snprintf(c3.grade_symbol, sizeof(c3.grade_symbol), "A");
    status = course_repo->update_course(course_repo->context, &c3);
    TEST_ASSERT_EQUAL(GG_OK, status);

    double tcp = 0.0;
    uint32_t tcu = 0;
    status = course_repo->get_live_totals(course_repo->context, &tcp, &tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 41.0, tcp);
    TEST_ASSERT_EQUAL_UINT32(9, tcu);

    status = course_repo->delete_course(course_repo->context, id2);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = course_repo->get_live_totals(course_repo->context, &tcp, &tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 25.0, tcp);
    TEST_ASSERT_EQUAL_UINT32(5, tcu);
}

static void test_sqlite_schema_no_static_tcp_tcu(void) {
    sqlite3 *db = gg_db_get_handle(db_conn);
    TEST_ASSERT_NOT_NULL(db);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, "PRAGMA table_info(course_entries);", -1, &stmt, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char *col_name = (const char *)sqlite3_column_text(stmt, 1);
        TEST_ASSERT_NOT_NULL(col_name);
        TEST_ASSERT_NOT_EQUAL(0, strcmp(col_name, "tcp"));
        TEST_ASSERT_NOT_EQUAL(0, strcmp(col_name, "tcu"));
        TEST_ASSERT_NOT_EQUAL(0, strcmp(col_name, "total_credit_points"));
        TEST_ASSERT_NOT_EQUAL(0, strcmp(col_name, "total_credit_units"));
    }
    sqlite3_finalize(stmt);
}

static void test_sqlite_transactions(void) {
    TEST_ASSERT_EQUAL(GG_ERR_DB, gg_db_commit(db_conn));
    TEST_ASSERT_EQUAL(GG_OK, gg_db_rollback(db_conn));

    GGStatus status = gg_db_begin_immediate(db_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL(GG_ERR_DB, gg_db_begin_immediate(db_conn));

    GGGradingScale scale = create_5point_scale();
    status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_ERR_DB, status);

    status = gg_db_rollback(db_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = gg_db_begin_immediate(db_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "TransTest");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    int64_t id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = gg_db_rollback(db_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry retrieved;
    status = course_repo->get_course_by_id(course_repo->context, id, &retrieved);
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, status);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_sqlite_connection_pragmas);
    RUN_TEST(test_sqlite_scale_repository_save_and_load);
    RUN_TEST(test_sqlite_scale_repository_invalid_args);
    RUN_TEST(test_sqlite_scale_repository_foreign_key_constraint);
    RUN_TEST(test_sqlite_scale_repository_delete_restrict);
    RUN_TEST(test_sqlite_scale_repository_cascade_update);
    RUN_TEST(test_sqlite_course_repository_insert_and_get_by_id);
    RUN_TEST(test_sqlite_course_repository_insert_with_null_course_label);
    RUN_TEST(test_sqlite_course_repository_validation_checks);
    RUN_TEST(test_sqlite_course_repository_update);
    RUN_TEST(test_sqlite_course_repository_delete);
    RUN_TEST(test_sqlite_course_repository_list_all);
    RUN_TEST(test_sqlite_course_repository_list_by_semester);
    RUN_TEST(test_sqlite_live_totals_empty_database);
    RUN_TEST(test_sqlite_live_totals_hand_calculated_vector);
    RUN_TEST(test_sqlite_live_totals_after_course_mutation);
    RUN_TEST(test_sqlite_schema_no_static_tcp_tcu);
    RUN_TEST(test_sqlite_transactions);

    return UNITY_END();
}
