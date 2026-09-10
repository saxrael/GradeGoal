#ifndef XLSXIO_READ_H
#define XLSXIO_READ_H

#include <stddef.h>

#define XLSXIOREAD_SKIP_EMPTY_ROWS 0x01

typedef struct xlsxio_read_struct* xlsxioreader;
typedef struct xlsxio_read_sheetlist_struct* xlsxioreadersheetlist;
typedef struct xlsxio_read_sheet_struct* xlsxioreadersheet;

xlsxioreader xlsxioread_open(const char *filename);
void xlsxioread_close(xlsxioreader handle);
xlsxioreadersheetlist xlsxioread_sheetlist_open(xlsxioreader handle);
void xlsxioread_sheetlist_close(xlsxioreadersheetlist handle);
const char *xlsxioread_sheetlist_next(xlsxioreadersheetlist handle);
xlsxioreadersheet xlsxioread_sheet_open(xlsxioreader handle, const char *sheetname, unsigned int flags);
void xlsxioread_sheet_close(xlsxioreadersheet handle);
int xlsxioread_sheet_next_row(xlsxioreadersheet handle);
int xlsxioread_sheet_next_cell_string(xlsxioreadersheet handle, char **value);

#endif
