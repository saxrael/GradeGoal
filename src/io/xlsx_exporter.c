#include "xlsx_exporter.h"
#include <stdlib.h>
#include <string.h>
#include <xlsxwriter.h>
#include "../core/course_list.h"

GGStatus gg_xlsx_export(const char *filepath, GGScaleRepository *scale_repo, GGCourseRepository *course_repo) {
    if (filepath == NULL || scale_repo == NULL || course_repo == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    GGStatus status = scale_repo->load_scale(scale_repo->context, &scale);
    if (status != GG_OK) {
        return status;
    }

    GGCourseList *courses = NULL;
    status = course_repo->list_all_courses(course_repo->context, &courses);
    if (status != GG_OK) {
        return status;
    }

    lxw_workbook *workbook = workbook_new(filepath);
    if (workbook == NULL) {
        gg_course_list_destroy(courses);
        return GG_ERR_IO;
    }

    lxw_format *header_format = workbook_add_format(workbook);
    format_set_bold(header_format);

    lxw_worksheet *scale_sheet = workbook_add_worksheet(workbook, "Scale");
    if (scale_sheet == NULL) {
        workbook_close(workbook);
        gg_course_list_destroy(courses);
        return GG_ERR_IO;
    }

    worksheet_write_string(scale_sheet, 0, 0, "grade_symbol", header_format);
    worksheet_write_string(scale_sheet, 0, 1, "grade_point", header_format);

    for (size_t i = 0; i < scale.count; i++) {
        lxw_row_t row = (lxw_row_t)(i + 1);
        worksheet_write_string(scale_sheet, row, 0, scale.items[i].grade_symbol, NULL);
        worksheet_write_number(scale_sheet, row, 1, scale.items[i].grade_point, NULL);
    }

    lxw_worksheet *history_sheet = workbook_add_worksheet(workbook, "History");
    if (history_sheet == NULL) {
        workbook_close(workbook);
        gg_course_list_destroy(courses);
        return GG_ERR_IO;
    }

    worksheet_write_string(history_sheet, 0, 0, "semester_label", header_format);
    worksheet_write_string(history_sheet, 0, 1, "course_label", header_format);
    worksheet_write_string(history_sheet, 0, 2, "credit_unit", header_format);
    worksheet_write_string(history_sheet, 0, 3, "grade_symbol", header_format);
    worksheet_write_string(history_sheet, 0, 4, "entry_date", header_format);

    for (size_t i = 0; i < courses->count; i++) {
        lxw_row_t row = (lxw_row_t)(i + 1);
        const GGCourseEntry *entry = &courses->entries[i];
        worksheet_write_string(history_sheet, row, 0, entry->semester_label, NULL);
        worksheet_write_string(history_sheet, row, 1, entry->course_label, NULL);
        worksheet_write_number(history_sheet, row, 2, (double)entry->credit_unit, NULL);
        worksheet_write_string(history_sheet, row, 3, entry->grade_symbol, NULL);
        worksheet_write_number(history_sheet, row, 4, (double)entry->entry_date, NULL);
    }

    worksheet_set_column(scale_sheet, 0, 0, 15.0, NULL);
    worksheet_set_column(scale_sheet, 1, 1, 15.0, NULL);
    worksheet_set_column(history_sheet, 0, 0, 20.0, NULL);
    worksheet_set_column(history_sheet, 1, 1, 25.0, NULL);
    worksheet_set_column(history_sheet, 2, 2, 12.0, NULL);
    worksheet_set_column(history_sheet, 3, 3, 12.0, NULL);
    worksheet_set_column(history_sheet, 4, 4, 15.0, NULL);

    lxw_error err = workbook_close(workbook);
    gg_course_list_destroy(courses);

    if (err != LXW_NO_ERROR) {
        return GG_ERR_IO;
    }

    return GG_OK;
}
