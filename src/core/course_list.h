#ifndef GG_COURSE_LIST_H
#define GG_COURSE_LIST_H

#include "gg_types.h"

GGStatus gg_course_list_create(size_t initial_capacity, GGCourseList **out_list);
GGStatus gg_course_list_append(GGCourseList *list, const GGCourseEntry *entry);
void gg_course_list_destroy(GGCourseList *list);

#endif
