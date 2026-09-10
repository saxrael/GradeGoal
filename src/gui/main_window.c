#include "main_window.h"
#include "dashboard_widget.h"
#include "first_run_wizard.h"
#include "history_widget.h"
#include "settings_widget.h"
#include "target_calculator_widget.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    GGAppContext *ctx;
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *dashboard_view;
    GtkWidget *history_view;
    GtkWidget *target_view;
    GtkWidget *settings_view;
} GGMainWindowState;

static void on_wizard_completed(gpointer user_data) {
    GGMainWindowState *state = (GGMainWindowState *)user_data;
    if (state != NULL) {
        gg_dashboard_widget_refresh(state->dashboard_view);
        gg_history_widget_refresh(state->history_view);
        gg_settings_widget_refresh(state->settings_view);
    }
}

static void on_stack_page_changed(GObject *object, GParamSpec *pspec, gpointer user_data) {
    (void)object;
    (void)pspec;
    GGMainWindowState *state = (GGMainWindowState *)user_data;
    if (state != NULL) {
        const char *visible = gtk_stack_get_visible_child_name(GTK_STACK(state->stack));
        if (visible != NULL) {
            if (strcmp(visible, "dashboard") == 0) {
                gg_dashboard_widget_refresh(state->dashboard_view);
            } else if (strcmp(visible, "history") == 0) {
                gg_history_widget_refresh(state->history_view);
            } else if (strcmp(visible, "settings") == 0) {
                gg_settings_widget_refresh(state->settings_view);
            }
        }
    }
}

GtkWidget *gg_main_window_create(GGAppContext *ctx) {
    GGMainWindowState *state = (GGMainWindowState *)calloc(1, sizeof(GGMainWindowState));
    if (state == NULL) {
        return NULL;
    }
    state->ctx = ctx;

    state->window = gtk_application_window_new(ctx->app);
    ctx->main_window = GTK_WINDOW(state->window);
    gtk_window_set_title(GTK_WINDOW(state->window), "GradeGoal");
    gtk_window_set_icon_name(GTK_WINDOW(state->window), "gradegoal");
    gtk_window_set_default_size(GTK_WINDOW(state->window), 960, 680);

    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(state->window), header_bar);

    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *app_icon = gtk_image_new_from_icon_name("gradegoal");
    gtk_image_set_pixel_size(GTK_IMAGE(app_icon), 22);
    GtkWidget *app_title = gtk_label_new("GradeGoal");
    gtk_widget_add_css_class(app_title, "title");
    gtk_box_append(GTK_BOX(title_box), app_icon);
    gtk_box_append(GTK_BOX(title_box), app_title);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), title_box);

    state->stack = gtk_stack_new();
    gtk_widget_set_vexpand(state->stack, TRUE);
    gtk_widget_set_hexpand(state->stack, TRUE);

    GtkWidget *switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher), GTK_STACK(state->stack));
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header_bar), switcher);

    state->dashboard_view = gg_dashboard_widget_create(ctx);
    state->history_view = gg_history_widget_create(ctx);
    state->target_view = gg_target_calculator_widget_create(ctx);
    state->settings_view = gg_settings_widget_create(ctx);

    GtkStackPage *p1 = gtk_stack_add_titled(GTK_STACK(state->stack), state->dashboard_view, "dashboard", "Dashboard");
    (void)p1;
    GtkStackPage *p2 = gtk_stack_add_titled(GTK_STACK(state->stack), state->history_view, "history", "History");
    (void)p2;
    GtkStackPage *p3 = gtk_stack_add_titled(GTK_STACK(state->stack), state->target_view, "target", "Target Solver");
    (void)p3;
    GtkStackPage *p4 = gtk_stack_add_titled(GTK_STACK(state->stack), state->settings_view, "settings", "Settings");
    (void)p4;

    g_signal_connect(state->stack, "notify::visible-child-name", G_CALLBACK(on_stack_page_changed), state);

    gtk_window_set_child(GTK_WINDOW(state->window), state->stack);

    GGGradingScale scale;
    memset(&scale, 0, sizeof(scale));
    GGStatus st =
        ctx->scale_repo != NULL ? ctx->scale_repo->load_scale(ctx->scale_repo->context, &scale) : GG_ERR_NOT_FOUND;
    if (st != GG_OK || scale.count == 0) {
        GtkWidget *wizard =
            gg_first_run_wizard_create(GTK_WINDOW(state->window), ctx, G_CALLBACK(on_wizard_completed), state);
        if (wizard != NULL) {
            gtk_window_present(GTK_WINDOW(wizard));
        }
    }

    g_object_set_data_full(G_OBJECT(state->window), "state", state, free);

    return state->window;
}
