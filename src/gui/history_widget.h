#ifndef GG_HISTORY_WIDGET_H
#define GG_HISTORY_WIDGET_H

#include "gg_gtk.h"
#include "gg_gui_types.h"

GtkWidget *gg_history_widget_create(GGAppContext *ctx);
void gg_history_widget_refresh(GtkWidget *widget);

#endif
