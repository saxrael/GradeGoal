#ifndef GG_IO_TYPES_H
#define GG_IO_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../core/gg_types.h"

typedef struct {
    size_t row_number;
    char field_name[32];
    char rejected_value[64];
    char reason[128];
} GGImportRejection;

typedef struct {
    GGImportRejection *rejections;
    size_t count;
    size_t capacity;
    size_t total_rows_read;
    size_t valid_rows_imported;
} GGImportSummary;

GGStatus gg_import_summary_create(GGImportSummary **out_summary);
GGStatus gg_import_summary_add_rejection(GGImportSummary *summary, size_t row, const char *field, const char *val, const char *reason);
void gg_import_summary_destroy(GGImportSummary *summary);

#endif
