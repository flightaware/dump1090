
/* starch generated code. Do not edit. */

#define STARCH_FLAVOR_ARMV8_NEON_SIMD
#define STARCH_FEATURE_NEON

#include "starch.h"

#undef STARCH_ALIGNMENT

#define STARCH_ALIGNMENT 1
#define STARCH_ALIGNED(_ptr) (_ptr)
#define STARCH_SYMBOL(_name) starch_ ## _name ## _ ## armv8_neon_simd
#define STARCH_IMPL(_function,_impl) starch_ ## _function ## _ ## _impl ## _ ## armv8_neon_simd
#define STARCH_IMPL_REQUIRES(_function,_impl,_feature) STARCH_IMPL(_function,_impl)

#include "../impl/count_above_u16.c"
#include "../impl/magnitude_power_uc8.c"
#include "../impl/magnitude_sc16.c"
#include "../impl/magnitude_sc16q11.c"
#include "../impl/magnitude_uc8.c"
#include "../impl/mean_power_u16.c"


#undef STARCH_ALIGNMENT
#undef STARCH_ALIGNED
#undef STARCH_SYMBOL
#undef STARCH_IMPL
#undef STARCH_IMPL_REQUIRES

#define STARCH_ALIGNMENT STARCH_MIX_ALIGNMENT
#define STARCH_ALIGNED(_ptr) (__builtin_assume_aligned((_ptr), STARCH_MIX_ALIGNMENT))
#define STARCH_SYMBOL(_name) starch_ ## _name ## _aligned_ ## armv8_neon_simd
#define STARCH_IMPL(_function,_impl) starch_ ## _function ## _aligned_ ## _impl ## _ ## armv8_neon_simd
#define STARCH_IMPL_REQUIRES(_function,_impl,_feature) STARCH_IMPL(_function,_impl)

#include "../impl/count_above_u16.c"
#include "../impl/magnitude_power_uc8.c"
#include "../impl/magnitude_sc16.c"
#include "../impl/magnitude_sc16q11.c"
#include "../impl/magnitude_uc8.c"
#include "../impl/mean_power_u16.c"

