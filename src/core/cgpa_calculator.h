#ifndef GG_CGPA_CALCULATOR_H
#define GG_CGPA_CALCULATOR_H

#include "gg_types.h"

GGStatus gg_cgpa_calculate(double total_tcp, uint32_t total_tcu, double *out_cgpa);
GGStatus gg_cgpa_combine_standing(double prior_tcp, uint32_t prior_tcu, double course_tcp, uint32_t course_tcu,
                                  double *out_cgpa, double *out_combined_tcp, uint32_t *out_combined_tcu);

#endif
