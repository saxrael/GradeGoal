#include "io_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

GGStatus gg_import_summary_create(GGImportSummary **out_summary) {
    if (out_summary == NULL) {
        return GG_ERR_INVALID_ARG;
    }
    GGImportSummary *summary = (GGImportSummary *)calloc(1, sizeof(GGImportSummary));
    if (summary == NULL) {
        return GG_ERR_NOMEM;
    }
    summary->capacity = 16;
    summary->rejections = (GGImportRejection *)calloc(summary->capacity, sizeof(GGImportRejection));
    if (summary->rejections == NULL) {
        free(summary);
        return GG_ERR_NOMEM;
    }
    *out_summary = summary;
    return GG_OK;
}

GGStatus gg_import_summary_add_rejection(GGImportSummary *summary, size_t row, const char *field, const char *val, const char *reason) {
    if (summary == NULL || field == NULL || reason == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    if (summary->count >= summary->capacity) {
        size_t new_cap = summary->capacity * 2;
        GGImportRejection *new_arr = (GGImportRejection *)realloc(summary->rejections, new_cap * sizeof(GGImportRejection));
        if (new_arr == NULL) {
            return GG_ERR_NOMEM;
        }
        summary->rejections = new_arr;
        summary->capacity = new_cap;
    }

    GGImportRejection *rej = &summary->rejections[summary->count];
    rej->row_number = row;
    snprintf(rej->field_name, sizeof(rej->field_name), "%s", field);
    if (val != NULL) {
        snprintf(rej->rejected_value, sizeof(rej->rejected_value), "%s", val);
    } else {
        rej->rejected_value[0] = '\0';
    }
    snprintf(rej->reason, sizeof(rej->reason), "%s", reason);
    summary->count++;

    return GG_OK;
}

void gg_import_summary_destroy(GGImportSummary *summary) {
    if (summary != NULL) {
        if (summary->rejections != NULL) {
            free(summary->rejections);
        }
        free(summary);
    }
}
