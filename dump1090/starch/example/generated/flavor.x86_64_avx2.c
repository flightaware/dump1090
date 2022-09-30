
/* starch generated code. Do not edit. */

#define STARCH_FLAVOR_X86_64_AVX2

#include "starch.h"

#undef STARCH_ALIGNMENT

#define STARCH_ALIGNMENT 1
#define STARCH_ALIGNED(_ptr) (_ptr)
#define STARCH_SYMBOL(_name) starch_ ## _name ## _ ## x86_64_avx2
#define STARCH_IMPL(_function,_impl) starch_ ## _function ## _ ## _impl ## _ ## x86_64_avx2
#define STARCH_IMPL_REQUIRES(_function,_impl,_feature) STARCH_IMPL(_function,_impl)

#include "../impl/subtract_n.c"


#undef STARCH_ALIGNMENT
#undef STARCH_ALIGNED
#undef STARCH_SYMBOL
#undef STARCH_IMPL
#undef STARCH_IMPL_REQUIRES

#define STARCH_ALIGNMENT STARCH_MIX_ALIGNMENT
#define STARCH_ALIGNED(_ptr) (__builtin_assume_aligned((_ptr), STARCH_MIX_ALIGNMENT))
#define STARCH_SYMBOL(_name) starch_ ## _name ## _aligned_ ## x86_64_avx2
#define STARCH_IMPL(_function,_impl) starch_ ## _function ## _aligned_ ## _impl ## _ ## x86_64_avx2
#define STARCH_IMPL_REQUIRES(_function,_impl,_feature) STARCH_IMPL(_function,_impl)

#include "../impl/subtract_n.c"

