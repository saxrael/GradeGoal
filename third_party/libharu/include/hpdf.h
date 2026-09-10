#ifndef HPDF_H
#define HPDF_H

#include <stddef.h>

#define HPDF_PAGE_SIZE_A4 0
#define HPDF_PAGE_PORTRAIT 0
#define HPDF_OK 0

typedef void* HPDF_Doc;
typedef void* HPDF_Page;
typedef void* HPDF_Font;
typedef unsigned long HPDF_STATUS;

typedef void (*HPDF_Error_Handler)(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data);

HPDF_Doc HPDF_New(HPDF_Error_Handler user_error_fn, void *user_data);
void HPDF_Free(HPDF_Doc pdf);
HPDF_Font HPDF_GetFont(HPDF_Doc pdf, const char *font_name, const char *encoding_name);
HPDF_Page HPDF_AddPage(HPDF_Doc pdf);
HPDF_STATUS HPDF_Page_SetSize(HPDF_Page page, int size, int direction);
HPDF_STATUS HPDF_Page_BeginText(HPDF_Page page);
HPDF_STATUS HPDF_Page_EndText(HPDF_Page page);
HPDF_STATUS HPDF_Page_SetFontAndSize(HPDF_Page page, HPDF_Font font, float size);
HPDF_STATUS HPDF_Page_MoveTextPos(HPDF_Page page, float x, float y);
HPDF_STATUS HPDF_Page_ShowText(HPDF_Page page, const char *text);
HPDF_STATUS HPDF_Page_SetLineWidth(HPDF_Page page, float size);
HPDF_STATUS HPDF_Page_MoveTo(HPDF_Page page, float x, float y);
HPDF_STATUS HPDF_Page_LineTo(HPDF_Page page, float x, float y);
HPDF_STATUS HPDF_Page_Stroke(HPDF_Page page);
HPDF_STATUS HPDF_SaveToFile(HPDF_Doc pdf, const char *file_name);

#endif
