#ifndef XLSXWRITER_H
#define XLSXWRITER_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    LXW_NO_ERROR = 0,
    LXW_ERROR_NULL_PARAMETER_IGNORED = 1,
    LXW_ERROR_MEMORY_MALLOC_FAILED = 2,
    LXW_ERROR_CREATING_XLSX_FILE = 3,
    LXW_ERROR_ZIP_FILE_OPERATION = 4
} lxw_error;

typedef uint32_t lxw_row_t;
typedef uint16_t lxw_col_t;

typedef struct lxw_format lxw_format;
typedef struct lxw_worksheet lxw_worksheet;
typedef struct lxw_workbook lxw_workbook;

lxw_workbook *workbook_new(const char *filename);
lxw_format *workbook_add_format(lxw_workbook *workbook);
void format_set_bold(lxw_format *format);
lxw_worksheet *workbook_add_worksheet(lxw_workbook *workbook, const char *sheetname);
lxw_error worksheet_write_string(lxw_worksheet *worksheet, lxw_row_t row, lxw_col_t col, const char *string, lxw_format *format);
lxw_error worksheet_write_number(lxw_worksheet *worksheet, lxw_row_t row, lxw_col_t col, double number, lxw_format *format);
lxw_error worksheet_set_column(lxw_worksheet *worksheet, lxw_col_t first_col, lxw_col_t last_col, double width, lxw_format *format);
lxw_error workbook_close(lxw_workbook *workbook);

#endif
