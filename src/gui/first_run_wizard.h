#ifndef GG_FIRST_RUN_WIZARD_H
#define GG_FIRST_RUN_WIZARD_H

#include "gg_gtk.h"
#include "gg_gui_types.h"

GtkWidget *gg_first_run_wizard_create(GtkWindow *parent, GGAppContext *ctx, GCallback on_finished, gpointer user_data);

#endif
