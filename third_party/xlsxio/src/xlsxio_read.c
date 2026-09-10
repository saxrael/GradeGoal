#include "../include/xlsxio_read.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

typedef struct {
    char name[128];
    uint16_t method;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint32_t local_header_offset;
} ZipEntry;

typedef struct {
    char name[64];
    char target_path[128];
    long legacy_offset;
} SheetMeta;

struct xlsxio_read_struct {
    char filename[512];
    int is_zip;
    SheetMeta sheets[32];
    size_t sheet_count;
    char **shared_strings;
    size_t shared_string_count;
    ZipEntry zip_entries[64];
    size_t zip_entry_count;
};

struct xlsxio_read_sheetlist_struct {
    struct xlsxio_read_struct *reader;
    size_t current_idx;
};

typedef struct {
    char **cells;
    size_t cell_count;
} SheetRow;

struct xlsxio_read_sheet_struct {
    struct xlsxio_read_struct *reader;
    int is_zip;
    FILE *legacy_fp;
    char legacy_current_line[2048];
    char *legacy_cursor;
    int legacy_at_eof;
    SheetRow *rows;
    size_t row_count;
    size_t current_row_idx;
    size_t current_col_idx;
    int started;
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

static uint16_t read_u16_le(const unsigned char *buf) {
    return (uint16_t)(buf[0] | (buf[1] << 8));
}

static uint32_t read_u32_le(const unsigned char *buf) {
    return (uint32_t)(buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24));
}

static char *decode_xml_entities(const char *src, size_t len) {
    char *dst = (char *)malloc(len + 1);
    if (dst == NULL) {
        return NULL;
    }
    size_t di = 0;
    for (size_t si = 0; si < len;) {
        if (src[si] == '&') {
            if (si + 5 <= len && strncmp(src + si, "&amp;", 5) == 0) {
                dst[di++] = '&';
                si += 5;
            } else if (si + 4 <= len && strncmp(src + si, "&lt;", 4) == 0) {
                dst[di++] = '<';
                si += 4;
            } else if (si + 4 <= len && strncmp(src + si, "&gt;", 4) == 0) {
                dst[di++] = '>';
                si += 4;
            } else if (si + 6 <= len && strncmp(src + si, "&quot;", 6) == 0) {
                dst[di++] = '"';
                si += 6;
            } else if (si + 6 <= len && strncmp(src + si, "&apos;", 6) == 0) {
                dst[di++] = '\'';
                si += 6;
            } else {
                dst[di++] = src[si++];
            }
        } else {
            dst[di++] = src[si++];
        }
    }
    dst[di] = '\0';
    return dst;
}

static uint32_t col_letters_to_index(const char *str) {
    uint32_t col = 0;
    while (*str >= 'A' && *str <= 'Z') {
        col = col * 26 + (uint32_t)(*str - 'A' + 1);
        str++;
    }
    return col > 0 ? col - 1 : 0;
}

static unsigned char *zip_extract_entry(FILE *fp, const ZipEntry *e) {
    if (fp == NULL || e == NULL) {
        return NULL;
    }
    if (fseek(fp, (long)e->local_header_offset, SEEK_SET) != 0) {
        return NULL;
    }
    unsigned char local_hdr[30];
    if (fread(local_hdr, 1, 30, fp) != 30) {
        return NULL;
    }
    if (read_u32_le(local_hdr) != 0x04034b50) {
        return NULL;
    }
    uint16_t fn_len = read_u16_le(local_hdr + 26);
    uint16_t extra_len = read_u16_le(local_hdr + 28);
    if (fseek(fp, (long)(fn_len + extra_len), SEEK_CUR) != 0) {
        return NULL;
    }

    unsigned char *comp_buf = (unsigned char *)malloc(e->comp_size + 1);
    if (comp_buf == NULL) {
        return NULL;
    }
    if (fread(comp_buf, 1, e->comp_size, fp) != e->comp_size) {
        free(comp_buf);
        return NULL;
    }
    comp_buf[e->comp_size] = '\0';

    if (e->method == 0) {
        return comp_buf;
    }

    if (e->method == 8) {
        unsigned char *uncomp_buf = (unsigned char *)malloc(e->uncomp_size + 1);
        if (uncomp_buf == NULL) {
            free(comp_buf);
            return NULL;
        }
        z_stream strm;
        memset(&strm, 0, sizeof(strm));
        int ret = inflateInit2(&strm, -MAX_WBITS);
        if (ret != Z_OK) {
            free(comp_buf);
            free(uncomp_buf);
            return NULL;
        }
        strm.next_in = comp_buf;
        strm.avail_in = (uInt)e->comp_size;
        strm.next_out = uncomp_buf;
        strm.avail_out = (uInt)e->uncomp_size;
        ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);
        free(comp_buf);
        if (ret != Z_STREAM_END && ret != Z_OK) {
            free(uncomp_buf);
            return NULL;
        }
        uncomp_buf[e->uncomp_size] = '\0';
        return uncomp_buf;
    }

    free(comp_buf);
    return NULL;
}

static const ZipEntry *find_zip_entry(const struct xlsxio_read_struct *reader, const char *name) {
    for (size_t i = 0; i < reader->zip_entry_count; i++) {
        if (strcmp(reader->zip_entries[i].name, name) == 0) {
            return &reader->zip_entries[i];
        }
    }
    return NULL;
}

static const ZipEntry *find_sheet_zip_entry(const struct xlsxio_read_struct *reader, const char *target) {
    const ZipEntry *ze = find_zip_entry(reader, target);
    if (ze != NULL) {
        return ze;
    }
    if (strncmp(target, "xl/", 3) == 0) {
        ze = find_zip_entry(reader, target + 3);
        if (ze != NULL) {
            return ze;
        }
    } else {
        char buf[160];
        snprintf(buf, sizeof(buf), "xl/%s", target);
        ze = find_zip_entry(reader, buf);
        if (ze != NULL) {
            return ze;
        }
    }
    return NULL;
}

static int parse_zip_directory(FILE *fp, struct xlsxio_read_struct *reader) {
    if (fseek(fp, 0, SEEK_END) != 0) {
        return -1;
    }
    long file_size = ftell(fp);
    if (file_size < 22) {
        return -1;
    }

    long seek_back = file_size > 4096 ? 4096 : file_size;
    if (fseek(fp, file_size - seek_back, SEEK_SET) != 0) {
        return -1;
    }

    unsigned char search_buf[4096];
    size_t bytes_read = fread(search_buf, 1, (size_t)seek_back, fp);
    if (bytes_read < 22) {
        return -1;
    }

    long eocd_pos = -1;
    for (long i = (long)bytes_read - 22; i >= 0; i--) {
        if (search_buf[i] == 0x50 && search_buf[i + 1] == 0x4b && search_buf[i + 2] == 0x05 &&
            search_buf[i + 3] == 0x06) {
            eocd_pos = (file_size - seek_back) + i;
            break;
        }
    }
    if (eocd_pos < 0) {
        return -1;
    }

    if (fseek(fp, eocd_pos, SEEK_SET) != 0) {
        return -1;
    }
    unsigned char eocd[22];
    if (fread(eocd, 1, 22, fp) != 22) {
        return -1;
    }

    uint16_t total_entries = read_u16_le(eocd + 10);
    uint32_t cd_offset = read_u32_le(eocd + 16);

    if (fseek(fp, (long)cd_offset, SEEK_SET) != 0) {
        return -1;
    }

    for (uint16_t i = 0; i < total_entries && reader->zip_entry_count < 64; i++) {
        unsigned char cd_hdr[46];
        if (fread(cd_hdr, 1, 46, fp) != 46) {
            break;
        }
        if (read_u32_le(cd_hdr) != 0x02014b50) {
            break;
        }
        uint16_t method = read_u16_le(cd_hdr + 10);
        uint32_t comp_size = read_u32_le(cd_hdr + 20);
        uint32_t uncomp_size = read_u32_le(cd_hdr + 24);
        uint16_t fn_len = read_u16_le(cd_hdr + 28);
        uint16_t extra_len = read_u16_le(cd_hdr + 30);
        uint16_t comment_len = read_u16_le(cd_hdr + 32);
        uint32_t local_hdr_offset = read_u32_le(cd_hdr + 42);

        ZipEntry *ze = &reader->zip_entries[reader->zip_entry_count];
        memset(ze, 0, sizeof(*ze));
        ze->method = method;
        ze->comp_size = comp_size;
        ze->uncomp_size = uncomp_size;
        ze->local_header_offset = local_hdr_offset;

        size_t to_read = fn_len < sizeof(ze->name) - 1 ? fn_len : sizeof(ze->name) - 1;
        if (fread(ze->name, 1, to_read, fp) != to_read) {
            break;
        }
        ze->name[to_read] = '\0';
        if (fn_len > to_read) {
            fseek(fp, (long)(fn_len - to_read), SEEK_CUR);
        }
        if (extra_len + comment_len > 0) {
            fseek(fp, (long)(extra_len + comment_len), SEEK_CUR);
        }
        reader->zip_entry_count++;
    }

    return 0;
}

static void parse_shared_strings(struct xlsxio_read_struct *reader, const char *xml_data) {
    if (reader == NULL || xml_data == NULL) {
        return;
    }
    const char *p = xml_data;
    size_t cap = 64;
    reader->shared_strings = (char **)malloc(cap * sizeof(char *));
    if (reader->shared_strings == NULL) {
        return;
    }

    while ((p = strstr(p, "<si>")) != NULL || (p = strstr(p, "<si ")) != NULL) {
        const char *si_end = strstr(p, "</si>");
        if (si_end == NULL) {
            break;
        }
        const char *t_open = strstr(p, "<t>");
        if (t_open == NULL || t_open > si_end) {
            t_open = strstr(p, "<t ");
            if (t_open != NULL && t_open < si_end) {
                t_open = strchr(t_open, '>');
                if (t_open != NULL) {
                    t_open++;
                }
            } else {
                t_open = NULL;
            }
        } else {
            t_open += 3;
        }

        char *val = NULL;
        if (t_open != NULL && t_open < si_end) {
            const char *t_close = strstr(t_open, "</t>");
            if (t_close != NULL && t_close <= si_end) {
                val = decode_xml_entities(t_open, (size_t)(t_close - t_open));
            }
        }
        if (val == NULL) {
            val = my_strdup("");
        }

        if (reader->shared_string_count >= cap) {
            cap *= 2;
            char **new_ss = (char **)realloc(reader->shared_strings, cap * sizeof(char *));
            if (new_ss == NULL) {
                free(val);
                break;
            }
            reader->shared_strings = new_ss;
        }
        reader->shared_strings[reader->shared_string_count++] = val;
        p = si_end + 5;
    }
}

static void parse_workbook_xml(struct xlsxio_read_struct *reader, const char *xml_data) {
    if (reader == NULL || xml_data == NULL) {
        return;
    }
    const char *p = xml_data;
    while ((p = strstr(p, "<sheet ")) != NULL) {
        const char *tag_end = strchr(p, '>');
        if (tag_end == NULL) {
            break;
        }
        const char *name_attr = strstr(p, "name=\"");
        if (name_attr != NULL && name_attr < tag_end && reader->sheet_count < 32) {
            name_attr += 6;
            const char *name_end = strchr(name_attr, '"');
            if (name_end != NULL && name_end <= tag_end) {
                size_t nlen = (size_t)(name_end - name_attr);
                SheetMeta *sm = &reader->sheets[reader->sheet_count];
                if (nlen < sizeof(sm->name)) {
                    memcpy(sm->name, name_attr, nlen);
                    sm->name[nlen] = '\0';
                }
                snprintf(sm->target_path, sizeof(sm->target_path), "xl/worksheets/sheet%zu.xml",
                         reader->sheet_count + 1);
                reader->sheet_count++;
            }
        }
        p = tag_end + 1;
    }
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

    unsigned char magic[4];
    if (fread(magic, 1, 4, fp) == 4 && magic[0] == 0x50 && magic[1] == 0x4b && magic[2] == 0x03 && magic[3] == 0x04) {
        reader->is_zip = 1;
        if (parse_zip_directory(fp, reader) == 0) {
            const ZipEntry *wb_entry = find_zip_entry(reader, "xl/workbook.xml");
            if (wb_entry == NULL) {
                wb_entry = find_zip_entry(reader, "workbook.xml");
            }
            if (wb_entry != NULL) {
                unsigned char *wb_xml = zip_extract_entry(fp, wb_entry);
                if (wb_xml != NULL) {
                    parse_workbook_xml(reader, (const char *)wb_xml);
                    free(wb_xml);
                }
            }

            const ZipEntry *ss_entry = find_zip_entry(reader, "xl/sharedStrings.xml");
            if (ss_entry == NULL) {
                ss_entry = find_zip_entry(reader, "sharedStrings.xml");
            }
            if (ss_entry != NULL) {
                unsigned char *ss_xml = zip_extract_entry(fp, ss_entry);
                if (ss_xml != NULL) {
                    parse_shared_strings(reader, (const char *)ss_xml);
                    free(ss_xml);
                }
            }
        }
        fclose(fp);
        return reader;
    }

    fseek(fp, 0, SEEK_SET);
    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "[SHEET:", 7) == 0) {
            char *end = strchr(line + 7, ']');
            if (end != NULL && reader->sheet_count < 32) {
                size_t len = (size_t)(end - (line + 7));
                if (len < sizeof(reader->sheets[0].name)) {
                    strncpy(reader->sheets[reader->sheet_count].name, line + 7, len);
                    reader->sheets[reader->sheet_count].name[len] = '\0';
                    reader->sheets[reader->sheet_count].legacy_offset = ftell(fp);
                    reader->sheet_count++;
                }
            }
        }
    }
    fclose(fp);
    return reader;
}

void xlsxioread_close(xlsxioreader handle) {
    if (handle == NULL) {
        return;
    }
    if (handle->shared_strings != NULL) {
        for (size_t i = 0; i < handle->shared_string_count; i++) {
            if (handle->shared_strings[i] != NULL) {
                free(handle->shared_strings[i]);
            }
        }
        free(handle->shared_strings);
    }
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

static void parse_worksheet_xml(struct xlsxio_read_sheet_struct *sheet, const char *xml_data) {
    const char *p = xml_data;
    const char *sheet_data = strstr(p, "<sheetData");
    if (sheet_data == NULL) {
        return;
    }
    p = sheet_data;
    const char *sheet_data_end = strstr(p, "</sheetData>");

    size_t row_cap = 64;
    sheet->rows = (SheetRow *)malloc(row_cap * sizeof(SheetRow));
    if (sheet->rows == NULL) {
        return;
    }

    while (p != NULL && (sheet_data_end == NULL || p < sheet_data_end)) {
        const char *row_start = strstr(p, "<row");
        if (row_start == NULL || (sheet_data_end != NULL && row_start >= sheet_data_end)) {
            break;
        }
        const char *row_end = strstr(row_start, "</row>");
        if (row_end == NULL) {
            break;
        }

        if (sheet->row_count >= row_cap) {
            row_cap *= 2;
            SheetRow *new_rows = (SheetRow *)realloc(sheet->rows, row_cap * sizeof(SheetRow));
            if (new_rows == NULL) {
                break;
            }
            sheet->rows = new_rows;
        }

        SheetRow *curr_row = &sheet->rows[sheet->row_count++];
        curr_row->cells = NULL;
        curr_row->cell_count = 0;
        size_t cell_cap = 16;
        curr_row->cells = (char **)calloc(cell_cap, sizeof(char *));

        const char *cp = row_start;
        while (cp != NULL && cp < row_end) {
            const char *c_start = strstr(cp, "<c");
            if (c_start == NULL || c_start >= row_end) {
                break;
            }
            const char *c_open_end = strchr(c_start, '>');
            if (c_open_end == NULL || c_open_end > row_end) {
                break;
            }

            int is_self_closing = (c_open_end > c_start && *(c_open_end - 1) == '/');
            const char *c_end = NULL;
            if (is_self_closing) {
                c_end = c_open_end;
            } else {
                c_end = strstr(c_open_end, "</c>");
                if (c_end == NULL || c_end > row_end) {
                    c_end = row_end;
                }
            }

            uint32_t col_idx = (uint32_t)curr_row->cell_count;
            const char *r_attr = strstr(c_start, "r=\"");
            if (r_attr != NULL && r_attr < c_open_end) {
                col_idx = col_letters_to_index(r_attr + 3);
            }

            char t_attr[32] = {0};
            const char *t_ptr = strstr(c_start, "t=\"");
            if (t_ptr != NULL && t_ptr < c_open_end) {
                t_ptr += 3;
                const char *t_end = strchr(t_ptr, '"');
                if (t_end != NULL && t_end <= c_open_end) {
                    size_t tlen = (size_t)(t_end - t_ptr);
                    if (tlen < sizeof(t_attr)) {
                        memcpy(t_attr, t_ptr, tlen);
                        t_attr[tlen] = '\0';
                    }
                }
            }

            char *cell_val = NULL;
            if (!is_self_closing) {
                if (strcmp(t_attr, "inlineStr") == 0) {
                    const char *t_tag = strstr(c_open_end, "<t>");
                    if (t_tag == NULL || t_tag >= c_end) {
                        t_tag = strstr(c_open_end, "<t ");
                        if (t_tag != NULL && t_tag < c_end) {
                            t_tag = strchr(t_tag, '>');
                            if (t_tag != NULL) {
                                t_tag++;
                            }
                        }
                    } else {
                        t_tag += 3;
                    }
                    if (t_tag != NULL && t_tag < c_end) {
                        const char *t_close = strstr(t_tag, "</t>");
                        if (t_close != NULL && t_close <= c_end) {
                            cell_val = decode_xml_entities(t_tag, (size_t)(t_close - t_tag));
                        }
                    }
                } else if (strcmp(t_attr, "s") == 0) {
                    const char *v_tag = strstr(c_open_end, "<v>");
                    if (v_tag != NULL && v_tag < c_end) {
                        v_tag += 3;
                        const char *v_close = strstr(v_tag, "</v>");
                        if (v_close != NULL && v_close <= c_end) {
                            size_t s_idx = (size_t)strtoul(v_tag, NULL, 10);
                            if (sheet->reader != NULL && s_idx < sheet->reader->shared_string_count) {
                                cell_val = my_strdup(sheet->reader->shared_strings[s_idx]);
                            }
                        }
                    }
                } else {
                    const char *v_tag = strstr(c_open_end, "<v>");
                    if (v_tag != NULL && v_tag < c_end) {
                        v_tag += 3;
                        const char *v_close = strstr(v_tag, "</v>");
                        if (v_close != NULL && v_close <= c_end) {
                            cell_val = decode_xml_entities(v_tag, (size_t)(v_close - v_tag));
                        }
                    }
                }
            }
            if (cell_val == NULL) {
                cell_val = my_strdup("");
            }

            while (curr_row->cell_count < col_idx) {
                if (curr_row->cell_count >= cell_cap) {
                    cell_cap = (curr_row->cell_count + 8) * 2;
                    curr_row->cells = (char **)realloc(curr_row->cells, cell_cap * sizeof(char *));
                }
                curr_row->cells[curr_row->cell_count++] = my_strdup("");
            }

            if (curr_row->cell_count >= cell_cap) {
                cell_cap = (curr_row->cell_count + 8) * 2;
                curr_row->cells = (char **)realloc(curr_row->cells, cell_cap * sizeof(char *));
            }
            curr_row->cells[curr_row->cell_count++] = cell_val;

            cp = is_self_closing ? c_open_end + 1 : c_end + 4;
        }

        p = row_end + 6;
    }
}

xlsxioreadersheet xlsxioread_sheet_open(xlsxioreader handle, const char *sheetname, unsigned int flags) {
    (void)flags;
    if (handle == NULL || sheetname == NULL) {
        return NULL;
    }

    size_t target_idx = (size_t)-1;
    for (size_t i = 0; i < handle->sheet_count; i++) {
        if (strcmp(handle->sheets[i].name, sheetname) == 0) {
            target_idx = i;
            break;
        }
    }
    if (target_idx == (size_t)-1) {
        return NULL;
    }

    struct xlsxio_read_sheet_struct *sheet = (struct xlsxio_read_sheet_struct *)calloc(1, sizeof(*sheet));
    if (sheet == NULL) {
        return NULL;
    }
    sheet->reader = handle;
    sheet->is_zip = handle->is_zip;

    if (!handle->is_zip) {
        FILE *fp = fopen(handle->filename, "rb");
        if (fp == NULL) {
            free(sheet);
            return NULL;
        }
        if (fseek(fp, handle->sheets[target_idx].legacy_offset, SEEK_SET) != 0) {
            fclose(fp);
            free(sheet);
            return NULL;
        }
        sheet->legacy_fp = fp;
        return sheet;
    }

    FILE *fp = fopen(handle->filename, "rb");
    if (fp == NULL) {
        free(sheet);
        return NULL;
    }

    const ZipEntry *ze = find_sheet_zip_entry(handle, handle->sheets[target_idx].target_path);
    if (ze == NULL) {
        fclose(fp);
        free(sheet);
        return NULL;
    }

    unsigned char *xml_data = zip_extract_entry(fp, ze);
    fclose(fp);
    if (xml_data == NULL) {
        free(sheet);
        return NULL;
    }

    parse_worksheet_xml(sheet, (const char *)xml_data);
    free(xml_data);
    return sheet;
}

void xlsxioread_sheet_close(xlsxioreadersheet handle) {
    if (handle == NULL) {
        return;
    }
    if (!handle->is_zip) {
        if (handle->legacy_fp != NULL) {
            fclose(handle->legacy_fp);
        }
    } else {
        if (handle->rows != NULL) {
            for (size_t r = 0; r < handle->row_count; r++) {
                if (handle->rows[r].cells != NULL) {
                    for (size_t c = 0; c < handle->rows[r].cell_count; c++) {
                        if (handle->rows[r].cells[c] != NULL) {
                            free(handle->rows[r].cells[c]);
                        }
                    }
                    free(handle->rows[r].cells);
                }
            }
            free(handle->rows);
        }
    }
    free(handle);
}

int xlsxioread_sheet_next_row(xlsxioreadersheet handle) {
    if (handle == NULL) {
        return 0;
    }
    if (!handle->is_zip) {
        if (handle->legacy_fp == NULL || handle->legacy_at_eof) {
            return 0;
        }
        while (fgets(handle->legacy_current_line, sizeof(handle->legacy_current_line), handle->legacy_fp) != NULL) {
            if (strncmp(handle->legacy_current_line, "[SHEET:", 7) == 0) {
                handle->legacy_at_eof = 1;
                return 0;
            }
            size_t len = strlen(handle->legacy_current_line);
            while (len > 0 &&
                   (handle->legacy_current_line[len - 1] == '\r' || handle->legacy_current_line[len - 1] == '\n')) {
                handle->legacy_current_line[len - 1] = '\0';
                len--;
            }
            if (len == 0) {
                continue;
            }
            handle->legacy_cursor = handle->legacy_current_line;
            return 1;
        }
        handle->legacy_at_eof = 1;
        return 0;
    }

    if (!handle->started) {
        handle->started = 1;
        handle->current_row_idx = 0;
    } else {
        handle->current_row_idx++;
    }

    if (handle->current_row_idx < handle->row_count) {
        handle->current_col_idx = 0;
        return 1;
    }

    return 0;
}

int xlsxioread_sheet_next_cell_string(xlsxioreadersheet handle, char **value) {
    if (handle == NULL || value == NULL) {
        if (value != NULL) {
            *value = NULL;
        }
        return 0;
    }
    if (!handle->is_zip) {
        if (handle->legacy_cursor == NULL || *handle->legacy_cursor == '\0') {
            *value = NULL;
            return 0;
        }
        char *start = handle->legacy_cursor;
        char *delim = strchr(start, '\t');
        if (delim != NULL) {
            *delim = '\0';
            handle->legacy_cursor = delim + 1;
        } else {
            handle->legacy_cursor = start + strlen(start);
        }
        *value = my_strdup(start);
        return (*value != NULL) ? 1 : 0;
    }

    if (!handle->started || handle->current_row_idx >= handle->row_count) {
        *value = NULL;
        return 0;
    }

    SheetRow *r = &handle->rows[handle->current_row_idx];
    if (handle->current_col_idx < r->cell_count) {
        *value = my_strdup(r->cells[handle->current_col_idx++]);
        return (*value != NULL) ? 1 : 0;
    }

    *value = NULL;
    return 0;
}
