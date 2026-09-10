#include "../include/xlsxwriter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct lxw_format {
    int bold;
};

typedef struct {
    char value[128];
} LXWCell;

typedef struct {
    LXWCell cells[16];
    size_t col_count;
} LXWRow;

struct lxw_worksheet {
    char name[64];
    LXWRow rows[1024];
    size_t row_count;
};

struct lxw_workbook {
    char filename[512];
    lxw_worksheet sheets[16];
    size_t sheet_count;
    lxw_format formats[16];
    size_t format_count;
};

lxw_workbook *workbook_new(const char *filename) {
    if (filename == NULL) {
        return NULL;
    }
    lxw_workbook *wb = (lxw_workbook *)calloc(1, sizeof(lxw_workbook));
    if (wb == NULL) {
        return NULL;
    }
    snprintf(wb->filename, sizeof(wb->filename), "%s", filename);
    return wb;
}

lxw_format *workbook_add_format(lxw_workbook *workbook) {
    if (workbook == NULL || workbook->format_count >= 16) {
        return NULL;
    }
    lxw_format *fmt = &workbook->formats[workbook->format_count++];
    memset(fmt, 0, sizeof(*fmt));
    return fmt;
}

void format_set_bold(lxw_format *format) {
    if (format != NULL) {
        format->bold = 1;
    }
}

lxw_worksheet *workbook_add_worksheet(lxw_workbook *workbook, const char *sheetname) {
    if (workbook == NULL || sheetname == NULL || workbook->sheet_count >= 16) {
        return NULL;
    }
    lxw_worksheet *ws = &workbook->sheets[workbook->sheet_count++];
    memset(ws, 0, sizeof(*ws));
    snprintf(ws->name, sizeof(ws->name), "%s", sheetname);
    return ws;
}

lxw_error worksheet_write_string(lxw_worksheet *worksheet, lxw_row_t row, lxw_col_t col, const char *string, lxw_format *format) {
    (void)format;
    if (worksheet == NULL || string == NULL || row >= 1024 || col >= 16) {
        return LXW_ERROR_NULL_PARAMETER_IGNORED;
    }
    if (row >= worksheet->row_count) {
        worksheet->row_count = row + 1;
    }
    if (col >= worksheet->rows[row].col_count) {
        worksheet->rows[row].col_count = col + 1;
    }
    snprintf(worksheet->rows[row].cells[col].value, sizeof(worksheet->rows[row].cells[col].value), "%s", string);
    return LXW_NO_ERROR;
}

lxw_error worksheet_write_number(lxw_worksheet *worksheet, lxw_row_t row, lxw_col_t col, double number, lxw_format *format) {
    (void)format;
    if (worksheet == NULL || row >= 1024 || col >= 16) {
        return LXW_ERROR_NULL_PARAMETER_IGNORED;
    }
    if (row >= worksheet->row_count) {
        worksheet->row_count = row + 1;
    }
    if (col >= worksheet->rows[row].col_count) {
        worksheet->rows[row].col_count = col + 1;
    }
    if (number == (double)(int64_t)number) {
        snprintf(worksheet->rows[row].cells[col].value, sizeof(worksheet->rows[row].cells[col].value), "%lld", (long long)number);
    } else {
        snprintf(worksheet->rows[row].cells[col].value, sizeof(worksheet->rows[row].cells[col].value), "%.2f", number);
    }
    return LXW_NO_ERROR;
}

lxw_error worksheet_set_column(lxw_worksheet *worksheet, lxw_col_t first_col, lxw_col_t last_col, double width, lxw_format *format) {
    (void)worksheet;
    (void)first_col;
    (void)last_col;
    (void)width;
    (void)format;
    return LXW_NO_ERROR;
}

lxw_error workbook_close(lxw_workbook *workbook) {
    if (workbook == NULL) {
        return LXW_ERROR_NULL_PARAMETER_IGNORED;
    }
    FILE *fp = fopen(workbook->filename, "wb");
    if (fp == NULL) {
        free(workbook);
        return LXW_ERROR_CREATING_XLSX_FILE;
    }
    fputs("GRADEGOAL_WORKBOOK_V1\n", fp);
    for (size_t s = 0; s < workbook->sheet_count; s++) {
        lxw_worksheet *ws = &workbook->sheets[s];
        fprintf(fp, "[SHEET:%s]\n", ws->name);
        for (size_t r = 0; r < ws->row_count; r++) {
            LXWRow *row = &ws->rows[r];
            for (size_t c = 0; c < row->col_count; c++) {
                if (c > 0) {
                    fputc('\t', fp);
                }
                fputs(row->cells[c].value, fp);
            }
            fputc('\n', fp);
        }
    }
    fclose(fp);
    free(workbook);
    return LXW_NO_ERROR;
}
