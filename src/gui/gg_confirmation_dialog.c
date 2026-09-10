#include "gg_confirmation_dialog.h"
#include <stdlib.h>

typedef struct {
    GGConfirmationCallback callback;
    gpointer user_data;
} GGAlertState;

static void on_alert_response(GObject *source, GAsyncResult *result, gpointer user_data) {
    GtkAlertDialog *dialog = GTK_ALERT_DIALOG(source);
    GGAlertState *state = (GGAlertState *)user_data;
    GError *error = NULL;
    int button = gtk_alert_dialog_choose_finish(dialog, result, &error);
    if (error != NULL) {
        g_error_free(error);
        if (state->callback != NULL) {
            state->callback(false, state->user_data);
        }
        free(state);
        return;
    }

    bool confirmed = (button == 1);
    if (state->callback != NULL) {
        state->callback(confirmed, state->user_data);
    }
    free(state);
}

void gg_confirmation_dialog_show(
    GtkWindow *parent,
    const char *title,
    const char *message,
    const char *detail,
    const char *confirm_button_label,
    const char *cancel_button_label,
    bool is_destructive,
    GGConfirmationCallback callback,
    gpointer user_data
) {
    (void)title;
    (void)is_destructive;

    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", message != NULL ? message : "");
    if (detail != NULL) {
        gtk_alert_dialog_set_detail(dialog, detail);
    }

    const char *cancel_text = cancel_button_label != NULL ? cancel_button_label : "Cancel";
    const char *confirm_text = confirm_button_label != NULL ? confirm_button_label : "Confirm";
    const char *buttons[] = { cancel_text, confirm_text, NULL };
    gtk_alert_dialog_set_buttons(dialog, buttons);
    gtk_alert_dialog_set_cancel_button(dialog, 0);
    gtk_alert_dialog_set_default_button(dialog, 1);

    GGAlertState *state = (GGAlertState *)calloc(1, sizeof(GGAlertState));
    if (state == NULL) {
        g_object_unref(dialog);
        if (callback != NULL) {
            callback(false, user_data);
        }
        return;
    }
    state->callback = callback;
    state->user_data = user_data;

    gtk_alert_dialog_choose(dialog, parent, NULL, on_alert_response, state);
    g_object_unref(dialog);
}
