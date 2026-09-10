#ifndef GG_BACKUP_MANAGER_H
#define GG_BACKUP_MANAGER_H

#include <time.h>
#include <stddef.h>
#include "io_types.h"

GGStatus gg_backup_create_snapshot(const char *db_filepath, const char *backup_dir);
GGStatus gg_backup_rotate(const char *backup_dir, size_t max_retained);
GGStatus gg_backup_get_last_timestamp(const char *backup_dir, time_t *out_timestamp);

#endif
