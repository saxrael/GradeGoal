#include "unity.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

static GGGradingScale create_nominal_scale(void) {
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

static uint32_t pseudo_rand(uint32_t *state) {
    *state = (*state * 1103515245U + 12345U) & 0x7FFFFFFFU;
    return *state;
}

typedef struct {
    int64_t id;
    uint32_t credit_unit;
    double grade_point;
    char grade_symbol[8];
    char semester_label[32];
    char course_label[32];
} TrackedCourse;

static void test_massive_mutations_1200_courses_live_drift(void) {
    GGGradingScale scale = create_nominal_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    const size_t total_courses = 1200;
    TrackedCourse *tracked = (TrackedCourse *)calloc(total_courses, sizeof(TrackedCourse));
    TEST_ASSERT_NOT_NULL(tracked);

    const char *semesters[] = {
        "Year 1 Fall", "Year 1 Spring",
        "Year 2 Fall", "Year 2 Spring",
        "Year 3 Fall", "Year 3 Spring",
        "Year 4 Fall", "Year 4 Spring",
        "Year 5 Fall", "Year 5 Spring"
    };

    uint32_t rng = 424242;
    double expected_tcp = 0.0;
    uint32_t expected_tcu = 0;

    for (size_t i = 0; i < total_courses; i++) {
        uint32_t sem_idx = pseudo_rand(&rng) % 10;
        uint32_t grade_idx = pseudo_rand(&rng) % 6;
        uint32_t credits = (pseudo_rand(&rng) % 6) + 1;

        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.semester_label, sizeof(entry.semester_label), "%s", semesters[sem_idx]);
        snprintf(entry.course_label, sizeof(entry.course_label), "CRS_%04zu", i);
        entry.credit_unit = credits;
        snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "%s", scale.items[grade_idx].grade_symbol);
        entry.entry_date = (int64_t)(1700000000 + i);

        int64_t out_id = 0;
        status = course_repo->insert_course(course_repo->context, &entry, &out_id);
        TEST_ASSERT_EQUAL(GG_OK, status);
        TEST_ASSERT_TRUE(out_id > 0);

        tracked[i].id = out_id;
        tracked[i].credit_unit = credits;
        tracked[i].grade_point = scale.items[grade_idx].grade_point;
        snprintf(tracked[i].grade_symbol, sizeof(tracked[i].grade_symbol), "%s", scale.items[grade_idx].grade_symbol);
        snprintf(tracked[i].semester_label, sizeof(tracked[i].semester_label), "%s", entry.semester_label);
        snprintf(tracked[i].course_label, sizeof(tracked[i].course_label), "%s", entry.course_label);

        expected_tcp += (double)credits * scale.items[grade_idx].grade_point;
        expected_tcu += credits;
    }

    double live_tcp = 0.0;
    uint32_t live_tcu = 0;
    status = course_repo->get_live_totals(course_repo->context, &live_tcp, &live_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, expected_tcp, live_tcp);
    TEST_ASSERT_EQUAL_UINT32(expected_tcu, live_tcu);

    const size_t update_count = 400;
    for (size_t u = 0; u < update_count; u++) {
        size_t idx = (size_t)(pseudo_rand(&rng) % total_courses);
        uint32_t new_grade_idx = pseudo_rand(&rng) % 6;
        uint32_t new_credits = (pseudo_rand(&rng) % 6) + 1;

        expected_tcp -= (double)tracked[idx].credit_unit * tracked[idx].grade_point;
        expected_tcu -= tracked[idx].credit_unit;

        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.id = tracked[idx].id;
        snprintf(entry.semester_label, sizeof(entry.semester_label), "%s", tracked[idx].semester_label);
        snprintf(entry.course_label, sizeof(entry.course_label), "UP_CRS_%04zu", idx);
        entry.credit_unit = new_credits;
        snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "%s", scale.items[new_grade_idx].grade_symbol);
        entry.entry_date = (int64_t)(1710000000 + u);

        status = course_repo->update_course(course_repo->context, &entry);
        TEST_ASSERT_EQUAL(GG_OK, status);

        tracked[idx].credit_unit = new_credits;
        tracked[idx].grade_point = scale.items[new_grade_idx].grade_point;
        snprintf(tracked[idx].grade_symbol, sizeof(tracked[idx].grade_symbol), "%s", entry.grade_symbol);
        snprintf(tracked[idx].course_label, sizeof(tracked[idx].course_label), "%s", entry.course_label);

        expected_tcp += (double)new_credits * scale.items[new_grade_idx].grade_point;
        expected_tcu += new_credits;
    }

    status = course_repo->get_live_totals(course_repo->context, &live_tcp, &live_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, expected_tcp, live_tcp);
    TEST_ASSERT_EQUAL_UINT32(expected_tcu, live_tcu);

    const size_t delete_count = 350;
    size_t deleted_so_far = 0;
    for (size_t d = 0; d < delete_count && deleted_so_far < total_courses; d++) {
        size_t idx = (size_t)(pseudo_rand(&rng) % total_courses);
        while (tracked[idx].id == 0) {
            idx = (idx + 1) % total_courses;
        }

        expected_tcp -= (double)tracked[idx].credit_unit * tracked[idx].grade_point;
        expected_tcu -= tracked[idx].credit_unit;

        status = course_repo->delete_course(course_repo->context, tracked[idx].id);
        TEST_ASSERT_EQUAL(GG_OK, status);

        tracked[idx].id = 0;
        deleted_so_far++;
    }

    status = course_repo->get_live_totals(course_repo->context, &live_tcp, &live_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, expected_tcp, live_tcp);
    TEST_ASSERT_EQUAL_UINT32(expected_tcu, live_tcu);

    GGCourseList *list = NULL;
    status = course_repo->list_all_courses(course_repo->context, &list);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_NOT_NULL(list);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(total_courses - delete_count), (uint32_t)list->count);

    double list_tcp = 0.0;
    uint32_t list_tcu = 0;
    for (size_t i = 0; i < list->count; i++) {
        double pt = 0.0;
        for (size_t g = 0; g < scale.count; g++) {
            if (strcmp(list->entries[i].grade_symbol, scale.items[g].grade_symbol) == 0) {
                pt = scale.items[g].grade_point;
                break;
            }
        }
        list_tcp += (double)list->entries[i].credit_unit * pt;
        list_tcu += list->entries[i].credit_unit;
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, expected_tcp, list_tcp);
    TEST_ASSERT_EQUAL_UINT32(expected_tcu, list_tcu);
    gg_course_list_destroy(list);

    size_t total_by_sem = 0;
    for (size_t s = 0; s < 10; s++) {
        GGCourseList *sem_list = NULL;
        status = course_repo->list_courses_by_semester(course_repo->context, semesters[s], &sem_list);
        TEST_ASSERT_EQUAL(GG_OK, status);
        TEST_ASSERT_NOT_NULL(sem_list);
        total_by_sem += sem_list->count;
        gg_course_list_destroy(sem_list);
    }
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(total_courses - delete_count), (uint32_t)total_by_sem);

    for (size_t i = 0; i < total_courses; i++) {
        if (tracked[i].id != 0) {
            status = course_repo->delete_course(course_repo->context, tracked[i].id);
            TEST_ASSERT_EQUAL(GG_OK, status);
            tracked[i].id = 0;
        }
    }

    status = course_repo->get_live_totals(course_repo->context, &live_tcp, &live_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, live_tcp);
    TEST_ASSERT_EQUAL_UINT32(0, live_tcu);

    free(tracked);
}

static void test_fk_delete_restrict_with_150_courses(void) {
    GGGradingScale scale = create_nominal_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    const size_t ref_count = 150;
    int64_t ids[150];
    for (size_t i = 0; i < ref_count; i++) {
        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem 1");
        snprintf(entry.course_label, sizeof(entry.course_label), "A_REF_%zu", i);
        entry.credit_unit = 3;
        snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
        entry.entry_date = (int64_t)(1000 + i);

        int64_t id = 0;
        status = course_repo->insert_course(course_repo->context, &entry, &id);
        TEST_ASSERT_EQUAL(GG_OK, status);
        ids[i] = id;
    }

    for (size_t i = 0; i < 50; i++) {
        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem 1");
        snprintf(entry.course_label, sizeof(entry.course_label), "B_REF_%zu", i);
        entry.credit_unit = 2;
        snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "B");
        entry.entry_date = (int64_t)(2000 + i);

        int64_t id = 0;
        status = course_repo->insert_course(course_repo->context, &entry, &id);
        TEST_ASSERT_EQUAL(GG_OK, status);
    }

    GGGradingScale scale_without_a;
    memset(&scale_without_a, 0, sizeof(scale_without_a));
    scale_without_a.count = 5;
    scale_without_a.max_point = 4.0;
    scale_without_a.min_point = 0.0;
    for (size_t i = 0; i < 5; i++) {
        snprintf(scale_without_a.items[i].grade_symbol, sizeof(scale_without_a.items[i].grade_symbol), "%s", scale.items[i + 1].grade_symbol);
        scale_without_a.items[i].grade_point = scale.items[i + 1].grade_point;
    }

    status = scale_repo->save_scale(scale_repo->context, &scale_without_a);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, status);

    sqlite3 *db = gg_db_get_handle(db_conn);
    TEST_ASSERT_NOT_NULL(db);
    int rc = sqlite3_exec(db, "DELETE FROM scale WHERE grade_symbol = 'A';", NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(SQLITE_CONSTRAINT, rc & 0xFF);

    GGGradingScale scale_without_f = scale;
    scale_without_f.count = 5;
    scale_without_f.min_point = 1.0;
    status = scale_repo->save_scale(scale_repo->context, &scale_without_f);
    TEST_ASSERT_EQUAL(GG_OK, status);

    for (size_t i = 0; i < ref_count; i++) {
        status = course_repo->delete_course(course_repo->context, ids[i]);
        TEST_ASSERT_EQUAL(GG_OK, status);
    }

    GGGradingScale scale_only_b;
    memset(&scale_only_b, 0, sizeof(scale_only_b));
    scale_only_b.count = 1;
    scale_only_b.max_point = 4.0;
    scale_only_b.min_point = 4.0;
    snprintf(scale_only_b.items[0].grade_symbol, sizeof(scale_only_b.items[0].grade_symbol), "B");
    scale_only_b.items[0].grade_point = 4.0;

    status = scale_repo->save_scale(scale_repo->context, &scale_only_b);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGGradingScale loaded;
    status = scale_repo->load_scale(scale_repo->context, &loaded);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)loaded.count);
    TEST_ASSERT_EQUAL_STRING("B", loaded.items[0].grade_symbol);
}

static void test_fk_cascade_update_symbol_renaming_200_courses(void) {
    GGGradingScale scale = create_nominal_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    const size_t a_count = 200;
    const size_t b_count = 100;
    int64_t a_ids[200];

    for (size_t i = 0; i < a_count; i++) {
        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem A");
        snprintf(entry.course_label, sizeof(entry.course_label), "A_CRS_%zu", i);
        entry.credit_unit = 3;
        snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
        entry.entry_date = (int64_t)(1000 + i);

        int64_t id = 0;
        status = course_repo->insert_course(course_repo->context, &entry, &id);
        TEST_ASSERT_EQUAL(GG_OK, status);
        a_ids[i] = id;
    }

    for (size_t i = 0; i < b_count; i++) {
        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem B");
        snprintf(entry.course_label, sizeof(entry.course_label), "B_CRS_%zu", i);
        entry.credit_unit = 2;
        snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "B");
        entry.entry_date = (int64_t)(2000 + i);

        int64_t id = 0;
        status = course_repo->insert_course(course_repo->context, &entry, &id);
        TEST_ASSERT_EQUAL(GG_OK, status);
    }

    double tcp_before = 0.0;
    uint32_t tcu_before = 0;
    status = course_repo->get_live_totals(course_repo->context, &tcp_before, &tcu_before);
    TEST_ASSERT_EQUAL(GG_OK, status);
    double expected_initial_tcp = (200.0 * 3.0 * 5.0) + (100.0 * 2.0 * 4.0);
    uint32_t expected_initial_tcu = (200 * 3) + (100 * 2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, expected_initial_tcp, tcp_before);
    TEST_ASSERT_EQUAL_UINT32(expected_initial_tcu, tcu_before);

    sqlite3 *db = gg_db_get_handle(db_conn);
    TEST_ASSERT_NOT_NULL(db);
    int rc = sqlite3_exec(db, "UPDATE scale SET grade_symbol = 'EXCELL' WHERE grade_symbol = 'A';", NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);

    for (size_t i = 0; i < 10; i++) {
        GGCourseEntry check_entry;
        status = course_repo->get_course_by_id(course_repo->context, a_ids[i], &check_entry);
        TEST_ASSERT_EQUAL(GG_OK, status);
        TEST_ASSERT_EQUAL_STRING("EXCELL", check_entry.grade_symbol);
    }

    double tcp_after = 0.0;
    uint32_t tcu_after = 0;
    status = course_repo->get_live_totals(course_repo->context, &tcp_after, &tcu_after);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, expected_initial_tcp, tcp_after);
    TEST_ASSERT_EQUAL_UINT32(expected_initial_tcu, tcu_after);

    GGCourseEntry old_a_entry;
    memset(&old_a_entry, 0, sizeof(old_a_entry));
    snprintf(old_a_entry.semester_label, sizeof(old_a_entry.semester_label), "Sem X");
    snprintf(old_a_entry.course_label, sizeof(old_a_entry.course_label), "TEST_A");
    old_a_entry.credit_unit = 3;
    snprintf(old_a_entry.grade_symbol, sizeof(old_a_entry.grade_symbol), "A");
    int64_t dummy_id = 0;
    status = course_repo->insert_course(course_repo->context, &old_a_entry, &dummy_id);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, status);

    GGCourseEntry new_sym_entry;
    memset(&new_sym_entry, 0, sizeof(new_sym_entry));
    snprintf(new_sym_entry.semester_label, sizeof(new_sym_entry.semester_label), "Sem X");
    snprintf(new_sym_entry.course_label, sizeof(new_sym_entry.course_label), "TEST_EXCELL");
    new_sym_entry.credit_unit = 4;
    snprintf(new_sym_entry.grade_symbol, sizeof(new_sym_entry.grade_symbol), "EXCELL");
    status = course_repo->insert_course(course_repo->context, &new_sym_entry, &dummy_id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    rc = sqlite3_exec(db, "UPDATE scale SET grade_symbol = 'A' WHERE grade_symbol = 'EXCELL';", NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(SQLITE_OK, rc);

    status = course_repo->get_course_by_id(course_repo->context, dummy_id, &new_sym_entry);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_STRING("A", new_sym_entry.grade_symbol);
}

static void test_boundary_credit_unit_zero_and_negative(void) {
    GGGradingScale scale = create_nominal_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "CS101");
    entry.credit_unit = 0;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");

    int64_t out_id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &out_id);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, status);

    entry.credit_unit = 3;
    status = course_repo->insert_course(course_repo->context, &entry, &out_id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    entry.id = out_id;
    entry.credit_unit = 0;
    status = course_repo->update_course(course_repo->context, &entry);
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, status);

    sqlite3 *db = gg_db_get_handle(db_conn);
    TEST_ASSERT_NOT_NULL(db);
    int rc = sqlite3_exec(db, "INSERT INTO course_entries (semester_label, credit_unit, grade_symbol, entry_date) VALUES ('Y1', 0, 'A', 100);", NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(SQLITE_CONSTRAINT, rc & 0xFF);

    rc = sqlite3_exec(db, "INSERT INTO course_entries (semester_label, credit_unit, grade_symbol, entry_date) VALUES ('Y1', -5, 'A', 100);", NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(SQLITE_CONSTRAINT, rc & 0xFF);
}

static void test_boundary_unlisted_grade_symbols(void) {
    GGGradingScale scale = create_nominal_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Year 1");
    entry.credit_unit = 3;

    int64_t id = 0;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "Z");
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, course_repo->insert_course(course_repo->context, &entry, &id));

    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "a");
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, course_repo->insert_course(course_repo->context, &entry, &id));

    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A ");
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, course_repo->insert_course(course_repo->context, &entry, &id));

    entry.grade_symbol[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, course_repo->insert_course(course_repo->context, &entry, &id));

    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    status = course_repo->insert_course(course_repo->context, &entry, &id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    entry.id = id;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "XYZ");
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, course_repo->update_course(course_repo->context, &entry));
}

static void test_boundary_labels_31_and_32_chars(void) {
    GGGradingScale scale = create_nominal_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    memset(entry.semester_label, 'S', 31);
    entry.semester_label[31] = '\0';
    memset(entry.course_label, 'C', 31);
    entry.course_label[31] = '\0';
    entry.credit_unit = 4;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    entry.entry_date = 99999;

    int64_t id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry retrieved;
    status = course_repo->get_course_by_id(course_repo->context, id, &retrieved);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_STRING(entry.semester_label, retrieved.semester_label);
    TEST_ASSERT_EQUAL_STRING(entry.course_label, retrieved.course_label);
    TEST_ASSERT_EQUAL_UINT32(4, retrieved.credit_unit);
    TEST_ASSERT_EQUAL_STRING("A", retrieved.grade_symbol);

    entry.semester_label[0] = '\0';
    TEST_ASSERT_EQUAL(GG_ERR_VALIDATION, course_repo->insert_course(course_repo->context, &entry, &id));

    snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem X");
    entry.course_label[0] = '\0';
    status = course_repo->insert_course(course_repo->context, &entry, &id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = course_repo->get_course_by_id(course_repo->context, id, &retrieved);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_STRING("", retrieved.course_label);

    snprintf(entry.course_label, sizeof(entry.course_label), "Robert'); DROP TABLE scale;--");
    status = course_repo->insert_course(course_repo->context, &entry, &id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = course_repo->get_course_by_id(course_repo->context, id, &retrieved);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_STRING("Robert'); DROP TABLE scale;--", retrieved.course_label);

    GGGradingScale loaded;
    status = scale_repo->load_scale(scale_repo->context, &loaded);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_UINT32(6, (uint32_t)loaded.count);
}

static void test_boundary_seven_char_grade_symbol(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 2;
    scale.max_point = 4.5;
    scale.min_point = 0.0;
    snprintf(scale.items[0].grade_symbol, sizeof(scale.items[0].grade_symbol), "ABCDEFG");
    scale.items[0].grade_point = 4.5;
    snprintf(scale.items[1].grade_symbol, sizeof(scale.items[1].grade_symbol), "FAILING");
    scale.items[1].grade_point = 0.0;

    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Semester 1");
    snprintf(entry.course_label, sizeof(entry.course_label), "CS700");
    entry.credit_unit = 4;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "ABCDEFG");

    int64_t id = 0;
    status = course_repo->insert_course(course_repo->context, &entry, &id);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry retrieved;
    status = course_repo->get_course_by_id(course_repo->context, id, &retrieved);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_EQUAL_STRING("ABCDEFG", retrieved.grade_symbol);

    double tcp = 0.0;
    uint32_t tcu = 0;
    status = course_repo->get_live_totals(course_repo->context, &tcp, &tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 18.0, tcp);
    TEST_ASSERT_EQUAL_UINT32(4, tcu);
}

static void test_boundary_course_ids_and_not_found(void) {
    GGGradingScale scale = create_nominal_scale();
    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    GGCourseEntry retrieved;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_course_by_id(course_repo->context, 0, &retrieved));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_course_by_id(course_repo->context, -1, &retrieved));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_course_by_id(course_repo->context, -999999, &retrieved));
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, course_repo->get_course_by_id(course_repo->context, 888888, &retrieved));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->delete_course(course_repo->context, 0));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->delete_course(course_repo->context, -1));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->delete_course(course_repo->context, -12345));
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, course_repo->delete_course(course_repo->context, 888888));

    GGCourseEntry dummy;
    memset(&dummy, 0, sizeof(dummy));
    snprintf(dummy.semester_label, sizeof(dummy.semester_label), "Sem 1");
    dummy.credit_unit = 3;
    snprintf(dummy.grade_symbol, sizeof(dummy.grade_symbol), "A");

    dummy.id = 0;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->update_course(course_repo->context, &dummy));
    dummy.id = -1;
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->update_course(course_repo->context, &dummy));
    dummy.id = 888888;
    TEST_ASSERT_EQUAL(GG_ERR_NOT_FOUND, course_repo->update_course(course_repo->context, &dummy));
}

static void test_boundary_null_arguments_all_functions(void) {
    GGCourseEntry entry;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem 1");
    entry.credit_unit = 3;
    snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "A");
    int64_t id = 0;
    GGCourseList *list = NULL;
    double tcp = 0.0;
    uint32_t tcu = 0;

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->insert_course(NULL, &entry, &id));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->insert_course(course_repo->context, NULL, &id));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->insert_course(course_repo->context, &entry, NULL));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->update_course(NULL, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->update_course(course_repo->context, NULL));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->delete_course(NULL, 1));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_course_by_id(NULL, 1, &entry));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_course_by_id(course_repo->context, 1, NULL));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->list_all_courses(NULL, &list));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->list_all_courses(course_repo->context, NULL));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->list_courses_by_semester(NULL, "Sem 1", &list));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->list_courses_by_semester(course_repo->context, NULL, &list));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->list_courses_by_semester(course_repo->context, "Sem 1", NULL));

    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_live_totals(NULL, &tcp, &tcu));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_live_totals(course_repo->context, NULL, &tcu));
    TEST_ASSERT_EQUAL(GG_ERR_INVALID_ARG, course_repo->get_live_totals(course_repo->context, &tcp, NULL));
}

static void test_large_scale_16_grades_and_point_mutations(void) {
    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    scale.count = 16;
    scale.max_point = 15.0;
    scale.min_point = 0.0;

    for (size_t i = 0; i < 16; i++) {
        snprintf(scale.items[i].grade_symbol, sizeof(scale.items[i].grade_symbol), "G%zu", i);
        scale.items[i].grade_point = (double)(15 - i);
    }

    GGStatus status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    double expected_tcp = 0.0;
    uint32_t expected_tcu = 0;

    for (size_t i = 0; i < 16; i++) {
        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        snprintf(entry.semester_label, sizeof(entry.semester_label), "Sem 16");
        snprintf(entry.course_label, sizeof(entry.course_label), "CRS_%zu", i);
        entry.credit_unit = (uint32_t)(i + 1);
        snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "%s", scale.items[i].grade_symbol);

        int64_t id = 0;
        status = course_repo->insert_course(course_repo->context, &entry, &id);
        TEST_ASSERT_EQUAL(GG_OK, status);

        expected_tcp += (double)entry.credit_unit * scale.items[i].grade_point;
        expected_tcu += entry.credit_unit;
    }

    double live_tcp = 0.0;
    uint32_t live_tcu = 0;
    status = course_repo->get_live_totals(course_repo->context, &live_tcp, &live_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, expected_tcp, live_tcp);
    TEST_ASSERT_EQUAL_UINT32(expected_tcu, live_tcu);

    scale.items[0].grade_point = 16.0;
    scale.max_point = 16.0;
    expected_tcp += (double)(1) * (16.0 - 15.0);

    scale.items[15].grade_point = 0.5;
    scale.min_point = 0.5;
    expected_tcp += (double)(16) * (0.5 - 0.0);

    status = scale_repo->save_scale(scale_repo->context, &scale);
    TEST_ASSERT_EQUAL(GG_OK, status);

    status = course_repo->get_live_totals(course_repo->context, &live_tcp, &live_tcu);
    TEST_ASSERT_EQUAL(GG_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, expected_tcp, live_tcp);
    TEST_ASSERT_EQUAL_UINT32(expected_tcu, live_tcu);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_massive_mutations_1200_courses_live_drift);
    RUN_TEST(test_fk_delete_restrict_with_150_courses);
    RUN_TEST(test_fk_cascade_update_symbol_renaming_200_courses);
    RUN_TEST(test_boundary_credit_unit_zero_and_negative);
    RUN_TEST(test_boundary_unlisted_grade_symbols);
    RUN_TEST(test_boundary_labels_31_and_32_chars);
    RUN_TEST(test_boundary_seven_char_grade_symbol);
    RUN_TEST(test_boundary_course_ids_and_not_found);
    RUN_TEST(test_boundary_null_arguments_all_functions);
    RUN_TEST(test_large_scale_16_grades_and_point_mutations);

    return UNITY_END();
}
