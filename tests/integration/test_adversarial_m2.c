#include "unity.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sqlite3.h>
#include "sqlite_connection.h"
#include "scale_repository.h"
#include "course_repository.h"
#include "../core/course_list.h"
#include "sqlite_error.h"

static GGDbConnection *global_conn = NULL;
static GGScaleRepository *global_scale_repo = NULL;
static GGCourseRepository *global_course_repo = NULL;

static void clean_file(const char *path) {
    char buf[256];
    remove(path);
    snprintf(buf, sizeof(buf), "%s-wal", path);
    remove(buf);
    snprintf(buf, sizeof(buf), "%s-shm", path);
    remove(buf);
}

void setUp(void) {
    GGStatus status = gg_db_connect(":memory:", &global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_db_bootstrap_schema(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_sqlite_scale_repository_create(global_conn, &global_scale_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_sqlite_course_repository_create(global_conn, &global_course_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);
}

void tearDown(void) {
    if (global_course_repo != NULL) {
        global_course_repo->destroy(global_course_repo);
        global_course_repo = NULL;
    }
    if (global_scale_repo != NULL) {
        global_scale_repo->destroy(global_scale_repo);
        global_scale_repo = NULL;
    }
    if (global_conn != NULL) {
        gg_db_disconnect(global_conn);
        global_conn = NULL;
    }
}

static GGGradingScale make_standard_scale(void) {
    GGGradingScale s;
    memset(&s, 0, sizeof(s));
    s.count = 5;
    s.max_point = 5.0;
    s.min_point = 0.0;
    snprintf(s.items[0].grade_symbol, sizeof(s.items[0].grade_symbol), "A");
    s.items[0].grade_point = 5.0;
    snprintf(s.items[1].grade_symbol, sizeof(s.items[1].grade_symbol), "B");
    s.items[1].grade_point = 4.0;
    snprintf(s.items[2].grade_symbol, sizeof(s.items[2].grade_symbol), "C");
    s.items[2].grade_point = 3.0;
    snprintf(s.items[3].grade_symbol, sizeof(s.items[3].grade_symbol), "D");
    s.items[3].grade_point = 2.0;
    snprintf(s.items[4].grade_symbol, sizeof(s.items[4].grade_symbol), "F");
    s.items[4].grade_point = 0.0;
    return s;
}

static void test_pragma_verification_memory(void) {
    sqlite3 *db = gg_db_get_handle(global_conn);
    TEST_ASSERT_NOT_NULL(db);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, "PRAGMA foreign_keys;", -1, &stmt, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    TEST_ASSERT_EQUAL(SQLITE_ROW, rc);
    TEST_ASSERT_EQUAL(1, sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(db, "PRAGMA synchronous;", -1, &stmt, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    TEST_ASSERT_EQUAL(SQLITE_ROW, rc);
    TEST_ASSERT_EQUAL(1, sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(db, "PRAGMA busy_timeout;", -1, &stmt, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    TEST_ASSERT_EQUAL(SQLITE_ROW, rc);
    TEST_ASSERT_EQUAL(5000, sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);
}

static void test_pragma_verification_disk(void) {
    const char *db_path = "test_adv_pragmas_disk.db";
    clean_file(db_path);

    GGDbConnection *conn = NULL;
    GGStatus status = gg_db_connect(db_path, &conn);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(conn);

    sqlite3 *db = gg_db_get_handle(conn);
    TEST_ASSERT_NOT_NULL(db);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, "PRAGMA foreign_keys;", -1, &stmt, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    TEST_ASSERT_EQUAL(SQLITE_ROW, rc);
    TEST_ASSERT_EQUAL(1, sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(db, "PRAGMA synchronous;", -1, &stmt, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    TEST_ASSERT_EQUAL(SQLITE_ROW, rc);
    TEST_ASSERT_EQUAL(1, sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(db, "PRAGMA busy_timeout;", -1, &stmt, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    TEST_ASSERT_EQUAL(SQLITE_ROW, rc);
    TEST_ASSERT_EQUAL(5000, sqlite3_column_int(stmt, 0));
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(db, "PRAGMA journal_mode;", -1, &stmt, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);
    rc = sqlite3_step(stmt);
    TEST_ASSERT_EQUAL(SQLITE_ROW, rc);
    const unsigned char *mode = sqlite3_column_text(stmt, 0);
    TEST_ASSERT_NOT_NULL(mode);
    TEST_ASSERT_EQUAL_STRING("wal", (const char *)mode);
    sqlite3_finalize(stmt);

    status = gg_db_disconnect(conn);
    TEST_ASSERT_EQUAL(GG_OK, status);
    clean_file(db_path);
}

static void test_transaction_state_transitions(void) {
    TEST_ASSERT_EQUAL(GG_ERR_DB, gg_db_commit(global_conn));
    TEST_ASSERT_EQUAL(GG_OK, gg_db_rollback(global_conn));

    TEST_ASSERT_EQUAL(GG_OK, gg_db_begin_immediate(global_conn));
    TEST_ASSERT_EQUAL(GG_ERR_DB, gg_db_begin_immediate(global_conn));

    TEST_ASSERT_EQUAL(GG_OK, gg_db_commit(global_conn));
    TEST_ASSERT_EQUAL(GG_ERR_DB, gg_db_commit(global_conn));

    TEST_ASSERT_EQUAL(GG_OK, gg_db_begin_immediate(global_conn));
    TEST_ASSERT_EQUAL(GG_OK, gg_db_rollback(global_conn));
    TEST_ASSERT_EQUAL(GG_OK, gg_db_rollback(global_conn));
    TEST_ASSERT_EQUAL(GG_ERR_DB, gg_db_commit(global_conn));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_db_begin_immediate(NULL));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_db_commit(NULL));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, gg_db_rollback(NULL));
    TEST_ASSERT_NULL(gg_db_get_handle(NULL));
}

static void test_transaction_stress_50_sequential(void) {
    GGGradingScale scale = make_standard_scale();
    GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    for (int i = 0; i < 50; ++i) {
        status = gg_db_begin_immediate(global_conn);
        TEST_ASSERT_EQUAL(GG_OK, status);

        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem %d", i % 4);
        snprintf(entry.course_label, sizeof(entry.course_label), "CS_%d", i);
        entry.credit_unit = 2;
        snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
        entry.entry_date = 1000 + i;

        int64_t out_id = 0;
        status = global_course_repo->insert_course(global_course_repo->context, &entry, &out_id);
        TEST_ASSERT_EQUAL(GG_OK, status);
        TEST_ASSERT_GREATER_THAN_INT64(0, out_id);

        if (i % 2 == 0) {
            status = gg_db_commit(global_conn);
            TEST_ASSERT_EQUAL(GG_OK, status);
        } else {
            status = gg_db_rollback(global_conn);
            TEST_ASSERT_EQUAL(GG_OK, status);
        }
    }

    GGCourseList *list = NULL;
    status = global_course_repo->list_all_courses(global_course_repo->context, &list);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT(25, list->count);
    gg_course_list_destroy(list);

    double tcp = 0.0;
    uint32_t tcu = 0;
    status = global_course_repo->get_live_totals(global_course_repo->context, &tcp, &tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(50, tcu);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 250.0, tcp);
}

static void test_transaction_rollback_complex_mutation(void) {
    GGGradingScale scale = make_standard_scale();
    GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry c1;
    memset(&c1, 0, sizeof(c1));
    snprintf(c1.semester_label, sizeof(c1.semester_label), "Year 1");
    snprintf(c1.course_label, sizeof(c1.course_label), "Base1");
    c1.credit_unit = 3;
    snprintf(c1.grade_symbol, sizeof(c1.grade_symbol), "B");
    int64_t id1 = 0;
    status = global_course_repo->insert_course(global_course_repo->context, &c1, &id1);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry c2;
    memset(&c2, 0, sizeof(c2));
    snprintf(c2.semester_label, sizeof(c2.semester_label), "Year 1");
    snprintf(c2.course_label, sizeof(c2.course_label), "Base2");
    c2.credit_unit = 4;
    snprintf(c2.grade_symbol, sizeof(c2.grade_symbol), "C");
    int64_t id2 = 0;
    status = global_course_repo->insert_course(global_course_repo->context, &c2, &id2);
    TEST_ASSERT_EQUAL(GG_OK, status);

    double init_tcp = 0.0;
    uint32_t init_tcu = 0;
    status = global_course_repo->get_live_totals(global_course_repo->context, &init_tcp, &init_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(7, init_tcu);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 24.0, init_tcp);

    status = gg_db_begin_immediate(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry c3;
    memset(&c3, 0, sizeof(c3));
    snprintf(c3.semester_label, sizeof(c3.semester_label), "Year 1");
    snprintf(c3.course_label, sizeof(c3.course_label), "MutInsert");
    c3.credit_unit = 5;
    snprintf(c3.grade_symbol, sizeof(c3.grade_symbol), "A");
    int64_t id3 = 0;
    status = global_course_repo->insert_course(global_course_repo->context, &c3, &id3);
    TEST_ASSERT_EQUAL(GG_OK, status);

    c1.id = id1;
    snprintf(c1.grade_symbol, sizeof(c1.grade_symbol), "A");
    status = global_course_repo->update_course(global_course_repo->context, &c1);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = global_course_repo->delete_course(global_course_repo->context, id2);
    TEST_ASSERT_EQUAL(GG_OK, status);

    double in_tcp = 0.0;
    uint32_t in_tcu = 0;
    status = global_course_repo->get_live_totals(global_course_repo->context, &in_tcp, &in_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(8, in_tcu);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 40.0, in_tcp);

    status = gg_db_rollback(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    double post_tcp = 0.0;
    uint32_t post_tcu = 0;
    status = global_course_repo->get_live_totals(global_course_repo->context, &post_tcp, &post_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(7, post_tcu);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 24.0, post_tcp);

    GGCourseEntry chk1;
    status = global_course_repo->get_course_by_id(global_course_repo->context, id1, &chk1);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_STRING("B", chk1.grade_symbol);

    GGCourseEntry chk2;
    status = global_course_repo->get_course_by_id(global_course_repo->context, id2, &chk2);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_STRING("C", chk2.grade_symbol);

    GGCourseEntry chk3;
    status = global_course_repo->get_course_by_id(global_course_repo->context, id3, &chk3);
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, status);
}

static void test_transaction_aborted_by_constraint(void) {
    GGGradingScale scale = make_standard_scale();
    GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = gg_db_begin_immediate(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry valid_entry;
    memset(&valid_entry, 0, sizeof(valid_entry));
    snprintf(valid_entry.semester_label, sizeof(valid_entry.semester_label), "Sem 1");
    snprintf(valid_entry.course_label, sizeof(valid_entry.course_label), "Valid1");
    valid_entry.credit_unit = 3;
    snprintf(valid_entry.grade_symbol, sizeof(valid_entry.grade_symbol), "A");
    int64_t valid_id = 0;
    status = global_course_repo->insert_course(global_course_repo->context, &valid_entry, &valid_id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry invalid_entry;
    memset(&invalid_entry, 0, sizeof(invalid_entry));
    snprintf(invalid_entry.semester_label, sizeof(invalid_entry.semester_label), "Sem 1");
    snprintf(invalid_entry.course_label, sizeof(invalid_entry.course_label), "InvalidFK");
    invalid_entry.credit_unit = 3;
    snprintf(invalid_entry.grade_symbol, sizeof(invalid_entry.grade_symbol), "Z_NONE");
    int64_t invalid_id = 0;
    status = global_course_repo->insert_course(global_course_repo->context, &invalid_entry, &invalid_id);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, status);

    status = gg_db_rollback(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry check_entry;
    status = global_course_repo->get_course_by_id(global_course_repo->context, valid_id, &check_entry);
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, status);

    status = gg_db_begin_immediate(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = global_course_repo->insert_course(global_course_repo->context, &valid_entry, &valid_id);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_db_commit(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = global_course_repo->get_course_by_id(global_course_repo->context, valid_id, &check_entry);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_STRING("Valid1", check_entry.course_label);
}

static void test_concurrency_wal_snapshot_isolation(void) {
    const char *db_path = "test_adv_wal_iso.db";
    clean_file(db_path);

    GGDbConnection *conn_a = NULL;
    GGStatus status = gg_db_connect(db_path, &conn_a);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_db_bootstrap_schema(conn_a);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGScaleRepository *scale_repo_a = NULL;
    status = gg_sqlite_scale_repository_create(conn_a, &scale_repo_a);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale scale = make_standard_scale();
    status = scale_repo_a->save_scale(scale_repo_a->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseRepository *course_repo_a = NULL;
    status = gg_sqlite_course_repository_create(conn_a, &course_repo_a);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGDbConnection *conn_b = NULL;
    status = gg_db_connect(db_path, &conn_b);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseRepository *course_repo_b = NULL;
    status = gg_sqlite_course_repository_create(conn_b, &course_repo_b);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = gg_db_begin_immediate(conn_a);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "IsolationTest");
    entry.credit_unit = 4;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    int64_t id_a = 0;
    status = course_repo_a->insert_course(course_repo_a->context, &entry, &id_a);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseList *list_b = NULL;
    status = course_repo_b->list_all_courses(course_repo_b->context, &list_b);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(list_b);
    TEST_ASSERT_EQUAL_UINT(0, list_b->count);
    gg_course_list_destroy(list_b);

    double tcp_b = 0.0;
    uint32_t tcu_b = 0;
    status = course_repo_b->get_live_totals(course_repo_b->context, &tcp_b, &tcu_b);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(0, tcu_b);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, tcp_b);

    status = gg_db_commit(conn_a);
    TEST_ASSERT_EQUAL(GG_OK, status);

    list_b = NULL;
    status = course_repo_b->list_all_courses(course_repo_b->context, &list_b);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(list_b);
    TEST_ASSERT_EQUAL_UINT(1, list_b->count);
    TEST_ASSERT_EQUAL_STRING("IsolationTest", list_b->entries[0].course_label);
    gg_course_list_destroy(list_b);

    status = course_repo_b->get_live_totals(course_repo_b->context, &tcp_b, &tcu_b);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(4, tcu_b);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 20.0, tcp_b);

    course_repo_b->destroy(course_repo_b);
    gg_db_disconnect(conn_b);

    course_repo_a->destroy(course_repo_a);
    scale_repo_a->destroy(scale_repo_a);
    gg_db_disconnect(conn_a);

    clean_file(db_path);
}

static void test_disconnect_rolls_back_uncommitted(void) {
    const char *db_path = "test_adv_disc_rb.db";
    clean_file(db_path);

    GGDbConnection *conn1 = NULL;
    GGStatus status = gg_db_connect(db_path, &conn1);
    TEST_ASSERT_EQUAL(GG_OK, status);
    status = gg_db_bootstrap_schema(conn1);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGScaleRepository *scale_repo = NULL;
    status = gg_sqlite_scale_repository_create(conn1, &scale_repo);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale scale = make_standard_scale();
    status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseRepository *course_repo1 = NULL;
    status = gg_sqlite_course_repository_create(conn1, &course_repo1);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = gg_db_begin_immediate(conn1);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "GhostCourse");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    int64_t ghost_id = 0;
    status = course_repo1->insert_course(course_repo1->context, &entry, &ghost_id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    course_repo1->destroy(course_repo1);
    scale_repo->destroy(scale_repo);
    status = gg_db_disconnect(conn1);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGDbConnection *conn2 = NULL;
    status = gg_db_connect(db_path, &conn2);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseRepository *course_repo2 = NULL;
    status = gg_sqlite_course_repository_create(conn2, &course_repo2);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry check;
    status = course_repo2->get_course_by_id(course_repo2->context, ghost_id, &check);
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, status);

    GGCourseList *list2 = NULL;
    status = course_repo2->list_all_courses(course_repo2->context, &list2);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(list2);
    TEST_ASSERT_EQUAL_UINT(0, list2->count);
    gg_course_list_destroy(list2);

    course_repo2->destroy(course_repo2);
    gg_db_disconnect(conn2);

    clean_file(db_path);
}

static void test_scale_repository_16_items_max_capacity(void) {
    const char *symbols[16] = {
        "A+", "A", "A-", "B+", "B", "B-", "C+", "C",
        "C-", "D+", "D", "D-", "E", "F+", "F", "F-"
    };
    const double points[16] = {
        5.0, 4.7, 4.3, 4.0, 3.7, 3.3, 3.0, 2.7,
        2.3, 2.0, 1.7, 1.3, 1.0, 0.7, 0.3, 0.0
    };

    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 16;
    scale.max_point = 5.0;
    scale.min_point = 0.0;
    for (size_t i = 0; i < 16; ++i) {
        snprintf(scale.items[i].grade_symbol, sizeof(scale.items[i].grade_symbol), "%s", symbols[i]);
        scale.items[i].grade_point = points[i];
    }

    GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale loaded;
    status = global_scale_repo->load_scale(global_scale_repo->context, &loaded);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(16, loaded.count);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, loaded.max_point);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, loaded.min_point);

    for (size_t i = 0; i < 16; ++i) {
        TEST_ASSERT_EQUAL_STRING(symbols[i], loaded.items[i].grade_symbol);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, points[i], loaded.items[i].grade_point);
    }
}

static void test_scale_repository_single_item_boundary(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 1;
    scale.max_point = 4.0;
    scale.min_point = 4.0;
    snprintf(scale.items[0].grade_symbol, sizeof(scale.items[0].grade_symbol), "PASS");
    scale.items[0].grade_point = 4.0;

    GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale loaded;
    status = global_scale_repo->load_scale(global_scale_repo->context, &loaded);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(1, loaded.count);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, loaded.max_point);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, loaded.min_point);
    TEST_ASSERT_EQUAL_STRING("PASS", loaded.items[0].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, loaded.items[0].grade_point);
}

static void test_scale_repository_complete_disjoint_replacement(void) {
    GGGradingScale scale1 = make_standard_scale();
    GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &scale1);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale scale2;
    memset(&scale2, 0, sizeof(scale2));
    scale2.count = 4;
    scale2.max_point = 7.0;
    scale2.min_point = 4.0;
    snprintf(scale2.items[0].grade_symbol, sizeof(scale2.items[0].grade_symbol), "HD");
    scale2.items[0].grade_point = 7.0;
    snprintf(scale2.items[1].grade_symbol, sizeof(scale2.items[1].grade_symbol), "DI");
    scale2.items[1].grade_point = 6.0;
    snprintf(scale2.items[2].grade_symbol, sizeof(scale2.items[2].grade_symbol), "CR");
    scale2.items[2].grade_point = 5.0;
    snprintf(scale2.items[3].grade_symbol, sizeof(scale2.items[3].grade_symbol), "PS");
    scale2.items[3].grade_point = 4.0;

    status = global_scale_repo->save_scale(global_scale_repo->context, &scale2);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale loaded;
    status = global_scale_repo->load_scale(global_scale_repo->context, &loaded);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(4, loaded.count);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 7.0, loaded.max_point);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, loaded.min_point);

    TEST_ASSERT_EQUAL_STRING("HD", loaded.items[0].grade_symbol);
    TEST_ASSERT_EQUAL_STRING("DI", loaded.items[1].grade_symbol);
    TEST_ASSERT_EQUAL_STRING("CR", loaded.items[2].grade_symbol);
    TEST_ASSERT_EQUAL_STRING("PS", loaded.items[3].grade_symbol);
}

static void test_scale_repository_atomic_rollback_on_restrict(void) {
    GGGradingScale initial_scale = make_standard_scale();
    GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &initial_scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "RefB");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "B");
    int64_t out_id = 0;
    status = global_course_repo->insert_course(global_course_repo->context, &entry, &out_id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale conflicting_scale;
    memset(&conflicting_scale, 0, sizeof(conflicting_scale));
    conflicting_scale.count = 2;
    conflicting_scale.max_point = 5.0;
    conflicting_scale.min_point = 3.0;
    snprintf(conflicting_scale.items[0].grade_symbol, sizeof(conflicting_scale.items[0].grade_symbol), "A");
    conflicting_scale.items[0].grade_point = 5.0;
    snprintf(conflicting_scale.items[1].grade_symbol, sizeof(conflicting_scale.items[1].grade_symbol), "NEW_G");
    conflicting_scale.items[1].grade_point = 3.0;

    status = global_scale_repo->save_scale(global_scale_repo->context, &conflicting_scale);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, status);

    GGGradingScale loaded;
    status = global_scale_repo->load_scale(global_scale_repo->context, &loaded);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(5, loaded.count);
    TEST_ASSERT_EQUAL_STRING("A", loaded.items[0].grade_symbol);
    TEST_ASSERT_EQUAL_STRING("B", loaded.items[1].grade_symbol);
    TEST_ASSERT_EQUAL_STRING("C", loaded.items[2].grade_symbol);
    TEST_ASSERT_EQUAL_STRING("D", loaded.items[3].grade_symbol);
    TEST_ASSERT_EQUAL_STRING("F", loaded.items[4].grade_symbol);
}

static void test_scale_repository_duplicate_symbols_handling(void) {
    GGGradingScale dup_scale;
    memset(&dup_scale, 0, sizeof(dup_scale));
    dup_scale.count = 3;
    dup_scale.max_point = 5.0;
    dup_scale.min_point = 3.0;
    snprintf(dup_scale.items[0].grade_symbol, sizeof(dup_scale.items[0].grade_symbol), "A");
    dup_scale.items[0].grade_point = 5.0;
    snprintf(dup_scale.items[1].grade_symbol, sizeof(dup_scale.items[1].grade_symbol), "A");
    dup_scale.items[1].grade_point = 4.5;
    snprintf(dup_scale.items[2].grade_symbol, sizeof(dup_scale.items[2].grade_symbol), "B");
    dup_scale.items[2].grade_point = 3.0;

    GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &dup_scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale loaded;
    status = global_scale_repo->load_scale(global_scale_repo->context, &loaded);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(2, loaded.count);
    TEST_ASSERT_EQUAL_STRING("A", loaded.items[0].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.5, loaded.items[0].grade_point);
    TEST_ASSERT_EQUAL_STRING("B", loaded.items[1].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, loaded.items[1].grade_point);
}

static void test_scale_repository_unsorted_input_ordering(void) {
    GGGradingScale unsorted_scale;
    memset(&unsorted_scale, 0, sizeof(unsorted_scale));
    unsorted_scale.count = 4;
    unsorted_scale.max_point = 5.0;
    unsorted_scale.min_point = 1.0;
    snprintf(unsorted_scale.items[0].grade_symbol, sizeof(unsorted_scale.items[0].grade_symbol), "D");
    unsorted_scale.items[0].grade_point = 1.0;
    snprintf(unsorted_scale.items[1].grade_symbol, sizeof(unsorted_scale.items[1].grade_symbol), "A");
    unsorted_scale.items[1].grade_point = 5.0;
    snprintf(unsorted_scale.items[2].grade_symbol, sizeof(unsorted_scale.items[2].grade_symbol), "C");
    unsorted_scale.items[2].grade_point = 2.0;
    snprintf(unsorted_scale.items[3].grade_symbol, sizeof(unsorted_scale.items[3].grade_symbol), "B");
    unsorted_scale.items[3].grade_point = 4.0;

    GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &unsorted_scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale loaded;
    status = global_scale_repo->load_scale(global_scale_repo->context, &loaded);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(4, loaded.count);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, loaded.max_point);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, loaded.min_point);

    TEST_ASSERT_EQUAL_STRING("A", loaded.items[0].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, loaded.items[0].grade_point);
    TEST_ASSERT_EQUAL_STRING("B", loaded.items[1].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, loaded.items[1].grade_point);
    TEST_ASSERT_EQUAL_STRING("C", loaded.items[2].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, loaded.items[2].grade_point);
    TEST_ASSERT_EQUAL_STRING("D", loaded.items[3].grade_symbol);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, loaded.items[3].grade_point);
}

static void test_scale_repository_100_rapid_updates(void) {
    for (int i = 0; i < 100; ++i) {
        GGGradingScale s;
        memset(&s, 0, sizeof(s));
        s.count = 3;
        s.max_point = 5.0 + (double)i * 0.01;
        s.min_point = 1.0;
        snprintf(s.items[0].grade_symbol, sizeof(s.items[0].grade_symbol), "A");
        s.items[0].grade_point = 5.0 + (double)i * 0.01;
        snprintf(s.items[1].grade_symbol, sizeof(s.items[1].grade_symbol), "B");
        s.items[1].grade_point = 3.0;
        snprintf(s.items[2].grade_symbol, sizeof(s.items[2].grade_symbol), "C");
        s.items[2].grade_point = 1.0;

        GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &s);
        TEST_ASSERT_EQUAL(GG_OK, status);

        GGGradingScale loaded;
        status = global_scale_repo->load_scale(global_scale_repo->context, &loaded);
        TEST_ASSERT_EQUAL(GG_OK, status);
        TEST_ASSERT_EQUAL_UINT(3, loaded.count);
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, s.items[0].grade_point, loaded.max_point);
    }
}

static void test_bulk_100_courses_transaction_and_live_totals(void) {
    GGGradingScale scale = make_standard_scale();
    GGStatus status = global_scale_repo->save_scale(global_scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = gg_db_begin_immediate(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    double expected_tcp = 0.0;
    uint32_t expected_tcu = 0;
    int64_t ids[100];

    for (int i = 0; i < 100; ++i) {
        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem %d", (i % 8) + 1);
        snprintf(entry.course_label, sizeof(entry.course_label), "CRS%03d", i);
        entry.credit_unit = (uint32_t)((i % 4) + 1);
        if (i % 3 == 0) {
            snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
            expected_tcp += (double)entry.credit_unit * 5.0;
        } else if (i % 3 == 1) {
            snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "B");
            expected_tcp += (double)entry.credit_unit * 4.0;
        } else {
            snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "C");
            expected_tcp += (double)entry.credit_unit * 3.0;
        }
        expected_tcu += entry.credit_unit;
        entry.entry_date = 10000 + i;

        int64_t out_id = 0;
        status = global_course_repo->insert_course(global_course_repo->context, &entry, &out_id);
        TEST_ASSERT_EQUAL(GG_OK, status);
        ids[i] = out_id;
    }

    status = gg_db_commit(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    double live_tcp = 0.0;
    uint32_t live_tcu = 0;
    status = global_course_repo->get_live_totals(global_course_repo->context, &live_tcp, &live_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(expected_tcu, live_tcu);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, expected_tcp, live_tcp);

    GGCourseList *list = NULL;
    status = global_course_repo->list_all_courses(global_course_repo->context, &list);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT(100, list->count);
    gg_course_list_destroy(list);

    status = gg_db_begin_immediate(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    for (int i = 0; i < 100; ++i) {
        status = global_course_repo->delete_course(global_course_repo->context, ids[i]);
        TEST_ASSERT_EQUAL(GG_OK, status);
    }

    status = gg_db_commit(global_conn);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = global_course_repo->get_live_totals(global_course_repo->context, &live_tcp, &live_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT(0, live_tcu);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, live_tcp);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_pragma_verification_memory);
    RUN_TEST(test_pragma_verification_disk);
    RUN_TEST(test_transaction_state_transitions);
    RUN_TEST(test_transaction_stress_50_sequential);
    RUN_TEST(test_transaction_rollback_complex_mutation);
    RUN_TEST(test_transaction_aborted_by_constraint);
    RUN_TEST(test_concurrency_wal_snapshot_isolation);
    RUN_TEST(test_disconnect_rolls_back_uncommitted);
    RUN_TEST(test_scale_repository_16_items_max_capacity);
    RUN_TEST(test_scale_repository_single_item_boundary);
    RUN_TEST(test_scale_repository_complete_disjoint_replacement);
    RUN_TEST(test_scale_repository_atomic_rollback_on_restrict);
    RUN_TEST(test_scale_repository_duplicate_symbols_handling);
    RUN_TEST(test_scale_repository_unsorted_input_ordering);
    RUN_TEST(test_scale_repository_100_rapid_updates);
    RUN_TEST(test_bulk_100_courses_transaction_and_live_totals);

    return UNITY_END();
}
