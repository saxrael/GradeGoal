#ifndef GG_SETTINGS_WIDGET_H
#define GG_SETTINGS_WIDGET_H

#include "gg_gtk.h"
#include "gg_gui_types.h"

GtkWidget *gg_settings_widget_create(GGAppContext *ctx);
void gg_settings_widget_refresh(GtkWidget *widget);

#endif
