
/* starch generated code. Do not edit. */

#define STARCH_FLAVOR_GENERIC

#include "starch.h"

#define STARCH_SYMBOL(_name) starch_ ## _name ## _ ## generic
#define STARCH_IMPL(_function,_impl) starch_ ## _function ## _ ## _impl ## _ ## generic

#include "../impl/count_above_u16.generic.c"
#include "../impl/demod2400x.generic.c"
#include "../impl/magnitude_power_uc8.generic.c"
#include "../impl/magnitude_sc16.generic.c"
#include "../impl/magnitude_sc16q11.generic.c"
#include "../impl/magnitude_uc8.generic.c"
#include "../impl/mean_power_u16.generic.c"
