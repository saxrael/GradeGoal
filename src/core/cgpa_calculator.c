#include "cgpa_calculator.h"

GGStatus gg_cgpa_calculate(double total_tcp, uint32_t total_tcu, double *out_cgpa) {
    if (out_cgpa == NULL || total_tcp < 0.0) {
        return GG_ERR_INVALID_ARG;
    }

    if (total_tcu == 0) {
        *out_cgpa = 0.0;
        return GG_OK;
    }

    *out_cgpa = total_tcp / (double)total_tcu;
    return GG_OK;
}

GGStatus gg_cgpa_combine_standing(double prior_tcp, uint32_t prior_tcu, double course_tcp, uint32_t course_tcu,
                                  double *out_cgpa, double *out_combined_tcp, uint32_t *out_combined_tcu) {
    if (out_cgpa == NULL || out_combined_tcp == NULL || out_combined_tcu == NULL) {
        return GG_ERR_INVALID_ARG;
    }

    if (prior_tcp < 0.0 || course_tcp < 0.0) {
        return GG_ERR_INVALID_ARG;
    }

    *out_combined_tcp = prior_tcp + course_tcp;
    *out_combined_tcu = prior_tcu + course_tcu;

    return gg_cgpa_calculate(*out_combined_tcp, *out_combined_tcu, out_cgpa);
}
