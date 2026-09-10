#ifndef GG_DASHBOARD_WIDGET_H
#define GG_DASHBOARD_WIDGET_H

#include "gg_gtk.h"
#include "gg_gui_types.h"

GtkWidget *gg_dashboard_widget_create(GGAppContext *ctx);
void gg_dashboard_widget_refresh(GtkWidget *widget);

#endif
