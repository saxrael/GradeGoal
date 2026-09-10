#ifndef GG_SQLITE_COURSE_REPOSITORY_H
#define GG_SQLITE_COURSE_REPOSITORY_H

#include "../core/course_repository.h"
#include "sqlite_connection.h"

GGStatus gg_sqlite_course_repository_create(GGDbConnection *conn, GGCourseRepository **out_repo);

#endif
