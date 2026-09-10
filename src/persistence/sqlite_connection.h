#ifndef GG_SQLITE_CONNECTION_H
#define GG_SQLITE_CONNECTION_H

#include <stdbool.h>
#include "gg_types.h"

typedef struct sqlite3 sqlite3;
typedef struct GGDbConnection GGDbConnection;

GGStatus gg_db_connect(const char *filepath, GGDbConnection **out_conn);
GGStatus gg_db_disconnect(GGDbConnection *conn);
GGStatus gg_db_begin_immediate(GGDbConnection *conn);
GGStatus gg_db_commit(GGDbConnection *conn);
GGStatus gg_db_rollback(GGDbConnection *conn);
GGStatus gg_db_bootstrap_schema(GGDbConnection *conn);
sqlite3 *gg_db_get_handle(GGDbConnection *conn);

#endif
