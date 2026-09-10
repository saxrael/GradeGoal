#include "gg_grade_row.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GGGradeRow *gg_grade_row_create(const GGGradeItem *initial_item, GCallback on_changed, GCallback on_remove,
                                gpointer user_data) {
    GGGradeRow *row = (GGGradeRow *)calloc(1, sizeof(GGGradeRow));
    if (row == NULL) {
        return NULL;
    }

    row->container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(row->container, 4);
    gtk_widget_set_margin_bottom(row->container, 4);

    row->symbol_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(row->symbol_entry), "Symbol (e.g. A)");
    gtk_entry_set_max_length(GTK_ENTRY(row->symbol_entry), 7);
    gtk_widget_set_hexpand(row->symbol_entry, TRUE);

    row->point_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(row->point_entry), "Points (e.g. 5.0)");
    gtk_widget_set_hexpand(row->point_entry, TRUE);

    if (initial_item != NULL) {
        gtk_editable_set_text(GTK_EDITABLE(row->symbol_entry), initial_item->grade_symbol);
        char point_buf[32];
        snprintf(point_buf, sizeof(point_buf), "%.2f", initial_item->grade_point);
        gtk_editable_set_text(GTK_EDITABLE(row->point_entry), point_buf);
    }

    row->remove_button = gtk_button_new_with_label("Remove");
    gtk_widget_add_css_class(row->remove_button, "action-btn-sm");
    gtk_widget_add_css_class(row->remove_button, "destructive-action");
    row->error_label = gtk_label_new("");
    gtk_widget_add_css_class(row->error_label, "error");

    gtk_box_append(GTK_BOX(row->container), row->symbol_entry);
    gtk_box_append(GTK_BOX(row->container), row->point_entry);
    gtk_box_append(GTK_BOX(row->container), row->remove_button);
    gtk_box_append(GTK_BOX(row->container), row->error_label);

    if (on_changed != NULL) {
        g_signal_connect(row->symbol_entry, "changed", on_changed, user_data);
        g_signal_connect(row->point_entry, "changed", on_changed, user_data);
    }
    if (on_remove != NULL) {
        g_signal_connect_swapped(row->remove_button, "clicked", on_remove, row);
    }

    return row;
}

void gg_grade_row_get_item(const GGGradeRow *row, GGGradeItem *out_item) {
    if (row == NULL || out_item == NULL) {
        return;
    }
    const char *sym = gtk_editable_get_text(GTK_EDITABLE(row->symbol_entry));
    const char *pt = gtk_editable_get_text(GTK_EDITABLE(row->point_entry));
    snprintf(out_item->grade_symbol, sizeof(out_item->grade_symbol), "%s", sym != NULL ? sym : "");
    out_item->grade_point = pt != NULL ? strtod(pt, NULL) : 0.0;
}

void gg_grade_row_set_error(GGGradeRow *row, const char *error_message) {
    if (row != NULL && row->error_label != NULL) {
        gtk_label_set_text(GTK_LABEL(row->error_label), error_message != NULL ? error_message : "");
    }
}

void gg_grade_row_clear_error(GGGradeRow *row) {
    if (row != NULL && row->error_label != NULL) {
        gtk_label_set_text(GTK_LABEL(row->error_label), "");
    }
}

void gg_grade_row_destroy(GGGradeRow *row) {
    if (row != NULL) {
        free(row);
    }
}
