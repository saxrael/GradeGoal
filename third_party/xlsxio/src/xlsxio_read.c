#include "../include/xlsxio_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[64];
    long file_offset;
} SheetMeta;

struct xlsxio_read_struct {
    char filename[512];
    SheetMeta sheets[16];
    size_t sheet_count;
};

struct xlsxio_read_sheetlist_struct {
    struct xlsxio_read_struct *reader;
    size_t current_idx;
};

struct xlsxio_read_sheet_struct {
    FILE *fp;
    char current_line[2048];
    char *cursor;
    int at_eof;
};

static char *my_strdup(const char *src) {
    if (src == NULL) {
        return NULL;
    }
    size_t len = strlen(src);
    char *dst = (char *)malloc(len + 1);
    if (dst != NULL) {
        memcpy(dst, src, len + 1);
    }
    return dst;
}

xlsxioreader xlsxioread_open(const char *filename) {
    if (filename == NULL) {
        return NULL;
    }
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        return NULL;
    }
    struct xlsxio_read_struct *reader = (struct xlsxio_read_struct *)calloc(1, sizeof(struct xlsxio_read_struct));
    if (reader == NULL) {
        fclose(fp);
        return NULL;
    }
    snprintf(reader->filename, sizeof(reader->filename), "%s", filename);

    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "[SHEET:", 7) == 0) {
            char *end = strchr(line + 7, ']');
            if (end != NULL && reader->sheet_count < 16) {
                size_t len = (size_t)(end - (line + 7));
                if (len < sizeof(reader->sheets[0].name)) {
                    strncpy(reader->sheets[reader->sheet_count].name, line + 7, len);
                    reader->sheets[reader->sheet_count].name[len] = '\0';
                    reader->sheets[reader->sheet_count].file_offset = ftell(fp);
                    reader->sheet_count++;
                }
            }
        }
    }
    fclose(fp);
    return reader;
}

void xlsxioread_close(xlsxioreader handle) {
    free(handle);
}

xlsxioreadersheetlist xlsxioread_sheetlist_open(xlsxioreader handle) {
    if (handle == NULL) {
        return NULL;
    }
    struct xlsxio_read_sheetlist_struct *sl = (struct xlsxio_read_sheetlist_struct *)calloc(1, sizeof(*sl));
    if (sl != NULL) {
        sl->reader = handle;
        sl->current_idx = 0;
    }
    return sl;
}

void xlsxioread_sheetlist_close(xlsxioreadersheetlist handle) {
    free(handle);
}

const char *xlsxioread_sheetlist_next(xlsxioreadersheetlist handle) {
    if (handle == NULL || handle->reader == NULL) {
        return NULL;
    }
    if (handle->current_idx < handle->reader->sheet_count) {
        return handle->reader->sheets[handle->current_idx++].name;
    }
    return NULL;
}

xlsxioreadersheet xlsxioread_sheet_open(xlsxioreader handle, const char *sheetname, unsigned int flags) {
    (void)flags;
    if (handle == NULL || sheetname == NULL) {
        return NULL;
    }
    long offset = -1;
    for (size_t i = 0; i < handle->sheet_count; i++) {
        if (strcmp(handle->sheets[i].name, sheetname) == 0) {
            offset = handle->sheets[i].file_offset;
            break;
        }
    }
    if (offset < 0) {
        return NULL;
    }
    FILE *fp = fopen(handle->filename, "rb");
    if (fp == NULL) {
        return NULL;
    }
    if (fseek(fp, offset, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    struct xlsxio_read_sheet_struct *sheet = (struct xlsxio_read_sheet_struct *)calloc(1, sizeof(*sheet));
    if (sheet == NULL) {
        fclose(fp);
        return NULL;
    }
    sheet->fp = fp;
    sheet->cursor = NULL;
    sheet->at_eof = 0;
    return sheet;
}

void xlsxioread_sheet_close(xlsxioreadersheet handle) {
    if (handle != NULL) {
        if (handle->fp != NULL) {
            fclose(handle->fp);
        }
        free(handle);
    }
}

int xlsxioread_sheet_next_row(xlsxioreadersheet handle) {
    if (handle == NULL || handle->fp == NULL || handle->at_eof) {
        return 0;
    }
    while (fgets(handle->current_line, sizeof(handle->current_line), handle->fp) != NULL) {
        if (strncmp(handle->current_line, "[SHEET:", 7) == 0) {
            handle->at_eof = 1;
            return 0;
        }
        size_t len = strlen(handle->current_line);
        while (len > 0 && (handle->current_line[len - 1] == '\r' || handle->current_line[len - 1] == '\n')) {
            handle->current_line[len - 1] = '\0';
            len--;
        }
        if (len == 0) {
            continue;
        }
        handle->cursor = handle->current_line;
        return 1;
    }
    handle->at_eof = 1;
    return 0;
}

int xlsxioread_sheet_next_cell_string(xlsxioreadersheet handle, char **value) {
    if (handle == NULL || value == NULL || handle->cursor == NULL || *handle->cursor == '\0') {
        if (value != NULL) {
            *value = NULL;
        }
        return 0;
    }
    char *start = handle->cursor;
    char *delim = strchr(start, '\t');
    if (delim != NULL) {
        *delim = '\0';
        handle->cursor = delim + 1;
    } else {
        handle->cursor = start + strlen(start);
    }
    *value = my_strdup(start);
    return (*value != NULL) ? 1 : 0;
}
