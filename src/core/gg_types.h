#ifndef GG_TYPES_H
#define GG_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    GG_OK = 0,
    GG_ERR_INVALID_ARG = 1,
    GG_ERR_VALIDATION = 2,
    GG_ERR_NOMEM = 3,
    GG_ERR_NOT_FOUND = 4,
    GG_ERR_DB = 5,
    GG_ERR_IO = 6,
    GG_ERR_UNREACHABLE = 7
} GGStatus;

typedef struct {
    char grade_symbol[8];
    double grade_point;
} GGGradeItem;

typedef struct {
    GGGradeItem items[16];
    size_t count;
    double max_point;
    double min_point;
} GGGradingScale;

typedef struct {
    int64_t id;
    char semester_label[32];
    char course_label[32];
    uint32_t credit_unit;
    char grade_symbol[8];
    int64_t entry_date;
} GGCourseEntry;

typedef struct {
    GGCourseEntry *entries;
    size_t count;
    size_t capacity;
} GGCourseList;

#endif
