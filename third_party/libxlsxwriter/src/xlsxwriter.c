#include "../include/xlsxwriter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

struct lxw_format {
    int bold;
};

typedef struct {
    uint32_t row;
    uint16_t col;
    int is_number;
    double number_value;
    char *str_value;
    int bold;
} LXWCell;

typedef struct {
    uint16_t first_col;
    uint16_t last_col;
    double width;
} LXWColWidth;

struct lxw_worksheet {
    char name[64];
    LXWCell *cells;
    size_t cell_count;
    size_t cell_capacity;
    LXWColWidth col_widths[16];
    size_t col_width_count;
};

struct lxw_workbook {
    char filename[512];
    lxw_worksheet sheets[16];
    size_t sheet_count;
    lxw_format formats[16];
    size_t format_count;
};

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Buffer;

static void buf_init(Buffer *b) {
    b->data = NULL;
    b->size = 0;
    b->capacity = 0;
}

static void buf_free(Buffer *b) {
    if (b->data != NULL) {
        free(b->data);
        b->data = NULL;
    }
    b->size = 0;
    b->capacity = 0;
}

static void buf_append(Buffer *b, const char *str, size_t len) {
    if (str == NULL || len == 0) {
        return;
    }
    if (b->size + len + 1 > b->capacity) {
        size_t new_cap = (b->capacity == 0) ? 1024 : b->capacity * 2;
        while (new_cap < b->size + len + 1) {
            new_cap *= 2;
        }
        char *new_data = (char *)realloc(b->data, new_cap);
        if (new_data == NULL) {
            return;
        }
        b->data = new_data;
        b->capacity = new_cap;
    }
    memcpy(b->data + b->size, str, len);
    b->size += len;
    b->data[b->size] = '\0';
}

static void buf_append_str(Buffer *b, const char *str) {
    if (str != NULL) {
        buf_append(b, str, strlen(str));
    }
}

static void buf_append_escaped(Buffer *b, const char *str) {
    if (str == NULL) {
        return;
    }
    for (const char *p = str; *p != '\0'; p++) {
        switch (*p) {
        case '&':
            buf_append(b, "&amp;", 5);
            break;
        case '<':
            buf_append(b, "&lt;", 4);
            break;
        case '>':
            buf_append(b, "&gt;", 4);
            break;
        case '"':
            buf_append(b, "&quot;", 6);
            break;
        case '\'':
            buf_append(b, "&apos;", 6);
            break;
        default:
            buf_append(b, p, 1);
            break;
        }
    }
}

static void col_to_name(uint16_t col, char *out, size_t out_size) {
    char buf[16];
    int idx = 0;
    uint32_t c = (uint32_t)col + 1;
    while (c > 0) {
        c--;
        buf[idx++] = (char)('A' + (c % 26));
        c /= 26;
    }
    for (int i = 0; i < idx && (size_t)i + 1 < out_size; i++) {
        out[i] = buf[idx - 1 - i];
    }
    out[idx < (int)out_size ? idx : (int)out_size - 1] = '\0';
}

static int compare_cells(const void *a, const void *b) {
    const LXWCell *ca = (const LXWCell *)a;
    const LXWCell *cb = (const LXWCell *)b;
    if (ca->row != cb->row) {
        return ca->row < cb->row ? -1 : 1;
    }
    if (ca->col != cb->col) {
        return ca->col < cb->col ? -1 : 1;
    }
    return 0;
}

static void write_u16_le(unsigned char *buf, uint16_t val) {
    buf[0] = (unsigned char)(val & 0xFF);
    buf[1] = (unsigned char)((val >> 8) & 0xFF);
}

static void write_u32_le(unsigned char *buf, uint32_t val) {
    buf[0] = (unsigned char)(val & 0xFF);
    buf[1] = (unsigned char)((val >> 8) & 0xFF);
    buf[2] = (unsigned char)((val >> 16) & 0xFF);
    buf[3] = (unsigned char)((val >> 24) & 0xFF);
}

static int compress_deflate(const unsigned char *in_data, size_t in_size, unsigned char **out_data, size_t *out_size,
                            uint32_t *out_crc) {
    *out_crc = (uint32_t)crc32(0L, Z_NULL, 0);
    if (in_data != NULL && in_size > 0) {
        *out_crc = (uint32_t)crc32(*out_crc, in_data, (uInt)in_size);
    }

    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    int ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        return -1;
    }

    uLong bound = deflateBound(&strm, (uLong)in_size) + 64;
    if (bound < (uLong)in_size + 128) {
        bound = (uLong)in_size + 128;
    }

    unsigned char *compressed = (unsigned char *)malloc(bound);
    if (compressed == NULL) {
        deflateEnd(&strm);
        return -1;
    }

    strm.next_in = (Bytef *)in_data;
    strm.avail_in = (uInt)in_size;
    strm.next_out = (Bytef *)compressed;
    strm.avail_out = (uInt)bound;

    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&strm);
        free(compressed);
        return -1;
    }

    *out_size = (size_t)strm.total_out;
    deflateEnd(&strm);
    *out_data = compressed;
    return 0;
}

typedef struct {
    char filename[128];
    unsigned char *uncomp_data;
    size_t uncomp_size;
    unsigned char *comp_data;
    size_t comp_size;
    uint32_t crc;
    uint32_t local_header_offset;
} ZipFileEntry;

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

lxw_error worksheet_write_string(lxw_worksheet *worksheet, lxw_row_t row, lxw_col_t col, const char *string,
                                 lxw_format *format) {
    if (worksheet == NULL || string == NULL) {
        return LXW_ERROR_NULL_PARAMETER_IGNORED;
    }
    if (worksheet->cell_count >= worksheet->cell_capacity) {
        size_t new_cap = worksheet->cell_capacity == 0 ? 64 : worksheet->cell_capacity * 2;
        LXWCell *new_cells = (LXWCell *)realloc(worksheet->cells, new_cap * sizeof(LXWCell));
        if (new_cells == NULL) {
            return LXW_ERROR_MEMORY_MALLOC_FAILED;
        }
        worksheet->cells = new_cells;
        worksheet->cell_capacity = new_cap;
    }
    LXWCell *cell = &worksheet->cells[worksheet->cell_count++];
    cell->row = row;
    cell->col = col;
    cell->is_number = 0;
    cell->number_value = 0.0;
    cell->str_value = strdup(string);
    cell->bold = (format != NULL && format->bold != 0) ? 1 : 0;
    return LXW_NO_ERROR;
}

lxw_error worksheet_write_number(lxw_worksheet *worksheet, lxw_row_t row, lxw_col_t col, double number,
                                 lxw_format *format) {
    if (worksheet == NULL) {
        return LXW_ERROR_NULL_PARAMETER_IGNORED;
    }
    if (worksheet->cell_count >= worksheet->cell_capacity) {
        size_t new_cap = worksheet->cell_capacity == 0 ? 64 : worksheet->cell_capacity * 2;
        LXWCell *new_cells = (LXWCell *)realloc(worksheet->cells, new_cap * sizeof(LXWCell));
        if (new_cells == NULL) {
            return LXW_ERROR_MEMORY_MALLOC_FAILED;
        }
        worksheet->cells = new_cells;
        worksheet->cell_capacity = new_cap;
    }
    LXWCell *cell = &worksheet->cells[worksheet->cell_count++];
    cell->row = row;
    cell->col = col;
    cell->is_number = 1;
    cell->number_value = number;
    cell->str_value = NULL;
    cell->bold = (format != NULL && format->bold != 0) ? 1 : 0;
    return LXW_NO_ERROR;
}

lxw_error worksheet_set_column(lxw_worksheet *worksheet, lxw_col_t first_col, lxw_col_t last_col, double width,
                               lxw_format *format) {
    (void)format;
    if (worksheet == NULL || worksheet->col_width_count >= 16) {
        return LXW_ERROR_NULL_PARAMETER_IGNORED;
    }
    LXWColWidth *cw = &worksheet->col_widths[worksheet->col_width_count++];
    cw->first_col = first_col;
    cw->last_col = last_col;
    cw->width = width;
    return LXW_NO_ERROR;
}

lxw_error workbook_close(lxw_workbook *workbook) {
    if (workbook == NULL) {
        return LXW_ERROR_NULL_PARAMETER_IGNORED;
    }

    size_t total_zip_entries = 5 + workbook->sheet_count;
    ZipFileEntry *zip_entries = (ZipFileEntry *)calloc(total_zip_entries, sizeof(ZipFileEntry));
    if (zip_entries == NULL) {
        free(workbook);
        return LXW_ERROR_MEMORY_MALLOC_FAILED;
    }

    Buffer ct_buf;
    buf_init(&ct_buf);
    buf_append_str(&ct_buf, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    buf_append_str(&ct_buf, "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n");
    buf_append_str(&ct_buf, "  <Default Extension=\"rels\" "
                            "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n");
    buf_append_str(&ct_buf, "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n");
    buf_append_str(&ct_buf,
                   "  <Override PartName=\"/xl/workbook.xml\" "
                   "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>\n");
    for (size_t s = 0; s < workbook->sheet_count; s++) {
        char s_override[256];
        snprintf(s_override, sizeof(s_override),
                 "  <Override PartName=\"/xl/worksheets/sheet%zu.xml\" "
                 "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>\n",
                 s + 1);
        buf_append_str(&ct_buf, s_override);
    }
    buf_append_str(&ct_buf,
                   "  <Override PartName=\"/xl/styles.xml\" "
                   "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>\n");
    buf_append_str(&ct_buf, "</Types>");

    snprintf(zip_entries[0].filename, sizeof(zip_entries[0].filename), "[Content_Types].xml");
    zip_entries[0].uncomp_data = (unsigned char *)ct_buf.data;
    zip_entries[0].uncomp_size = ct_buf.size;

    Buffer rels_buf;
    buf_init(&rels_buf);
    buf_append_str(&rels_buf, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    buf_append_str(&rels_buf,
                   "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
    buf_append_str(&rels_buf,
                   "  <Relationship Id=\"rId1\" "
                   "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
                   "Target=\"xl/workbook.xml\"/>\n");
    buf_append_str(&rels_buf, "</Relationships>");

    snprintf(zip_entries[1].filename, sizeof(zip_entries[1].filename), "_rels/.rels");
    zip_entries[1].uncomp_data = (unsigned char *)rels_buf.data;
    zip_entries[1].uncomp_size = rels_buf.size;

    Buffer wb_rels_buf;
    buf_init(&wb_rels_buf);
    buf_append_str(&wb_rels_buf, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    buf_append_str(&wb_rels_buf,
                   "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n");
    for (size_t s = 0; s < workbook->sheet_count; s++) {
        char s_rel[256];
        snprintf(s_rel, sizeof(s_rel),
                 "  <Relationship Id=\"rId%zu\" "
                 "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" "
                 "Target=\"worksheets/sheet%zu.xml\"/>\n",
                 s + 1, s + 1);
        buf_append_str(&wb_rels_buf, s_rel);
    }
    char styles_rel[256];
    snprintf(styles_rel, sizeof(styles_rel),
             "  <Relationship Id=\"rId%zu\" "
             "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" "
             "Target=\"styles.xml\"/>\n",
             workbook->sheet_count + 1);
    buf_append_str(&wb_rels_buf, styles_rel);
    buf_append_str(&wb_rels_buf, "</Relationships>");

    snprintf(zip_entries[2].filename, sizeof(zip_entries[2].filename), "xl/_rels/workbook.xml.rels");
    zip_entries[2].uncomp_data = (unsigned char *)wb_rels_buf.data;
    zip_entries[2].uncomp_size = wb_rels_buf.size;

    Buffer wb_buf;
    buf_init(&wb_buf);
    buf_append_str(&wb_buf, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    buf_append_str(&wb_buf, "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">\n");
    buf_append_str(&wb_buf, "  <sheets>\n");
    for (size_t s = 0; s < workbook->sheet_count; s++) {
        char s_entry[256];
        snprintf(s_entry, sizeof(s_entry), "    <sheet name=\"%s\" sheetId=\"%zu\" r:id=\"rId%zu\"/>\n",
                 workbook->sheets[s].name, s + 1, s + 1);
        buf_append_str(&wb_buf, s_entry);
    }
    buf_append_str(&wb_buf, "  </sheets>\n");
    buf_append_str(&wb_buf, "</workbook>");

    snprintf(zip_entries[3].filename, sizeof(zip_entries[3].filename), "xl/workbook.xml");
    zip_entries[3].uncomp_data = (unsigned char *)wb_buf.data;
    zip_entries[3].uncomp_size = wb_buf.size;

    Buffer styles_buf;
    buf_init(&styles_buf);
    buf_append_str(&styles_buf, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
    buf_append_str(&styles_buf, "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n");
    buf_append_str(&styles_buf, "  <fonts count=\"2\">\n"
                                "    <font><sz val=\"11\"/><name val=\"Calibri\"/></font>\n"
                                "    <font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font>\n"
                                "  </fonts>\n"
                                "  <fills count=\"2\">\n"
                                "    <fill><patternFill patternType=\"none\"/></fill>\n"
                                "    <fill><patternFill patternType=\"gray125\"/></fill>\n"
                                "  </fills>\n"
                                "  <borders count=\"1\">\n"
                                "    <border><left/><right/><top/><bottom/><diagonal/></border>\n"
                                "  </borders>\n"
                                "  <cellStyleXfs count=\"1\">\n"
                                "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>\n"
                                "  </cellStyleXfs>\n"
                                "  <cellXfs count=\"2\">\n"
                                "    <xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/>\n"
                                "    <xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" applyFont=\"1\"/>\n"
                                "  </cellXfs>\n"
                                "  <cellStyles count=\"1\">\n"
                                "    <cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/>\n"
                                "  </cellStyles>\n"
                                "</styleSheet>");

    snprintf(zip_entries[4].filename, sizeof(zip_entries[4].filename), "xl/styles.xml");
    zip_entries[4].uncomp_data = (unsigned char *)styles_buf.data;
    zip_entries[4].uncomp_size = styles_buf.size;

    Buffer sheet_bufs[16];
    for (size_t s = 0; s < workbook->sheet_count; s++) {
        buf_init(&sheet_bufs[s]);
        lxw_worksheet *ws = &workbook->sheets[s];

        buf_append_str(&sheet_bufs[s], "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
        buf_append_str(&sheet_bufs[s],
                       "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">\n");

        if (ws->col_width_count > 0) {
            buf_append_str(&sheet_bufs[s], "  <cols>\n");
            for (size_t c = 0; c < ws->col_width_count; c++) {
                char col_tag[128];
                snprintf(col_tag, sizeof(col_tag),
                         "    <col min=\"%u\" max=\"%u\" width=\"%.2f\" customWidth=\"1\"/>\n",
                         ws->col_widths[c].first_col + 1, ws->col_widths[c].last_col + 1, ws->col_widths[c].width);
                buf_append_str(&sheet_bufs[s], col_tag);
            }
            buf_append_str(&sheet_bufs[s], "  </cols>\n");
        }

        buf_append_str(&sheet_bufs[s], "  <sheetData>\n");

        if (ws->cell_count > 0) {
            qsort(ws->cells, ws->cell_count, sizeof(LXWCell), compare_cells);
            uint32_t curr_row = UINT32_MAX;

            for (size_t i = 0; i < ws->cell_count; i++) {
                LXWCell *cell = &ws->cells[i];
                if (cell->row != curr_row) {
                    if (curr_row != UINT32_MAX) {
                        buf_append_str(&sheet_bufs[s], "    </row>\n");
                    }
                    curr_row = cell->row;
                    char r_open[64];
                    snprintf(r_open, sizeof(r_open), "    <row r=\"%u\">\n", curr_row + 1);
                    buf_append_str(&sheet_bufs[s], r_open);
                }

                char col_name[16];
                col_to_name(cell->col, col_name, sizeof(col_name));

                if (cell->is_number) {
                    char c_tag[128];
                    if (cell->number_value == (double)(int64_t)cell->number_value) {
                        snprintf(c_tag, sizeof(c_tag), "      <c r=\"%s%u\"><v>%lld</v></c>\n", col_name, curr_row + 1,
                                 (long long)cell->number_value);
                    } else {
                        snprintf(c_tag, sizeof(c_tag), "      <c r=\"%s%u\"><v>%.2f</v></c>\n", col_name, curr_row + 1,
                                 cell->number_value);
                    }
                    buf_append_str(&sheet_bufs[s], c_tag);
                } else {
                    char c_open[128];
                    if (cell->bold) {
                        snprintf(c_open, sizeof(c_open), "      <c r=\"%s%u\" t=\"inlineStr\" s=\"1\"><is><t>",
                                 col_name, curr_row + 1);
                    } else {
                        snprintf(c_open, sizeof(c_open), "      <c r=\"%s%u\" t=\"inlineStr\"><is><t>", col_name,
                                 curr_row + 1);
                    }
                    buf_append_str(&sheet_bufs[s], c_open);
                    buf_append_escaped(&sheet_bufs[s], cell->str_value != NULL ? cell->str_value : "");
                    buf_append_str(&sheet_bufs[s], "</t></is></c>\n");
                }
            }
            if (curr_row != UINT32_MAX) {
                buf_append_str(&sheet_bufs[s], "    </row>\n");
            }
        }

        buf_append_str(&sheet_bufs[s], "  </sheetData>\n");
        buf_append_str(&sheet_bufs[s], "</worksheet>");

        size_t entry_idx = 5 + s;
        snprintf(zip_entries[entry_idx].filename, sizeof(zip_entries[entry_idx].filename), "xl/worksheets/sheet%zu.xml",
                 s + 1);
        zip_entries[entry_idx].uncomp_data = (unsigned char *)sheet_bufs[s].data;
        zip_entries[entry_idx].uncomp_size = sheet_bufs[s].size;
    }

    FILE *fp = fopen(workbook->filename, "wb");
    if (fp == NULL) {
        buf_free(&ct_buf);
        buf_free(&rels_buf);
        buf_free(&wb_rels_buf);
        buf_free(&wb_buf);
        buf_free(&styles_buf);
        for (size_t s = 0; s < workbook->sheet_count; s++) {
            buf_free(&sheet_bufs[s]);
        }
        free(zip_entries);
        for (size_t s = 0; s < workbook->sheet_count; s++) {
            for (size_t i = 0; i < workbook->sheets[s].cell_count; i++) {
                if (workbook->sheets[s].cells[i].str_value != NULL) {
                    free(workbook->sheets[s].cells[i].str_value);
                }
            }
            if (workbook->sheets[s].cells != NULL) {
                free(workbook->sheets[s].cells);
            }
        }
        free(workbook);
        return LXW_ERROR_CREATING_XLSX_FILE;
    }

    for (size_t i = 0; i < total_zip_entries; i++) {
        ZipFileEntry *ze = &zip_entries[i];
        if (compress_deflate(ze->uncomp_data, ze->uncomp_size, &ze->comp_data, &ze->comp_size, &ze->crc) != 0) {
            fclose(fp);
            return LXW_ERROR_ZIP_FILE_OPERATION;
        }

        ze->local_header_offset = (uint32_t)ftell(fp);

        unsigned char local_hdr[30];
        write_u32_le(local_hdr + 0, 0x04034b50);
        write_u16_le(local_hdr + 4, 20);
        write_u16_le(local_hdr + 6, 0);
        write_u16_le(local_hdr + 8, 8);
        write_u16_le(local_hdr + 10, 0);
        write_u16_le(local_hdr + 12, 0x0021);
        write_u32_le(local_hdr + 14, ze->crc);
        write_u32_le(local_hdr + 18, (uint32_t)ze->comp_size);
        write_u32_le(local_hdr + 22, (uint32_t)ze->uncomp_size);
        write_u16_le(local_hdr + 26, (uint16_t)strlen(ze->filename));
        write_u16_le(local_hdr + 28, 0);

        fwrite(local_hdr, 1, 30, fp);
        fwrite(ze->filename, 1, strlen(ze->filename), fp);
        fwrite(ze->comp_data, 1, ze->comp_size, fp);
        free(ze->comp_data);
        ze->comp_data = NULL;
    }

    uint32_t cd_offset = (uint32_t)ftell(fp);

    for (size_t i = 0; i < total_zip_entries; i++) {
        ZipFileEntry *ze = &zip_entries[i];
        unsigned char cd_hdr[46];
        write_u32_le(cd_hdr + 0, 0x02014b50);
        write_u16_le(cd_hdr + 4, 20);
        write_u16_le(cd_hdr + 6, 20);
        write_u16_le(cd_hdr + 8, 0);
        write_u16_le(cd_hdr + 10, 8);
        write_u16_le(cd_hdr + 12, 0);
        write_u16_le(cd_hdr + 14, 0x0021);
        write_u32_le(cd_hdr + 16, ze->crc);
        write_u32_le(cd_hdr + 20, (uint32_t)ze->comp_size);
        write_u32_le(cd_hdr + 24, (uint32_t)ze->uncomp_size);
        write_u16_le(cd_hdr + 28, (uint16_t)strlen(ze->filename));
        write_u16_le(cd_hdr + 30, 0);
        write_u16_le(cd_hdr + 32, 0);
        write_u16_le(cd_hdr + 34, 0);
        write_u16_le(cd_hdr + 36, 0);
        write_u32_le(cd_hdr + 38, 0);
        write_u32_le(cd_hdr + 42, ze->local_header_offset);

        fwrite(cd_hdr, 1, 46, fp);
        fwrite(ze->filename, 1, strlen(ze->filename), fp);
    }

    uint32_t cd_size = (uint32_t)ftell(fp) - cd_offset;

    unsigned char eocd[22];
    write_u32_le(eocd + 0, 0x06054b50);
    write_u16_le(eocd + 4, 0);
    write_u16_le(eocd + 6, 0);
    write_u16_le(eocd + 8, (uint16_t)total_zip_entries);
    write_u16_le(eocd + 10, (uint16_t)total_zip_entries);
    write_u32_le(eocd + 12, cd_size);
    write_u32_le(eocd + 16, cd_offset);
    write_u16_le(eocd + 20, 0);

    fwrite(eocd, 1, 22, fp);
    fclose(fp);

    buf_free(&ct_buf);
    buf_free(&rels_buf);
    buf_free(&wb_rels_buf);
    buf_free(&wb_buf);
    buf_free(&styles_buf);
    for (size_t s = 0; s < workbook->sheet_count; s++) {
        buf_free(&sheet_bufs[s]);
    }
    free(zip_entries);

    for (size_t s = 0; s < workbook->sheet_count; s++) {
        for (size_t i = 0; i < workbook->sheets[s].cell_count; i++) {
            if (workbook->sheets[s].cells[i].str_value != NULL) {
                free(workbook->sheets[s].cells[i].str_value);
            }
        }
        if (workbook->sheets[s].cells != NULL) {
            free(workbook->sheets[s].cells);
        }
    }
    free(workbook);

    return LXW_NO_ERROR;
}
