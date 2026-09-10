#ifndef GG_PDF_EXPORTER_H
#define GG_PDF_EXPORTER_H

#include "io_types.h"
#include "../core/scale_repository.h"
#include "../core/course_repository.h"

GGStatus gg_pdf_export(const char *filepath, GGScaleRepository *scale_repo, GGCourseRepository *course_repo);

#endif
