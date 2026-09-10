#ifndef GG_SCALE_REPOSITORY_H
#define GG_SCALE_REPOSITORY_H

#include "gg_types.h"

typedef struct GGScaleRepository GGScaleRepository;

struct GGScaleRepository {
    void *context;
    GGStatus (*save_scale)(void *context, const GGGradingScale *scale);
    GGStatus (*load_scale)(void *context, GGGradingScale *out_scale);
    void (*destroy)(GGScaleRepository *repo);
};

#endif
