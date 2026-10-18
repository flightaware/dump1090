
/* starch generated code. Do not edit. */

#define STARCH_FLAVOR_ARMV7A_NEON_VFPV4

#include "starch.h"

#define STARCH_SYMBOL(_name) starch_ ## _name ## _ ## armv7a_neon_vfpv4
#define STARCH_IMPL(_function,_impl) starch_ ## _function ## _ ## _impl ## _ ## armv7a_neon_vfpv4

#include "../impl/count_above_u16.generic.c"
#include "../impl/count_above_u16.neon.c"
#include "../impl/demod2400x.generic.c"
#include "../impl/demod2400x.neon.c"
#include "../impl/magnitude_power_uc8.generic.c"
#include "../impl/magnitude_power_uc8.neon.c"
#include "../impl/magnitude_sc16.generic.c"
#include "../impl/magnitude_sc16.neon.c"
#include "../impl/magnitude_sc16q11.generic.c"
#include "../impl/magnitude_sc16q11.neon.c"
#include "../impl/magnitude_uc8.generic.c"
#include "../impl/magnitude_uc8.neon.c"
#include "../impl/mean_power_u16.generic.c"
#include "../impl/mean_power_u16.neon.c"
