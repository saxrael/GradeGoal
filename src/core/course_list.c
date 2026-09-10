#include "course_list.h"
#include <stdlib.h>

GGStatus gg_course_list_create(size_t initial_capacity, GGCourseList **out_list) {
    if (out_list == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    size_t capacity = initial_capacity == 0 ? 8 : initial_capacity;

    GGCourseList *list = (GGCourseList *)calloc(1, sizeof(GGCourseList));
    if (list == NULL) {
        return GG_ERR_NOMEM;
    }

    list->entries = (GGCourseEntry *)calloc(capacity, sizeof(GGCourseEntry));
    if (list->entries == NULL) {
        free(list);
        return GG_ERR_NOMEM;
    }

    list->count = 0;
    list->capacity = capacity;
    *out_list = list;
    return GG_OK;
}

GGStatus gg_course_list_append(GGCourseList *list, const GGCourseEntry *entry) {
    if (list == NULL || entry == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        GGCourseEntry *new_entries = (GGCourseEntry *)realloc(list->entries, new_capacity * sizeof(GGCourseEntry));
        if (new_entries == NULL) {
            return GG_ERR_NOMEM;
        }
        list->entries = new_entries;
        list->capacity = new_capacity;
    }

    list->entries[list->count] = *entry;
    list->count++;
    return GG_OK;
}

void gg_course_list_destroy(GGCourseList *list) {
    if (list == NULL) {
        return;
    }

    if (list->entries != NULL) {
        free(list->entries);
        list->entries = NULL;
    }
    list->count = 0;
    list->capacity = 0;
    free(list);
}
