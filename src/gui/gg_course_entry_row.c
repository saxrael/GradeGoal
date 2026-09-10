#include "gg_course_entry_row.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GGCourseEntryRow *gg_course_entry_row_create(const GGGradingScale *scale, const GGCourseEntry *initial_entry,
                                             bool show_grade_combo, GCallback on_changed, GCallback on_remove,
                                             gpointer user_data) {
    GGCourseEntryRow *row = (GGCourseEntryRow *)calloc(1, sizeof(GGCourseEntryRow));
    if (row == NULL) {
        return NULL;
    }

    row->container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(row->container, 4);
    gtk_widget_set_margin_bottom(row->container, 4);

    row->course_label_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(row->course_label_entry), "Course Code (e.g. CSC401)");
    gtk_entry_set_max_length(GTK_ENTRY(row->course_label_entry), 31);
    gtk_widget_set_hexpand(row->course_label_entry, TRUE);

    row->credit_unit_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(row->credit_unit_entry), "Units (e.g. 3)");
    gtk_entry_set_max_length(GTK_ENTRY(row->credit_unit_entry), 8);
    gtk_widget_set_size_request(row->credit_unit_entry, 100, -1);

    if (show_grade_combo && scale != NULL && scale->count > 0) {
        const char *symbols[17];
        for (size_t i = 0; i < scale->count; i++) {
            symbols[i] = scale->items[i].grade_symbol;
        }
        symbols[scale->count] = NULL;
        GtkStringList *string_list = gtk_string_list_new(symbols);
        row->grade_combo = gtk_drop_down_new(G_LIST_MODEL(string_list), NULL);
        gtk_widget_set_size_request(row->grade_combo, 100, -1);
    } else {
        row->grade_combo = NULL;
    }

    row->remove_button = gtk_button_new_with_label("Remove");
    gtk_widget_add_css_class(row->remove_button, "action-btn-sm");
    gtk_widget_add_css_class(row->remove_button, "destructive-action");
    row->unit_error_label = gtk_label_new("");
    gtk_widget_add_css_class(row->unit_error_label, "error");

    gtk_box_append(GTK_BOX(row->container), row->course_label_entry);
    gtk_box_append(GTK_BOX(row->container), row->credit_unit_entry);
    if (row->grade_combo != NULL) {
        gtk_box_append(GTK_BOX(row->container), row->grade_combo);
    }
    gtk_box_append(GTK_BOX(row->container), row->remove_button);
    gtk_box_append(GTK_BOX(row->container), row->unit_error_label);

    if (initial_entry != NULL) {
        gtk_editable_set_text(GTK_EDITABLE(row->course_label_entry), initial_entry->course_label);
        char units_buf[16];
        snprintf(units_buf, sizeof(units_buf), "%u", initial_entry->credit_unit);
        gtk_editable_set_text(GTK_EDITABLE(row->credit_unit_entry), units_buf);

        if (row->grade_combo != NULL && scale != NULL) {
            for (size_t i = 0; i < scale->count; i++) {
                if (strcmp(scale->items[i].grade_symbol, initial_entry->grade_symbol) == 0) {
                    gtk_drop_down_set_selected(GTK_DROP_DOWN(row->grade_combo), (guint)i);
                    break;
                }
            }
        }
    }

    if (on_changed != NULL) {
        g_signal_connect(row->course_label_entry, "changed", on_changed, user_data);
        g_signal_connect(row->credit_unit_entry, "changed", on_changed, user_data);
        if (row->grade_combo != NULL) {
            g_signal_connect(row->grade_combo, "notify::selected", on_changed, user_data);
        }
    }
    if (on_remove != NULL) {
        g_signal_connect_swapped(row->remove_button, "clicked", on_remove, row);
    }

    return row;
}

void gg_course_entry_row_get_entry(const GGCourseEntryRow *row, GGCourseEntry *out_entry) {
    if (row == NULL || out_entry == NULL) {
        return;
    }
    const char *lbl = gtk_editable_get_text(GTK_EDITABLE(row->course_label_entry));
    const char *units_str = gtk_editable_get_text(GTK_EDITABLE(row->credit_unit_entry));
    snprintf(out_entry->course_label, sizeof(out_entry->course_label), "%s", lbl != NULL ? lbl : "");
    out_entry->credit_unit = units_str != NULL ? (uint32_t)strtoul(units_str, NULL, 10) : 0;

    if (row->grade_combo != NULL) {
        guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(row->grade_combo));
        GListModel *model = gtk_drop_down_get_model(GTK_DROP_DOWN(row->grade_combo));
        const char *sym = gtk_string_list_get_string(GTK_STRING_LIST(model), selected);
        snprintf(out_entry->grade_symbol, sizeof(out_entry->grade_symbol), "%s", sym != NULL ? sym : "");
    }
}

bool gg_course_entry_row_validate(GGCourseEntryRow *row, const GGGradingScale *scale) {
    if (row == NULL) {
        return false;
    }
    const char *units_str = gtk_editable_get_text(GTK_EDITABLE(row->credit_unit_entry));
    unsigned long units = units_str != NULL ? strtoul(units_str, NULL, 10) : 0;
    if (units == 0) {
        gtk_label_set_text(GTK_LABEL(row->unit_error_label), "Units must be > 0");
        return false;
    }

    if (row->grade_combo != NULL && scale != NULL) {
        guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(row->grade_combo));
        if (selected == GTK_INVALID_LIST_POSITION || (size_t)selected >= scale->count) {
            gtk_label_set_text(GTK_LABEL(row->unit_error_label), "Invalid grade");
            return false;
        }
    }

    gtk_label_set_text(GTK_LABEL(row->unit_error_label), "");
    return true;
}

void gg_course_entry_row_destroy(GGCourseEntryRow *row) {
    if (row != NULL) {
        free(row);
    }
}
