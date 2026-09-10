#include "scale_validator.h"
#include <string.h>

GGStatus gg_scale_validator_validate_scale(const GGGradingScale *scale) {
    if (scale == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    if (scale->count == 0 || scale->count > 16) {
        return GG_ERR_VALIDATION;
    }

    for (size_t i = 0; i < scale->count; i++) {
        if (scale->items[i].grade_point < 0.0) {
            return GG_ERR_VALIDATION;
        }

        if (scale->items[i].grade_symbol[0] == '\0') {
            return GG_ERR_VALIDATION;
        }

        if (memchr(scale->items[i].grade_symbol, '\0', sizeof(scale->items[i].grade_symbol)) == NULL) {
            return GG_ERR_VALIDATION;
        }
    }

    for (size_t i = 0; i < scale->count; i++) {
        for (size_t j = i + 1; j < scale->count; j++) {
            if (strncmp(scale->items[i].grade_symbol, scale->items[j].grade_symbol,
                        sizeof(scale->items[i].grade_symbol)) == 0) {
                return GG_ERR_VALIDATION;
            }
        }
    }

    for (size_t i = 0; i + 1 < scale->count; i++) {
        if (scale->items[i].grade_point <= scale->items[i + 1].grade_point) {
            return GG_ERR_VALIDATION;
        }
    }

    if (scale->max_point != scale->items[0].grade_point) {
        return GG_ERR_VALIDATION;
    }

    if (scale->min_point != scale->items[scale->count - 1].grade_point) {
        return GG_ERR_VALIDATION;
    }

    return GG_OK;
}

GGStatus gg_scale_validator_validate_course(const GGGradingScale *scale, const GGCourseEntry *entry) {
    if (scale == NULL || entry == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    if (entry->credit_unit == 0) {
        return GG_ERR_VALIDATION;
    }

    if (entry->semester_label[0] == '\0') {
        return GG_ERR_VALIDATION;
    }

    if (memchr(entry->semester_label, '\0', sizeof(entry->semester_label)) == NULL) {
        return GG_ERR_VALIDATION;
    }

    if (memchr(entry->course_label, '\0', sizeof(entry->course_label)) == NULL) {
        return GG_ERR_VALIDATION;
    }

    if (memchr(entry->grade_symbol, '\0', sizeof(entry->grade_symbol)) == NULL) {
        return GG_ERR_VALIDATION;
    }

    if (entry->grade_symbol[0] == '\0') {
        return GG_ERR_VALIDATION;
    }

    if (scale->count == 0 || scale->count > 16) {
        return GG_ERR_VALIDATION;
    }

    bool matched = false;
    for (size_t i = 0; i < scale->count; i++) {
        if (strncmp(entry->grade_symbol, scale->items[i].grade_symbol, sizeof(entry->grade_symbol)) == 0) {
            matched = true;
            break;
        }
    }

    if (!matched) {
        return GG_ERR_VALIDATION;
    }

    return GG_OK;
}

GGStatus ggvalidate_course_entry(const GGGradingScale *scale, const GGCourseEntry *entry) {
    return gg_scale_validator_validate_course(scale, entry);
}
