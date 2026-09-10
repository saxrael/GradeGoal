#ifndef GG_SQLITE_SCALE_REPOSITORY_H
#define GG_SQLITE_SCALE_REPOSITORY_H

#include "../core/scale_repository.h"
#include "sqlite_connection.h"

GGStatus gg_sqlite_scale_repository_create(GGDbConnection *conn, GGScaleRepository **out_repo);

#endif
