#include "app.h"
#include "../persistence/course_repository.h"
#include "../persistence/scale_repository.h"
#include "../persistence/sqlite_connection.h"
#include "gg_theme.h"
#include "main_window.h"
#include <stdlib.h>

static GGDbConnection *g_db_conn = NULL;
static GGScaleRepository *g_scale_repo = NULL;
static GGCourseRepository *g_course_repo = NULL;
static GGAppContext g_app_ctx;

static void on_app_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    const char *db_path = "gradegoal.db";
    const char *backup_path = "backups";

    gg_theme_init();

    GGStatus status = gg_db_connect(db_path, &g_db_conn);
    if (status != GG_OK) {
        return;
    }

    status = gg_db_bootstrap_schema(g_db_conn);
    if (status != GG_OK) {
        gg_db_disconnect(g_db_conn);
        return;
    }

    status = gg_sqlite_scale_repository_create(g_db_conn, &g_scale_repo);
    if (status != GG_OK) {
        gg_db_disconnect(g_db_conn);
        return;
    }

    status = gg_sqlite_course_repository_create(g_db_conn, &g_course_repo);
    if (status != GG_OK) {
        g_scale_repo->destroy(g_scale_repo);
        gg_db_disconnect(g_db_conn);
        return;
    }

    g_app_ctx.app = app;
    g_app_ctx.main_window = NULL;
    g_app_ctx.scale_repo = g_scale_repo;
    g_app_ctx.course_repo = g_course_repo;
    g_app_ctx.db_filepath = db_path;
    g_app_ctx.backup_dir = backup_path;

    GtkWidget *win = gg_main_window_create(&g_app_ctx);
    if (win != NULL) {
        gtk_window_present(GTK_WINDOW(win));
    }
}

static void on_app_shutdown(GApplication *app, gpointer user_data) {
    (void)app;
    (void)user_data;

    gg_theme_cleanup();

    if (g_course_repo != NULL) {
        g_course_repo->destroy(g_course_repo);
        g_course_repo = NULL;
    }
    if (g_scale_repo != NULL) {
        g_scale_repo->destroy(g_scale_repo);
        g_scale_repo = NULL;
    }
    if (g_db_conn != NULL) {
        gg_db_disconnect(g_db_conn);
        g_db_conn = NULL;
    }
}

int gg_app_run(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.gradegoal.GradeGoal", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK(on_app_shutdown), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
