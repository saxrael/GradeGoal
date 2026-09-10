#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include "scale_repository.h"
#include "sqlite_error.h"

typedef struct {
    GGDbConnection *conn;
} GGSqliteScaleContext;

static GGStatus sqlite_scale_save(void *context, const GGGradingScale *scale);
static GGStatus sqlite_scale_load(void *context, GGGradingScale *out_scale);
static void sqlite_scale_destroy(GGScaleRepository *repo);

static GGStatus sqlite_scale_save(void *context, const GGGradingScale *scale) {
    if (context == NULL || scale == NULL || scale->count == 0 || scale->count > 16) {
        return GG_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < scale->count; ++i) {
        if (scale->items[i].grade_symbol[0] == '\0') {
            return GG_ERR_INVALID_ARG;
        }
        if (scale->items[i].grade_point < 0.0) {
            return GG_ERR_VALIDATION;
        }
    }

    GGSqliteScaleContext *ctx = (GGSqliteScaleContext *)context;
    sqlite3 *db = gg_db_get_handle(ctx->conn);
    if (db == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    GGStatus status = gg_db_begin_immediate(ctx->conn);
    if (status != GG_OK) {
        return status;
    }

    sqlite3_stmt *upsert_stmt = NULL;
    const char *upsert_sql =
        "INSERT INTO scale (grade_symbol, grade_point) VALUES (?, ?)\n"
        "ON CONFLICT(grade_symbol) DO UPDATE SET grade_point = excluded.grade_point;";
    int rc = sqlite3_prepare_v2(db, upsert_sql, -1, &upsert_stmt, NULL);
    if (rc != SQLITE_OK) {
        status = gg_status_from_sqlite(rc);
        gg_db_rollback(ctx->conn);
        return status;
    }

    for (size_t i = 0; i < scale->count; ++i) {
        sqlite3_reset(upsert_stmt);
        sqlite3_clear_bindings(upsert_stmt);
        rc = sqlite3_bind_text(upsert_stmt, 1, scale->items[i].grade_symbol, -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            status = gg_status_from_sqlite(rc);
            sqlite3_finalize(upsert_stmt);
            gg_db_rollback(ctx->conn);
            return status;
        }
        rc = sqlite3_bind_double(upsert_stmt, 2, scale->items[i].grade_point);
        if (rc != SQLITE_OK) {
            status = gg_status_from_sqlite(rc);
            sqlite3_finalize(upsert_stmt);
            gg_db_rollback(ctx->conn);
            return status;
        }
        rc = sqlite3_step(upsert_stmt);
        if (rc != SQLITE_DONE) {
            status = gg_status_from_sqlite(rc);
            sqlite3_finalize(upsert_stmt);
            gg_db_rollback(ctx->conn);
            return status;
        }
    }
    sqlite3_finalize(upsert_stmt);
    upsert_stmt = NULL;

    sqlite3_stmt *select_stmt = NULL;
    sqlite3_stmt *del_stmt = NULL;
    rc = sqlite3_prepare_v2(db, "SELECT grade_symbol FROM scale;", -1, &select_stmt, NULL);
    if (rc != SQLITE_OK) {
        status = gg_status_from_sqlite(rc);
        gg_db_rollback(ctx->conn);
        return status;
    }
    rc = sqlite3_prepare_v2(db, "DELETE FROM scale WHERE grade_symbol = ?;", -1, &del_stmt, NULL);
    if (rc != SQLITE_OK) {
        status = gg_status_from_sqlite(rc);
        sqlite3_finalize(select_stmt);
        gg_db_rollback(ctx->conn);
        return status;
    }

    while ((rc = sqlite3_step(select_stmt)) == SQLITE_ROW) {
        const unsigned char *sym_text = sqlite3_column_text(select_stmt, 0);
        if (sym_text == NULL) {
            continue;
        }
        bool found = false;
        for (size_t i = 0; i < scale->count; ++i) {
            if (strcmp((const char *)sym_text, scale->items[i].grade_symbol) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            sqlite3_reset(del_stmt);
            sqlite3_clear_bindings(del_stmt);
            sqlite3_bind_text(del_stmt, 1, (const char *)sym_text, -1, SQLITE_TRANSIENT);
            int del_rc = sqlite3_step(del_stmt);
            if (del_rc != SQLITE_DONE) {
                status = gg_status_from_sqlite(del_rc);
                sqlite3_finalize(select_stmt);
                sqlite3_finalize(del_stmt);
                gg_db_rollback(ctx->conn);
                return status;
            }
        }
    }
    sqlite3_finalize(select_stmt);
    sqlite3_finalize(del_stmt);

    status = gg_db_commit(ctx->conn);
    return status;
}

static GGStatus sqlite_scale_load(void *context, GGGradingScale *out_scale) {
    if (context == NULL || out_scale == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    GGSqliteScaleContext *ctx = (GGSqliteScaleContext *)context;
    sqlite3 *db = gg_db_get_handle(ctx->conn);
    if (db == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    memset(out_scale, 0, sizeof(*out_scale));

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, "SELECT grade_symbol, grade_point FROM scale ORDER BY grade_point DESC;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }

    size_t idx = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (idx >= 16) {
            sqlite3_finalize(stmt);
            return GG_ERR_VALIDATION;
        }
        const unsigned char *sym = sqlite3_column_text(stmt, 0);
        if (sym == NULL) {
            sqlite3_finalize(stmt);
            return GG_ERR_DB;
        }
        snprintf(out_scale->items[idx].grade_symbol, sizeof(out_scale->items[idx].grade_symbol), "%s", (const char *)sym);
        out_scale->items[idx].grade_point = sqlite3_column_double(stmt, 1);
        idx++;
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        return gg_status_from_sqlite(rc);
    }
    if (idx == 0) {
        return GG_ERR_NOT_FOUND;
    }

    out_scale->count = idx;
    out_scale->max_point = out_scale->items[0].grade_point;
    out_scale->min_point = out_scale->items[idx - 1].grade_point;
    return GG_OK;
}

static void sqlite_scale_destroy(GGScaleRepository *repo) {
    if (repo == NULL) {
        return;
    }
    if (repo->context != NULL) {
        free(repo->context);
        repo->context = NULL;
    }
    free(repo);
}

GGStatus gg_sqlite_scale_repository_create(GGDbConnection *conn, GGScaleRepository **out_repo) {
    if (conn == NULL || out_repo == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    *out_repo = NULL;

    GGSqliteScaleContext *ctx = (GGSqliteScaleContext *)malloc(sizeof(GGSqliteScaleContext));
    if (ctx == NULL) {
        return GG_ERR_NOMEM;
    }
    ctx->conn = conn;

    GGScaleRepository *repo = (GGScaleRepository *)malloc(sizeof(GGScaleRepository));
    if (repo == NULL) {
        free(ctx);
        return GG_ERR_NOMEM;
    }
    repo->context = ctx;
    repo->save_scale = sqlite_scale_save;
    repo->load_scale = sqlite_scale_load;
    repo->destroy = sqlite_scale_destroy;

    *out_repo = repo;
    return GG_OK;
}
