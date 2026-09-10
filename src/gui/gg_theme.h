#ifndef GG_THEME_H
#define GG_THEME_H

#include "../core/gg_types.h"
#include <stdbool.h>

GGStatus gg_theme_init(void);
void gg_theme_apply(bool prefer_dark);
void gg_theme_cleanup(void);

#endif
