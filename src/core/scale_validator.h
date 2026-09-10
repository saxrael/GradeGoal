#ifndef GG_SCALE_VALIDATOR_H
#define GG_SCALE_VALIDATOR_H

#include "gg_types.h"

GGStatus gg_scale_validator_validate_scale(const GGGradingScale *scale);
GGStatus gg_scale_validator_validate_course(const GGGradingScale *scale, const GGCourseEntry *entry);
GGStatus ggvalidate_course_entry(const GGGradingScale *scale, const GGCourseEntry *entry);

#endif
