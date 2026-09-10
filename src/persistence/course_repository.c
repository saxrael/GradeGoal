#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sqlite3.h>
#include "course_repository.h"
#include "../core/course_list.h"
#include "sqlite_error.h"

typedef struct {
    GGDbConnection *conn;
} GGSqliteCourseContext;

static GGStatus sqlite_course_insert(void *context, const GGCourseEntry *entry, int64_t *out_id);
static GGStatus sqlite_course_update(void *context, const GGCourseEntry *entry);
static GGStatus sqlite_course_delete(void *context, int64_t id);
static GGStatus sqlite_course_get_by_id(void *context, int64_t id, GGCourseEntry *out_entry);
static GGStatus sqlite_course_list_all(void *context, GGCourseList **out_list);
static GGStatus sqlite_course_list_by_semester(void *context, const char *semester_label, GGCourseList **out_list);
static GGStatus sqlite_course_get_live_totals(void *context, double *out_tcp, uint32_t *out_tcu);
static void sqlite_course_destroy(GGCourseRepository *repo);

static GGStatus sqlite_course_insert(void *context, const GGCourseEntry *entry, int64_t *out_id) {
    if (context == NULL || entry == NULL || out_id == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    if (entry->semester_label[0] == '\0' || entry->credit_unit == 0 || entry->grade_symbol[0] == '\0') {
        return GG_ERR_VALIDATION;
    }
    *out_id = 0;

    GGSqliteCourseContext *ctx = (GGSqliteCourseContext *)context;
    sqlite3 *db = gg_db_get_handle(ctx->conn);
    if (db == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO course_entries (semester_label, course_label, credit_unit, grade_symbol, entry_date)\n"
        "VALUES (?, ?, ?, ?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_text(stmt, 1, entry->semester_label, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    if (entry->course_label[0] != '\0') {
        rc = sqlite3_bind_text(stmt, 2, entry->course_label, -1, SQLITE_TRANSIENT);
    } else {
        rc = sqlite3_bind_null(stmt, 2);
    }
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_int(stmt, 3, (int)entry->credit_unit);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_text(stmt, 4, entry->grade_symbol, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    int64_t ed = entry->entry_date;
    if (ed == 0) {
        ed = (int64_t)time(NULL);
    }
    rc = sqlite3_bind_int64(stmt, 5, (sqlite3_int64)ed);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        GGStatus status = gg_status_from_sqlite(rc);
        sqlite3_finalize(stmt);
        return status;
    }

    *out_id = (int64_t)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return GG_OK;
}

static GGStatus sqlite_course_update(void *context, const GGCourseEntry *entry) {
    if (context == NULL || entry == NULL || entry->id <= 0) {
        return GG_ERR_INVALID_ARG;
    }
    if (entry->semester_label[0] == '\0' || entry->credit_unit == 0 || entry->grade_symbol[0] == '\0') {
        return GG_ERR_VALIDATION;
    }

    GGSqliteCourseContext *ctx = (GGSqliteCourseContext *)context;
    sqlite3 *db = gg_db_get_handle(ctx->conn);
    if (db == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "UPDATE course_entries\n"
        "SET semester_label = ?, course_label = ?, credit_unit = ?, grade_symbol = ?, entry_date = ?\n"
        "WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_text(stmt, 1, entry->semester_label, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    if (entry->course_label[0] != '\0') {
        rc = sqlite3_bind_text(stmt, 2, entry->course_label, -1, SQLITE_TRANSIENT);
    } else {
        rc = sqlite3_bind_null(stmt, 2);
    }
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_int(stmt, 3, (int)entry->credit_unit);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_text(stmt, 4, entry->grade_symbol, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_int64(stmt, 5, (sqlite3_int64)entry->entry_date);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_int64(stmt, 6, (sqlite3_int64)entry->id);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        GGStatus status = gg_status_from_sqlite(rc);
        sqlite3_finalize(stmt);
        return status;
    }

    sqlite3_finalize(stmt);

    if (sqlite3_changes(db) == 0) {
        return GG_ERR_NOT_FOUND;
    }
    return GG_OK;
}

static GGStatus sqlite_course_delete(void *context, int64_t id) {
    if (context == NULL || id <= 0) {
        return GG_ERR_INVALID_ARG;
    }

    GGSqliteCourseContext *ctx = (GGSqliteCourseContext *)context;
    sqlite3 *db = gg_db_get_handle(ctx->conn);
    if (db == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM course_entries WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        GGStatus status = gg_status_from_sqlite(rc);
        sqlite3_finalize(stmt);
        return status;
    }

    sqlite3_finalize(stmt);

    if (sqlite3_changes(db) == 0) {
        return GG_ERR_NOT_FOUND;
    }
    return GG_OK;
}

static GGStatus sqlite_course_get_by_id(void *context, int64_t id, GGCourseEntry *out_entry) {
    if (context == NULL || id <= 0 || out_entry == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    GGSqliteCourseContext *ctx = (GGSqliteCourseContext *)context;
    sqlite3 *db = gg_db_get_handle(ctx->conn);
    if (db == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id, semester_label, course_label, credit_unit, grade_symbol, entry_date\n"
        "FROM course_entries WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_int64(stmt, 1, (sqlite3_int64)id);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return GG_ERR_NOT_FOUND;
    }
    if (rc != SQLITE_ROW) {
        GGStatus status = gg_status_from_sqlite(rc);
        sqlite3_finalize(stmt);
        return status;
    }

    memset(out_entry, 0, sizeof(*out_entry));
    out_entry->id = sqlite3_column_int64(stmt, 0);

    const unsigned char *sem = sqlite3_column_text(stmt, 1);
    if (sem != NULL) {
        snprintf(out_entry->semester_label, sizeof(out_entry->semester_label), "%s", (const char *)sem);
    }

    const unsigned char *course = sqlite3_column_text(stmt, 2);
    if (course != NULL) {
        snprintf(out_entry->course_label, sizeof(out_entry->course_label), "%s", (const char *)course);
    } else {
        out_entry->course_label[0] = '\0';
    }

    out_entry->credit_unit = (uint32_t)sqlite3_column_int(stmt, 3);

    const unsigned char *grade = sqlite3_column_text(stmt, 4);
    if (grade != NULL) {
        snprintf(out_entry->grade_symbol, sizeof(out_entry->grade_symbol), "%s", (const char *)grade);
    }

    out_entry->entry_date = sqlite3_column_int64(stmt, 5);

    sqlite3_finalize(stmt);
    return GG_OK;
}

static GGStatus sqlite_course_list_all(void *context, GGCourseList **out_list) {
    if (context == NULL || out_list == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    *out_list = NULL;

    GGSqliteCourseContext *ctx = (GGSqliteCourseContext *)context;
    sqlite3 *db = gg_db_get_handle(ctx->conn);
    if (db == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    GGStatus status = gg_course_list_create(16, out_list);
    if (status != GG_OK) {
        return status;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id, semester_label, course_label, credit_unit, grade_symbol, entry_date\n"
        "FROM course_entries\n"
        "ORDER BY entry_date ASC, id ASC;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        gg_course_list_destroy(*out_list);
        *out_list = NULL;
        return gg_status_from_sqlite(rc);
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.id = sqlite3_column_int64(stmt, 0);

        const unsigned char *sem = sqlite3_column_text(stmt, 1);
        if (sem != NULL) {
            snprintf(entry.semester_label, sizeof(entry.semester_label), "%s", (const char *)sem);
        }

        const unsigned char *course = sqlite3_column_text(stmt, 2);
        if (course != NULL) {
            snprintf(entry.course_label, sizeof(entry.course_label), "%s", (const char *)course);
        } else {
            entry.course_label[0] = '\0';
        }

        entry.credit_unit = (uint32_t)sqlite3_column_int(stmt, 3);

        const unsigned char *grade = sqlite3_column_text(stmt, 4);
        if (grade != NULL) {
            snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "%s", (const char *)grade);
        }

        entry.entry_date = sqlite3_column_int64(stmt, 5);

        status = gg_course_list_append(*out_list, &entry);
        if (status != GG_OK) {
            sqlite3_finalize(stmt);
            gg_course_list_destroy(*out_list);
            *out_list = NULL;
            return status;
        }
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        gg_course_list_destroy(*out_list);
        *out_list = NULL;
        return gg_status_from_sqlite(rc);
    }

    return GG_OK;
}

static GGStatus sqlite_course_list_by_semester(void *context, const char *semester_label, GGCourseList **out_list) {
    if (context == NULL || semester_label == NULL || out_list == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    *out_list = NULL;

    GGSqliteCourseContext *ctx = (GGSqliteCourseContext *)context;
    sqlite3 *db = gg_db_get_handle(ctx->conn);
    if (db == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    GGStatus status = gg_course_list_create(16, out_list);
    if (status != GG_OK) {
        return status;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id, semester_label, course_label, credit_unit, grade_symbol, entry_date\n"
        "FROM course_entries\n"
        "WHERE semester_label = ?\n"
        "ORDER BY entry_date ASC, id ASC;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        gg_course_list_destroy(*out_list);
        *out_list = NULL;
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_bind_text(stmt, 1, semester_label, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        sqlite3_finalize(stmt);
        gg_course_list_destroy(*out_list);
        *out_list = NULL;
        return gg_status_from_sqlite(rc);
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.id = sqlite3_column_int64(stmt, 0);

        const unsigned char *sem = sqlite3_column_text(stmt, 1);
        if (sem != NULL) {
            snprintf(entry.semester_label, sizeof(entry.semester_label), "%s", (const char *)sem);
        }

        const unsigned char *course = sqlite3_column_text(stmt, 2);
        if (course != NULL) {
            snprintf(entry.course_label, sizeof(entry.course_label), "%s", (const char *)course);
        } else {
            entry.course_label[0] = '\0';
        }

        entry.credit_unit = (uint32_t)sqlite3_column_int(stmt, 3);

        const unsigned char *grade = sqlite3_column_text(stmt, 4);
        if (grade != NULL) {
            snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "%s", (const char *)grade);
        }

        entry.entry_date = sqlite3_column_int64(stmt, 5);

        status = gg_course_list_append(*out_list, &entry);
        if (status != GG_OK) {
            sqlite3_finalize(stmt);
            gg_course_list_destroy(*out_list);
            *out_list = NULL;
            return status;
        }
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        gg_course_list_destroy(*out_list);
        *out_list = NULL;
        return gg_status_from_sqlite(rc);
    }

    return GG_OK;
}

static GGStatus sqlite_course_get_live_totals(void *context, double *out_tcp, uint32_t *out_tcu) {
    if (context == NULL || out_tcp == NULL || out_tcu == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    *out_tcp = 0.0;
    *out_tcu = 0;

    GGSqliteCourseContext *ctx = (GGSqliteCourseContext *)context;
    sqlite3 *db = gg_db_get_handle(ctx->conn);
    if (db == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT COALESCE(SUM(ce.credit_unit * s.grade_point), 0.0),\n"
        "       COALESCE(SUM(ce.credit_unit), 0)\n"
        "FROM course_entries ce\n"
        "JOIN scale s ON ce.grade_symbol = s.grade_symbol;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *out_tcp = sqlite3_column_double(stmt, 0);
        *out_tcu = (uint32_t)sqlite3_column_int64(stmt, 1);
        sqlite3_finalize(stmt);
        return GG_OK;
    }

    GGStatus status = gg_status_from_sqlite(rc);
    sqlite3_finalize(stmt);
    return status;
}

static void sqlite_course_destroy(GGCourseRepository *repo) {
    if (repo == NULL) {
        return;
    }
    if (repo->context != NULL) {
        free(repo->context);
        repo->context = NULL;
    }
    free(repo);
}

GGStatus gg_sqlite_course_repository_create(GGDbConnection *conn, GGCourseRepository **out_repo) {
    if (conn == NULL || out_repo == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    *out_repo = NULL;

    GGSqliteCourseContext *ctx = (GGSqliteCourseContext *)malloc(sizeof(GGSqliteCourseContext));
    if (ctx == NULL) {
        return GG_ERR_NOMEM;
    }
    ctx->conn = conn;

    GGCourseRepository *repo = (GGCourseRepository *)malloc(sizeof(GGCourseRepository));
    if (repo == NULL) {
        free(ctx);
        return GG_ERR_NOMEM;
    }
    repo->context = ctx;
    repo->insert_course = sqlite_course_insert;
    repo->update_course = sqlite_course_update;
    repo->delete_course = sqlite_course_delete;
    repo->get_course_by_id = sqlite_course_get_by_id;
    repo->list_all_courses = sqlite_course_list_all;
    repo->list_courses_by_semester = sqlite_course_list_by_semester;
    repo->get_live_totals = sqlite_course_get_live_totals;
    repo->destroy = sqlite_course_destroy;

    *out_repo = repo;
    return GG_OK;
}
