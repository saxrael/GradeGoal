#if !defined(_WIN32)
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#endif
#include "backup_manager.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#define gg_mkdir(path) _mkdir(path)
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define gg_mkdir(path) mkdir(path, 0755)
#endif

typedef struct {
    char filename[512];
} GGBackupEntry;

static time_t gg_timegm(const struct tm *tm) {
    int y = tm->tm_year + 1900;
    int m = tm->tm_mon + 1;
    if (m <= 2) {
        y -= 1;
        m += 12;
    }
    long long days = 365LL * (long long)y + (long long)(y / 4) - (long long)(y / 100) + (long long)(y / 400) +
                     (long long)((153 * (m - 3) + 2) / 5) + (long long)tm->tm_mday - 719469LL;
    long long secs =
        days * 86400LL + (long long)tm->tm_hour * 3600LL + (long long)tm->tm_min * 60LL + (long long)tm->tm_sec;
    return (time_t)secs;
}

static int compare_backup_names(const void *a, const void *b) {
    const GGBackupEntry *entry_a = (const GGBackupEntry *)a;
    const GGBackupEntry *entry_b = (const GGBackupEntry *)b;
    return strcmp(entry_a->filename, entry_b->filename);
}

static size_t list_backup_files(const char *backup_dir, GGBackupEntry *entries, size_t max_entries) {
    size_t count = 0;
#if defined(_WIN32)
    char search_pattern[4096];
    snprintf(search_pattern, sizeof(search_pattern), "%.3000s\\GradeGoal_backup_*.db", backup_dir);
    WIN32_FIND_DATAA find_data;
    HANDLE handle = FindFirstFileA(search_pattern, &find_data);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                if (count < max_entries) {
                    snprintf(entries[count].filename, sizeof(entries[count].filename), "%.511s", find_data.cFileName);
                    count++;
                }
            }
        } while (FindNextFileA(handle, &find_data));
        FindClose(handle);
    }
#else
    DIR *dir = opendir(backup_dir);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strncmp(entry->d_name, "GradeGoal_backup_", 17) == 0) {
                size_t len = strlen(entry->d_name);
                if (len > 3 && strcmp(entry->d_name + len - 3, ".db") == 0) {
                    if (count < max_entries) {
                        snprintf(entries[count].filename, sizeof(entries[count].filename), "%.511s", entry->d_name);
                        count++;
                    }
                }
            }
        }
        closedir(dir);
    }
#endif
    return count;
}

GGStatus gg_backup_create_snapshot(const char *db_filepath, const char *backup_dir) {
    if (db_filepath == NULL || backup_dir == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    gg_mkdir(backup_dir);

    time_t now = time(NULL);
    struct tm tm_info;
#if defined(_WIN32)
    gmtime_s(&tm_info, &now);
#else
    gmtime_r(&now, &tm_info);
#endif

    char target_path[4096];
#if defined(_WIN32)
    snprintf(target_path, sizeof(target_path), "%.3000s\\GradeGoal_backup_%04d%02d%02d_%02d%02d%02d.db", backup_dir,
             tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min,
             tm_info.tm_sec);
#else
    snprintf(target_path, sizeof(target_path), "%.3000s/GradeGoal_backup_%04d%02d%02d_%02d%02d%02d.db", backup_dir,
             tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min,
             tm_info.tm_sec);
#endif

    sqlite3 *src_db = NULL;
    int rc = sqlite3_open_v2(db_filepath, &src_db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        if (src_db != NULL) {
            sqlite3_close(src_db);
        }
        return GG_ERR_IO;
    }

    sqlite3 *dst_db = NULL;
    rc = sqlite3_open_v2(target_path, &dst_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(src_db);
        if (dst_db != NULL) {
            sqlite3_close(dst_db);
        }
        return GG_ERR_IO;
    }

    sqlite3_backup *backup = sqlite3_backup_init(dst_db, "main", src_db, "main");
    if (backup == NULL) {
        sqlite3_close(dst_db);
        sqlite3_close(src_db);
        return GG_ERR_DB;
    }

    rc = sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
    sqlite3_close(dst_db);
    sqlite3_close(src_db);

    if (rc != SQLITE_DONE) {
        return GG_ERR_DB;
    }

    return gg_backup_rotate(backup_dir, 5);
}

GGStatus gg_backup_rotate(const char *backup_dir, size_t max_retained) {
    if (backup_dir == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    GGBackupEntry entries[256];
    size_t count = list_backup_files(backup_dir, entries, 256);
    if (count <= max_retained) {
        return GG_OK;
    }

    qsort(entries, count, sizeof(GGBackupEntry), compare_backup_names);

    size_t files_to_delete = count - max_retained;
    for (size_t i = 0; i < files_to_delete; i++) {
        char full_path[4096];
#if defined(_WIN32)
        snprintf(full_path, sizeof(full_path), "%.3000s\\%.512s", backup_dir, entries[i].filename);
#else
        snprintf(full_path, sizeof(full_path), "%.3000s/%.512s", backup_dir, entries[i].filename);
#endif
        remove(full_path);
    }

    return GG_OK;
}

GGStatus gg_backup_get_last_timestamp(const char *backup_dir, time_t *out_timestamp) {
    if (backup_dir == NULL || out_timestamp == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    *out_timestamp = 0;

    GGBackupEntry entries[256];
    size_t count = list_backup_files(backup_dir, entries, 256);
    if (count == 0) {
        return GG_ERR_NOT_FOUND;
    }

    qsort(entries, count, sizeof(GGBackupEntry), compare_backup_names);

    const char *newest = entries[count - 1].filename;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int parsed =
        sscanf(newest, "GradeGoal_backup_%04d%02d%02d_%02d%02d%02d.db", &year, &month, &day, &hour, &minute, &second);
    if (parsed != 6) {
        return GG_ERR_VALIDATION;
    }

    struct tm tm_info;
    memset(&tm_info, 0, sizeof(tm_info));
    tm_info.tm_year = year - 1900;
    tm_info.tm_mon = month - 1;
    tm_info.tm_mday = day;
    tm_info.tm_hour = hour;
    tm_info.tm_min = minute;
    tm_info.tm_sec = second;
    tm_info.tm_isdst = 0;

    *out_timestamp = gg_timegm(&tm_info);

    return GG_OK;
}
