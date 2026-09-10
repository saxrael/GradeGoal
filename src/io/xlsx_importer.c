#include "xlsx_importer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <xlsxio_read.h>
#include "../core/scale_validator.h"

GGStatus gg_xlsx_import(
    const char *filepath,
    GGScaleRepository *scale_repo,
    GGCourseRepository *course_repo,
    bool dry_run,
    GGImportSummary *summary
) {
    if (filepath == NULL || scale_repo == NULL || course_repo == NULL || summary == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    xlsxioreader reader = xlsxioread_open(filepath);
    if (reader == NULL) {
        return GG_ERR_IO;
    }

    bool has_scale = false;
    bool has_history = false;
    xlsxioreadersheetlist sheetlist = xlsxioread_sheetlist_open(reader);
    if (sheetlist != NULL) {
        const char *sheet_name = NULL;
        while ((sheet_name = xlsxioread_sheetlist_next(sheetlist)) != NULL) {
            if (strcmp(sheet_name, "Scale") == 0) {
                has_scale = true;
            } else if (strcmp(sheet_name, "History") == 0) {
                has_history = true;
            }
        }
        xlsxioread_sheetlist_close(sheetlist);
    }

    if (!has_scale || !has_history) {
        xlsxioread_close(reader);
        gg_import_summary_add_rejection(summary, 0, "workbook", filepath, "Missing required Scale or History sheet");
        return GG_ERR_VALIDATION;
    }

    xlsxioreadersheet scale_sheet = xlsxioread_sheet_open(reader, "Scale", XLSXIOREAD_SKIP_EMPTY_ROWS);
    if (scale_sheet == NULL) {
        xlsxioread_close(reader);
        return GG_ERR_IO;
    }

    xlsxioread_sheet_next_row(scale_sheet);

    GGGradingScale temp_scale;
    memset(&temp_scale, 0, sizeof(temp_scale));

    char *val = NULL;
    while (xlsxioread_sheet_next_row(scale_sheet)) {
        if (temp_scale.count >= 16) {
            break;
        }
        if (xlsxioread_sheet_next_cell_string(scale_sheet, &val)) {
            if (val != NULL) {
                snprintf(temp_scale.items[temp_scale.count].grade_symbol,
                         sizeof(temp_scale.items[temp_scale.count].grade_symbol),
                         "%s", val);
                free(val);
                val = NULL;
            }
        }
        if (xlsxioread_sheet_next_cell_string(scale_sheet, &val)) {
            if (val != NULL) {
                temp_scale.items[temp_scale.count].grade_point = strtod(val, NULL);
                free(val);
                val = NULL;
            }
        }
        while (xlsxioread_sheet_next_cell_string(scale_sheet, &val)) {
            if (val != NULL) {
                free(val);
                val = NULL;
            }
        }
        temp_scale.count++;
    }
    xlsxioread_sheet_close(scale_sheet);

    if (temp_scale.count > 0) {
        temp_scale.max_point = temp_scale.items[0].grade_point;
        temp_scale.min_point = temp_scale.items[temp_scale.count - 1].grade_point;
    }

    GGStatus status = gg_scale_validator_validate_scale(&temp_scale);
    if (status != GG_OK) {
        xlsxioread_close(reader);
        gg_import_summary_add_rejection(summary, 0, "Scale", "invalid", "Imported Scale sheet failed validation");
        return GG_ERR_VALIDATION;
    }

    xlsxioreadersheet history_sheet = xlsxioread_sheet_open(reader, "History", XLSXIOREAD_SKIP_EMPTY_ROWS);
    if (history_sheet == NULL) {
        xlsxioread_close(reader);
        return GG_ERR_IO;
    }

    xlsxioread_sheet_next_row(history_sheet);

    if (!dry_run) {
        status = scale_repo->save_scale(scale_repo->context, &temp_scale);
        if (status != GG_OK) {
            xlsxioread_sheet_close(history_sheet);
            xlsxioread_close(reader);
            return status;
        }
    }

    size_t row_number = 1;
    while (xlsxioread_sheet_next_row(history_sheet)) {
        row_number++;
        summary->total_rows_read++;

        GGCourseEntry entry;
        memset(&entry, 0, sizeof(entry));

        if (xlsxioread_sheet_next_cell_string(history_sheet, &val)) {
            if (val != NULL) {
                snprintf(entry.semester_label, sizeof(entry.semester_label), "%s", val);
                free(val);
                val = NULL;
            }
        }
        if (xlsxioread_sheet_next_cell_string(history_sheet, &val)) {
            if (val != NULL) {
                snprintf(entry.course_label, sizeof(entry.course_label), "%s", val);
                free(val);
                val = NULL;
            }
        }
        if (xlsxioread_sheet_next_cell_string(history_sheet, &val)) {
            if (val != NULL) {
                entry.credit_unit = (uint32_t)strtoul(val, NULL, 10);
                free(val);
                val = NULL;
            }
        }
        if (xlsxioread_sheet_next_cell_string(history_sheet, &val)) {
            if (val != NULL) {
                snprintf(entry.grade_symbol, sizeof(entry.grade_symbol), "%s", val);
                free(val);
                val = NULL;
            }
        }
        if (xlsxioread_sheet_next_cell_string(history_sheet, &val)) {
            if (val != NULL) {
                entry.entry_date = (int64_t)strtoll(val, NULL, 10);
                free(val);
                val = NULL;
            }
        }
        while (xlsxioread_sheet_next_cell_string(history_sheet, &val)) {
            if (val != NULL) {
                free(val);
                val = NULL;
            }
        }

        GGStatus row_status = ggvalidate_course_entry(&temp_scale, &entry);
        if (row_status != GG_OK) {
            gg_import_summary_add_rejection(summary, row_number, "entry", entry.course_label, "Course entry failed validation");
            continue;
        }

        if (!dry_run) {
            int64_t out_id = 0;
            status = course_repo->insert_course(course_repo->context, &entry, &out_id);
            if (status != GG_OK) {
                gg_import_summary_add_rejection(summary, row_number, "database", entry.course_label, "Failed to insert into database");
                continue;
            }
        }
        summary->valid_rows_imported++;
    }

    xlsxioread_sheet_close(history_sheet);
    xlsxioread_close(reader);

    return GG_OK;
}
