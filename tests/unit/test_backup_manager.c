#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "../../src/io/backup_manager.h"

#if defined(_WIN32)
#include <windows.h>
#include <direct.h>
#define test_mkdir(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define test_mkdir(p) mkdir(p, 0755)
#endif

static const char *test_db_path = "test_source.db";
static const char *test_backup_dir = "test_backups";

void setUp(void) {
    sqlite3 *db = NULL;
    sqlite3_open(test_db_path, &db);
    sqlite3_exec(db, "CREATE TABLE test (id INTEGER PRIMARY KEY, val TEXT);", NULL, NULL, NULL);
    sqlite3_exec(db, "INSERT INTO test (val) VALUES ('sample');", NULL, NULL, NULL);
    sqlite3_close(db);

    test_mkdir(test_backup_dir);
}

void tearDown(void) {
    remove(test_db_path);

#if defined(_WIN32)
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s\\*.*", test_backup_dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char fpath[512];
                snprintf(fpath, sizeof(fpath), "%s\\%s", test_backup_dir, fd.cFileName);
                remove(fpath);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    _rmdir(test_backup_dir);
#else
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_backup_dir);
    int sys_rc = system(cmd);
    (void)sys_rc;
#endif
}

static void test_backup_null_args(void) {
    TEST_ASSERT_EQUAL_INT(GG_ERR_INVALID_ARG, gg_backup_create_snapshot(NULL, test_backup_dir));
    TEST_ASSERT_EQUAL_INT(GG_ERR_INVALID_ARG, gg_backup_create_snapshot(test_db_path, NULL));
    TEST_ASSERT_EQUAL_INT(GG_ERR_INVALID_ARG, gg_backup_rotate(NULL, 5));
    TEST_ASSERT_EQUAL_INT(GG_ERR_INVALID_ARG, gg_backup_get_last_timestamp(NULL, NULL));
}

static void test_backup_snapshot_creation(void) {
    GGStatus status = gg_backup_create_snapshot(test_db_path, test_backup_dir);
    TEST_ASSERT_EQUAL_INT(GG_OK, status);

    time_t last_time = 0;
    status = gg_backup_get_last_timestamp(test_backup_dir, &last_time);
    TEST_ASSERT_EQUAL_INT(GG_OK, status);
    TEST_ASSERT_TRUE(last_time > 0);
}

static void test_backup_rotation_prunes_to_five(void) {
    for (int i = 0; i < 7; i++) {
        char dummy_path[512];
#if defined(_WIN32)
        snprintf(dummy_path, sizeof(dummy_path), "%s\\GradeGoal_backup_20260910_10000%d.db", test_backup_dir, i);
#else
        snprintf(dummy_path, sizeof(dummy_path), "%s/GradeGoal_backup_20260910_10000%d.db", test_backup_dir, i);
#endif
        FILE *f = fopen(dummy_path, "w");
        if (f != NULL) {
            fputs("dummy", f);
            fclose(f);
        }
    }

    GGStatus status = gg_backup_rotate(test_backup_dir, 5);
    TEST_ASSERT_EQUAL_INT(GG_OK, status);

    time_t last_time = 0;
    status = gg_backup_get_last_timestamp(test_backup_dir, &last_time);
    TEST_ASSERT_EQUAL_INT(GG_OK, status);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_backup_null_args);
    RUN_TEST(test_backup_snapshot_creation);
    RUN_TEST(test_backup_rotation_prunes_to_five);
    return UNITY_END();
}
