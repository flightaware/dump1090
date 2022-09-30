
/* starch generated code. Do not edit. */

#define STARCH_FLAVOR_GENERIC

#include "starch.h"

#undef STARCH_ALIGNMENT

#define STARCH_ALIGNMENT 1
#define STARCH_ALIGNED(_ptr) (_ptr)
#define STARCH_SYMBOL(_name) starch_ ## _name ## _ ## generic
#define STARCH_IMPL(_function,_impl) starch_ ## _function ## _ ## _impl ## _ ## generic
#define STARCH_IMPL_REQUIRES(_function,_impl,_feature) STARCH_IMPL(_function,_impl)

#include "../impl/count_above_u16.c"
#include "../impl/magnitude_power_uc8.c"
#include "../impl/magnitude_sc16.c"
#include "../impl/magnitude_sc16q11.c"
#include "../impl/magnitude_uc8.c"
#include "../impl/mean_power_u16.c"

