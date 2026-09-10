#ifndef GG_COURSE_REPOSITORY_H
#define GG_COURSE_REPOSITORY_H

#include "gg_types.h"

typedef struct GGCourseRepository GGCourseRepository;

struct GGCourseRepository {
    void *context;
    GGStatus (*insert_course)(void *context, const GGCourseEntry *entry, int64_t *out_id);
    GGStatus (*update_course)(void *context, const GGCourseEntry *entry);
    GGStatus (*delete_course)(void *context, int64_t id);
    GGStatus (*get_course_by_id)(void *context, int64_t id, GGCourseEntry *out_entry);
    GGStatus (*list_all_courses)(void *context, GGCourseList **out_list);
    GGStatus (*list_courses_by_semester)(void *context, const char *semester_label, GGCourseList **out_list);
    GGStatus (*get_live_totals)(void *context, double *out_tcp, uint32_t *out_tcu);
    void (*destroy)(GGCourseRepository *repo);
};

#endif
