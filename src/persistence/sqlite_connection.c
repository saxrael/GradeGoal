#include "sqlite_connection.h"
#include "sqlite_error.h"
#include <sqlite3.h>
#include <stdlib.h>

struct GGDbConnection {
    sqlite3 *db;
    bool in_transaction;
};

sqlite3 *gg_db_get_handle(GGDbConnection *conn) {
    if (conn == NULL) {
        return NULL;
    }
    return conn->db;
}

GGStatus gg_db_connect(const char *filepath, GGDbConnection **out_conn) {
    if (filepath == NULL || out_conn == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    *out_conn = NULL;

    GGDbConnection *conn = (GGDbConnection *)malloc(sizeof(GGDbConnection));
    if (conn == NULL) {
        return GG_ERR_NOMEM;
    }
    conn->db = NULL;
    conn->in_transaction = false;

    int rc = sqlite3_open_v2(filepath, &conn->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        GGStatus status = gg_status_from_sqlite(rc);
        if (conn->db != NULL) {
            sqlite3_close_v2(conn->db);
            conn->db = NULL;
        }
        free(conn);
        return status;
    }

    rc = sqlite3_exec(conn->db, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        GGStatus status = gg_status_from_sqlite(rc);
        sqlite3_close_v2(conn->db);
        free(conn);
        return status;
    }

    rc = sqlite3_exec(conn->db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        GGStatus status = gg_status_from_sqlite(rc);
        sqlite3_close_v2(conn->db);
        free(conn);
        return status;
    }

    rc = sqlite3_exec(conn->db, "PRAGMA synchronous = NORMAL;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        GGStatus status = gg_status_from_sqlite(rc);
        sqlite3_close_v2(conn->db);
        free(conn);
        return status;
    }

    rc = sqlite3_busy_timeout(conn->db, 5000);
    if (rc != SQLITE_OK) {
        GGStatus status = gg_status_from_sqlite(rc);
        sqlite3_close_v2(conn->db);
        free(conn);
        return status;
    }

    *out_conn = conn;
    return GG_OK;
}

GGStatus gg_db_disconnect(GGDbConnection *conn) {
    if (conn == NULL) {
        return GG_OK;
    }
    int rc = SQLITE_OK;
    if (conn->db != NULL) {
        if (conn->in_transaction) {
            sqlite3_exec(conn->db, "ROLLBACK;", NULL, NULL, NULL);
            conn->in_transaction = false;
        }
        rc = sqlite3_close_v2(conn->db);
        conn->db = NULL;
    }
    free(conn);
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }
    return GG_OK;
}

GGStatus gg_db_begin_immediate(GGDbConnection *conn) {
    if (conn == NULL || conn->db == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    if (conn->in_transaction) {
        return GG_ERR_DB;
    }
    int rc = sqlite3_exec(conn->db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }
    conn->in_transaction = true;
    return GG_OK;
}

GGStatus gg_db_commit(GGDbConnection *conn) {
    if (conn == NULL || conn->db == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    if (!conn->in_transaction) {
        return GG_ERR_DB;
    }
    int rc = sqlite3_exec(conn->db, "COMMIT;", NULL, NULL, NULL);
    conn->in_transaction = false;
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }
    return GG_OK;
}

GGStatus gg_db_rollback(GGDbConnection *conn) {
    if (conn == NULL || conn->db == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    if (!conn->in_transaction) {
        return GG_OK;
    }
    int rc = sqlite3_exec(conn->db, "ROLLBACK;", NULL, NULL, NULL);
    conn->in_transaction = false;
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }
    return GG_OK;
}

GGStatus gg_db_bootstrap_schema(GGDbConnection *conn) {
    if (conn == NULL || conn->db == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    const char *ddl = "PRAGMA foreign_keys = ON;\n"
                      "CREATE TABLE IF NOT EXISTS scale (\n"
                      "    grade_symbol TEXT PRIMARY KEY NOT NULL,\n"
                      "    grade_point REAL NOT NULL CHECK(grade_point >= 0.0)\n"
                      ");\n"
                      "CREATE TABLE IF NOT EXISTS course_entries (\n"
                      "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
                      "    semester_label TEXT NOT NULL,\n"
                      "    course_label TEXT,\n"
                      "    credit_unit INTEGER NOT NULL CHECK(credit_unit > 0),\n"
                      "    grade_symbol TEXT NOT NULL,\n"
                      "    entry_date INTEGER NOT NULL,\n"
                      "    FOREIGN KEY(grade_symbol) REFERENCES scale(grade_symbol)\n"
                      "        ON UPDATE CASCADE\n"
                      "        ON DELETE RESTRICT\n"
                      ");\n"
                      "CREATE INDEX IF NOT EXISTS idx_course_entries_semester ON course_entries(semester_label);\n"
                      "CREATE INDEX IF NOT EXISTS idx_course_entries_grade ON course_entries(grade_symbol);\n";

    int rc = sqlite3_exec(conn->db, ddl, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        return gg_status_from_sqlite(rc);
    }
    return GG_OK;
}
