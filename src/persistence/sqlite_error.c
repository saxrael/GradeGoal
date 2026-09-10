#include "sqlite_error.h"
#include <sqlite3.h>

GGStatus gg_status_from_sqlite(int sqlite_code) {
    switch (sqlite_code & 0xFF) {
    case SQLITE_OK:
    case SQLITE_DONE:
    case SQLITE_ROW:
        return GG_OK;
    case SQLITE_NOMEM:
        return GG_ERR_NOMEM;
    case SQLITE_CONSTRAINT:
    case SQLITE_MISMATCH:
        return GG_ERR_VALIDATION;
    case SQLITE_NOTFOUND:
        return GG_ERR_NOT_FOUND;
    case SQLITE_BUSY:
    case SQLITE_LOCKED:
        return GG_ERR_DB;
    case SQLITE_IOERR:
    case SQLITE_CORRUPT:
    case SQLITE_CANTOPEN:
    case SQLITE_FULL:
        return GG_ERR_IO;
    default:
        return GG_ERR_DB;
    }
}
