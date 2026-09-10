#include "gg_theme.h"
#include "gg_gtk.h"
#include <gio/gio.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(__has_include)
#if __has_include(<fontconfig/fontconfig.h>)
#include <fontconfig/fontconfig.h>
#define GG_HAVE_FONTCONFIG 1
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#endif

extern GResource *gradegoal_get_resource(void);

static GtkCssProvider *g_theme_provider = NULL;
static gulong g_theme_signal_id = 0;

static void register_single_font_file(const char *font_path) {
    if (font_path == NULL) {
        return;
    }
#ifdef GG_HAVE_FONTCONFIG
    FcConfigAppFontAddFile(NULL, (const FcChar8 *)font_path);
#endif
#ifdef _WIN32
    AddFontResourceExA(font_path, FR_PRIVATE, 0);
#endif
}

static void ensure_font_file_available(const char *res_rel_path, const char *local_filename) {
    char disk_path[1024];
    snprintf(disk_path, sizeof(disk_path), "assets/fonts/%s", res_rel_path);
    if (g_file_test(disk_path, G_FILE_TEST_EXISTS)) {
        register_single_font_file(disk_path);
        return;
    }

    const char *cache_dir = g_get_user_cache_dir();
    char *target_dir = g_build_filename(cache_dir, "gradegoal", "fonts", NULL);
    g_mkdir_with_parents(target_dir, 0755);

    char *target_file = g_build_filename(target_dir, local_filename, NULL);

    if (!g_file_test(target_file, G_FILE_TEST_EXISTS)) {
        char res_path[1024];
        snprintf(res_path, sizeof(res_path), "/com/gradegoal/GradeGoal/fonts/%s", local_filename);
        GBytes *data = g_resources_lookup_data(res_path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
        if (data != NULL) {
            gsize size = 0;
            gconstpointer ptr = g_bytes_get_data(data, &size);
            if (ptr != NULL && size > 0) {
                GError *err = NULL;
                g_file_set_contents(target_file, (const gchar *)ptr, (gssize)size, &err);
                if (err != NULL) {
                    g_error_free(err);
                }
            }
            g_bytes_unref(data);
        }
    }

    if (g_file_test(target_file, G_FILE_TEST_EXISTS)) {
        register_single_font_file(target_file);
    }

    g_free(target_file);
    g_free(target_dir);
}

static void register_private_fonts(void) {
    ensure_font_file_available("PlusJakartaSans/PlusJakartaSans-Regular.ttf", "PlusJakartaSans-Regular.ttf");
    ensure_font_file_available("PlusJakartaSans/PlusJakartaSans-Medium.ttf", "PlusJakartaSans-Medium.ttf");
    ensure_font_file_available("PlusJakartaSans/PlusJakartaSans-SemiBold.ttf", "PlusJakartaSans-SemiBold.ttf");
    ensure_font_file_available("PlusJakartaSans/PlusJakartaSans-Bold.ttf", "PlusJakartaSans-Bold.ttf");
    ensure_font_file_available("SourceSerif4/SourceSerif4-SemiBold.ttf", "SourceSerif4-SemiBold.ttf");

#ifdef GG_HAVE_FONTCONFIG
    pango_cairo_font_map_set_default(NULL);
#endif
}

void gg_theme_apply(bool prefer_dark) {
    const char *css_res = prefer_dark ? "/com/gradegoal/GradeGoal/theme/gradegoal-dark.css"
                                      : "/com/gradegoal/GradeGoal/theme/gradegoal-light.css";

    if (g_theme_provider == NULL) {
        g_theme_provider = gtk_css_provider_new();
        GdkDisplay *display = gdk_display_get_default();
        if (display != NULL) {
            gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(g_theme_provider),
                                                       GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }
    }

    gtk_css_provider_load_from_resource(g_theme_provider, css_res);
}

static void on_prefer_dark_changed(GObject *object, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    (void)user_data;
    gboolean prefer_dark = FALSE;
    g_object_get(object, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
    gg_theme_apply(prefer_dark != FALSE);
}

GGStatus gg_theme_init(void) {
    g_resources_register(gradegoal_get_resource());

    register_private_fonts();

    GdkDisplay *display = gdk_display_get_default();
    if (display != NULL) {
        GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display(display);
        if (icon_theme != NULL) {
            gtk_icon_theme_add_resource_path(icon_theme, "/com/gradegoal/GradeGoal/icons");
        }
    }
    gtk_window_set_default_icon_name("gradegoal");

    GtkSettings *settings = gtk_settings_get_default();
    gboolean prefer_dark = FALSE;
    if (settings != NULL) {
        g_object_get(settings, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
        g_theme_signal_id = g_signal_connect(settings, "notify::gtk-application-prefer-dark-theme",
                                             G_CALLBACK(on_prefer_dark_changed), NULL);
    }

    gg_theme_apply(prefer_dark != FALSE);
    return GG_OK;
}

void gg_theme_cleanup(void) {
    if (g_theme_signal_id != 0) {
        GtkSettings *settings = gtk_settings_get_default();
        if (settings != NULL) {
            g_signal_handler_disconnect(settings, g_theme_signal_id);
        }
        g_theme_signal_id = 0;
    }

    if (g_theme_provider != NULL) {
        GdkDisplay *display = gdk_display_get_default();
        if (display != NULL) {
            gtk_style_context_remove_provider_for_display(display, GTK_STYLE_PROVIDER(g_theme_provider));
        }
        g_object_unref(g_theme_provider);
        g_theme_provider = NULL;
    }
}
