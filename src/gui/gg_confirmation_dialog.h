#ifndef GG_CONFIRMATION_DIALOG_H
#define GG_CONFIRMATION_DIALOG_H

#include "gg_gtk.h"
#include <stdbool.h>

typedef void (*GGConfirmationCallback)(bool confirmed, gpointer user_data);

void gg_confirmation_dialog_show(GtkWindow *parent, const char *title, const char *message, const char *detail,
                                 const char *confirm_button_label, const char *cancel_button_label, bool is_destructive,
                                 GGConfirmationCallback callback, gpointer user_data);

#endif
