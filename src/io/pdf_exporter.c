#include "pdf_exporter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hpdf.h>
#include "../core/cgpa_calculator.h"
#include "../core/course_list.h"

static void pdf_error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data) {
    (void)error_no;
    (void)detail_no;
    (void)user_data;
}

GGStatus gg_pdf_export(const char *filepath, GGScaleRepository *scale_repo, GGCourseRepository *course_repo) {
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

    double tcp = 0.0;
    uint32_t tcu = 0;
    status = course_repo->get_live_totals(course_repo->context, &tcp, &tcu);
    if (status != GG_OK) {
        gg_course_list_destroy(courses);
        return status;
    }

    double cgpa = 0.0;
    status = gg_cgpa_calculate(tcp, tcu, &cgpa);
    if (status != GG_OK) {
        gg_course_list_destroy(courses);
        return status;
    }

    HPDF_Doc pdf = HPDF_New(pdf_error_handler, NULL);
    if (pdf == NULL) {
        gg_course_list_destroy(courses);
        return GG_ERR_IO;
    }

    HPDF_Font bold_font = HPDF_GetFont(pdf, "Helvetica-Bold", NULL);
    HPDF_Font regular_font = HPDF_GetFont(pdf, "Helvetica", NULL);

    HPDF_Page page = HPDF_AddPage(pdf);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);

    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, bold_font, 18);
    HPDF_Page_MoveTextPos(page, 50, 790);
    HPDF_Page_ShowText(page, "GradeGoal - Academic Standing Report");
    HPDF_Page_EndText(page);

    char summary_text[128];
    snprintf(summary_text, sizeof(summary_text), "Cumulative CGPA: %.2f  |  Total Points (TCP): %.2f  |  Total Units (TCU): %u", cgpa, tcp, tcu);
    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, regular_font, 11);
    HPDF_Page_MoveTextPos(page, 50, 765);
    HPDF_Page_ShowText(page, summary_text);
    HPDF_Page_EndText(page);

    HPDF_Page_SetLineWidth(page, 0.5);
    HPDF_Page_MoveTo(page, 50, 750);
    HPDF_Page_LineTo(page, 545, 750);
    HPDF_Page_Stroke(page);

    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, bold_font, 12);
    HPDF_Page_MoveTextPos(page, 50, 730);
    HPDF_Page_ShowText(page, "Course History");
    HPDF_Page_EndText(page);

    float y = 705.0f;
    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, bold_font, 10);
    HPDF_Page_MoveTextPos(page, 50, y);
    HPDF_Page_ShowText(page, "Semester");
    HPDF_Page_MoveTextPos(page, 130, 0);
    HPDF_Page_ShowText(page, "Course");
    HPDF_Page_MoveTextPos(page, 180, 0);
    HPDF_Page_ShowText(page, "Units");
    HPDF_Page_MoveTextPos(page, 70, 0);
    HPDF_Page_ShowText(page, "Grade");
    HPDF_Page_EndText(page);

    y -= 15.0f;
    HPDF_Page_MoveTo(page, 50, y + 10.0f);
    HPDF_Page_LineTo(page, 545, y + 10.0f);
    HPDF_Page_Stroke(page);

    for (size_t i = 0; i < courses->count; i++) {
        if (y < 60.0f) {
            page = HPDF_AddPage(pdf);
            HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
            y = 780.0f;

            HPDF_Page_BeginText(page);
            HPDF_Page_SetFontAndSize(page, bold_font, 10);
            HPDF_Page_MoveTextPos(page, 50, y);
            HPDF_Page_ShowText(page, "Semester");
            HPDF_Page_MoveTextPos(page, 130, 0);
            HPDF_Page_ShowText(page, "Course");
            HPDF_Page_MoveTextPos(page, 180, 0);
            HPDF_Page_ShowText(page, "Units");
            HPDF_Page_MoveTextPos(page, 70, 0);
            HPDF_Page_ShowText(page, "Grade");
            HPDF_Page_EndText(page);

            y -= 15.0f;
            HPDF_Page_MoveTo(page, 50, y + 10.0f);
            HPDF_Page_LineTo(page, 545, y + 10.0f);
            HPDF_Page_Stroke(page);
        }

        const GGCourseEntry *entry = &courses->entries[i];
        char units_str[16];
        snprintf(units_str, sizeof(units_str), "%u", entry->credit_unit);

        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, regular_font, 9);
        HPDF_Page_MoveTextPos(page, 50, y);
        HPDF_Page_ShowText(page, entry->semester_label);
        HPDF_Page_MoveTextPos(page, 130, 0);
        HPDF_Page_ShowText(page, entry->course_label);
        HPDF_Page_MoveTextPos(page, 180, 0);
        HPDF_Page_ShowText(page, units_str);
        HPDF_Page_MoveTextPos(page, 70, 0);
        HPDF_Page_ShowText(page, entry->grade_symbol);
        HPDF_Page_EndText(page);

        y -= 18.0f;
    }

    HPDF_STATUS hpdf_status = HPDF_SaveToFile(pdf, filepath);
    HPDF_Free(pdf);
    gg_course_list_destroy(courses);

    if (hpdf_status != HPDF_OK) {
        return GG_ERR_IO;
    }

    return GG_OK;
}
