#include "../include/hpdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char stream[16384];
    size_t stream_len;
} HPDF_Page_Internal;

typedef struct {
    HPDF_Error_Handler error_handler;
    void *user_data;
    HPDF_Page_Internal pages[32];
    size_t page_count;
} HPDF_Doc_Internal;

HPDF_Doc HPDF_New(HPDF_Error_Handler user_error_fn, void *user_data) {
    HPDF_Doc_Internal *doc = (HPDF_Doc_Internal *)calloc(1, sizeof(HPDF_Doc_Internal));
    if (doc == NULL) {
        return NULL;
    }
    doc->error_handler = user_error_fn;
    doc->user_data = user_data;
    return (HPDF_Doc)doc;
}

void HPDF_Free(HPDF_Doc pdf) {
    free(pdf);
}

HPDF_Font HPDF_GetFont(HPDF_Doc pdf, const char *font_name, const char *encoding_name) {
    (void)pdf;
    (void)font_name;
    (void)encoding_name;
    static int dummy_font = 1;
    return (HPDF_Font)&dummy_font;
}

HPDF_Page HPDF_AddPage(HPDF_Doc pdf) {
    if (pdf == NULL) {
        return NULL;
    }
    HPDF_Doc_Internal *doc = (HPDF_Doc_Internal *)pdf;
    if (doc->page_count >= 32) {
        return NULL;
    }
    HPDF_Page_Internal *page = &doc->pages[doc->page_count++];
    memset(page, 0, sizeof(*page));
    return (HPDF_Page)page;
}

HPDF_STATUS HPDF_Page_SetSize(HPDF_Page page, int size, int direction) {
    (void)page;
    (void)size;
    (void)direction;
    return HPDF_OK;
}

HPDF_STATUS HPDF_Page_BeginText(HPDF_Page page) {
    (void)page;
    return HPDF_OK;
}

HPDF_STATUS HPDF_Page_EndText(HPDF_Page page) {
    (void)page;
    return HPDF_OK;
}

HPDF_STATUS HPDF_Page_SetFontAndSize(HPDF_Page page, HPDF_Font font, float size) {
    (void)page;
    (void)font;
    (void)size;
    return HPDF_OK;
}

HPDF_STATUS HPDF_Page_MoveTextPos(HPDF_Page page, float x, float y) {
    (void)page;
    (void)x;
    (void)y;
    return HPDF_OK;
}

HPDF_STATUS HPDF_Page_ShowText(HPDF_Page page, const char *text) {
    if (page == NULL || text == NULL) {
        return 1;
    }
    HPDF_Page_Internal *p = (HPDF_Page_Internal *)page;
    size_t len = strlen(text);
    if (p->stream_len + len + 2 < sizeof(p->stream)) {
        strncat(p->stream, text, sizeof(p->stream) - p->stream_len - 1);
        strncat(p->stream, "\n", sizeof(p->stream) - p->stream_len - 1);
        p->stream_len = strlen(p->stream);
    }
    return HPDF_OK;
}

HPDF_STATUS HPDF_Page_SetLineWidth(HPDF_Page page, float size) {
    (void)page;
    (void)size;
    return HPDF_OK;
}

HPDF_STATUS HPDF_Page_MoveTo(HPDF_Page page, float x, float y) {
    (void)page;
    (void)x;
    (void)y;
    return HPDF_OK;
}

HPDF_STATUS HPDF_Page_LineTo(HPDF_Page page, float x, float y) {
    (void)page;
    (void)x;
    (void)y;
    return HPDF_OK;
}

HPDF_STATUS HPDF_Page_Stroke(HPDF_Page page) {
    (void)page;
    return HPDF_OK;
}

HPDF_STATUS HPDF_SaveToFile(HPDF_Doc pdf, const char *file_name) {
    if (pdf == NULL || file_name == NULL) {
        return 1;
    }
    HPDF_Doc_Internal *doc = (HPDF_Doc_Internal *)pdf;
    FILE *fp = fopen(file_name, "wb");
    if (fp == NULL) {
        return 1;
    }

    fprintf(fp, "%%PDF-1.4\n");
    fprintf(fp, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    fprintf(fp, "2 0 obj\n<< /Type /Pages /Count %zu /Kids [", doc->page_count);
    for (size_t i = 0; i < doc->page_count; i++) {
        fprintf(fp, " %zu 0 R", 3 + i * 2);
    }
    fprintf(fp, " ] >>\nendobj\n");

    for (size_t i = 0; i < doc->page_count; i++) {
        size_t page_obj_id = 3 + i * 2;
        size_t content_obj_id = page_obj_id + 1;
        HPDF_Page_Internal *p = &doc->pages[i];

        fprintf(fp, "%zu 0 obj\n<< /Type /Page /Parent 2 0 R /Contents %zu 0 R >>\nendobj\n",
                page_obj_id, content_obj_id);
        fprintf(fp, "%zu 0 obj\n<< /Length %zu >>\nstream\n%sendstream\nendobj\n",
                content_obj_id, p->stream_len, p->stream);
    }

    fprintf(fp, "xref\n0 %zu\n0000000000 65535 f \n", 3 + doc->page_count * 2);
    fprintf(fp, "trailer\n<< /Size %zu /Root 1 0 R >>\nstartxref\n100\n%%%%EOF\n", 3 + doc->page_count * 2);

    fclose(fp);
    return HPDF_OK;
}
