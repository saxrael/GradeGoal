#ifndef GG_XLSX_IMPORTER_H
#define GG_XLSX_IMPORTER_H

#include "io_types.h"
#include "../core/scale_repository.h"
#include "../core/course_repository.h"

GGStatus gg_xlsx_import(
    const char *filepath,
    GGScaleRepository *scale_repo,
    GGCourseRepository *course_repo,
    bool dry_run,
    GGImportSummary *summary
);

#endif
