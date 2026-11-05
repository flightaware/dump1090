
/* starch generated code. Do not edit. */

#define STARCH_FLAVOR_X86_AVX2

#include "starch.h"

#define STARCH_SYMBOL(_name) starch_ ## _name ## _ ## x86_avx2
#define STARCH_IMPL(_function,_impl) starch_ ## _function ## _ ## _impl ## _ ## x86_avx2

#include "../impl/count_above_u16.generic.c"
#include "../impl/demod2400x.avx2.c"
#include "../impl/demod2400x.generic.c"
#include "../impl/magnitude_power_uc8.generic.c"
#include "../impl/magnitude_sc16.generic.c"
#include "../impl/magnitude_sc16q11.generic.c"
#include "../impl/magnitude_uc8.generic.c"
#include "../impl/mean_power_u16.generic.c"
