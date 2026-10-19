
/* starch generated code. Do not edit. */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "starch.h"

/* helper for re-sorting registries */
struct starch_regentry_prefix {
    int rank;
};

static int starch_regentry_rank_compare (const void *l, const void *r)
{
    const struct starch_regentry_prefix *left = l, *right = r;
    return left->rank - right->rank;
}

/* dispatcher / registry for count_above_u16 */

starch_count_above_u16_regentry * starch_count_above_u16_select() {
    for (starch_count_above_u16_regentry *entry = starch_count_above_u16_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static void starch_count_above_u16_dispatch ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 ) {
    starch_count_above_u16_regentry *entry = starch_count_above_u16_select();
    if (!entry)
        abort();

    starch_count_above_u16 = entry->callable;
    starch_count_above_u16 ( arg0, arg1, arg2, arg3 );
}

starch_count_above_u16_ptr starch_count_above_u16 = starch_count_above_u16_dispatch;

void starch_count_above_u16_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_count_above_u16_regentry *entry;
    for (entry = starch_count_above_u16_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_count_above_u16_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_count_above_u16_registry, entry - starch_count_above_u16_registry, sizeof(starch_count_above_u16_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_count_above_u16 = starch_count_above_u16_dispatch;
}

starch_count_above_u16_regentry starch_count_above_u16_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "generic_armv8_neon_simd", "armv8_neon_simd", starch_count_above_u16_generic_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "neon_armv8_neon_simd", "armv8_neon_simd", starch_count_above_u16_neon_armv8_neon_simd, cpu_supports_armv8_simd },
    { 2, "generic_generic", "generic", starch_count_above_u16_generic_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_count_above_u16_neon_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "generic_generic", "generic", starch_count_above_u16_generic_generic, NULL },
    { 2, "generic_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_count_above_u16_generic_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "generic_generic", "generic", starch_count_above_u16_generic_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "generic_x86_avx2", "x86_avx2", starch_count_above_u16_generic_x86_avx2, cpu_supports_avx2 },
    { 1, "generic_generic", "generic", starch_count_above_u16_generic_generic, NULL },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for iqdata_conv_sc16 */

starch_iqdata_conv_sc16_regentry * starch_iqdata_conv_sc16_select() {
    for (starch_iqdata_conv_sc16_regentry *entry = starch_iqdata_conv_sc16_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static void starch_iqdata_conv_sc16_dispatch ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, const int32_t * arg3, int32_t * arg4, double * arg5, double * arg6 ) {
    starch_iqdata_conv_sc16_regentry *entry = starch_iqdata_conv_sc16_select();
    if (!entry)
        abort();

    starch_iqdata_conv_sc16 = entry->callable;
    starch_iqdata_conv_sc16 ( arg0, arg1, arg2, arg3, arg4, arg5, arg6 );
}

starch_iqdata_conv_sc16_ptr starch_iqdata_conv_sc16 = starch_iqdata_conv_sc16_dispatch;

void starch_iqdata_conv_sc16_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_iqdata_conv_sc16_regentry *entry;
    for (entry = starch_iqdata_conv_sc16_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_iqdata_conv_sc16_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_iqdata_conv_sc16_registry, entry - starch_iqdata_conv_sc16_registry, sizeof(starch_iqdata_conv_sc16_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_iqdata_conv_sc16 = starch_iqdata_conv_sc16_dispatch;
}

starch_iqdata_conv_sc16_regentry starch_iqdata_conv_sc16_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_armv8_neon_simd", "armv8_neon_simd", starch_iqdata_conv_sc16_neon_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "generic_armv8_neon_simd", "armv8_neon_simd", starch_iqdata_conv_sc16_generic_armv8_neon_simd, cpu_supports_armv8_simd },
    { 2, "generic_generic", "generic", starch_iqdata_conv_sc16_generic_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_iqdata_conv_sc16_neon_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "generic_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_iqdata_conv_sc16_generic_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 2, "generic_generic", "generic", starch_iqdata_conv_sc16_generic_generic, NULL },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "generic_generic", "generic", starch_iqdata_conv_sc16_generic_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "avx2_x86_avx2", "x86_avx2", starch_iqdata_conv_sc16_avx2_x86_avx2, cpu_supports_avx2 },
    { 1, "generic_x86_avx2", "x86_avx2", starch_iqdata_conv_sc16_generic_x86_avx2, cpu_supports_avx2 },
    { 2, "generic_generic", "generic", starch_iqdata_conv_sc16_generic_generic, NULL },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for iqdata_conv_sc16q11 */

starch_iqdata_conv_sc16q11_regentry * starch_iqdata_conv_sc16q11_select() {
    for (starch_iqdata_conv_sc16q11_regentry *entry = starch_iqdata_conv_sc16q11_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static void starch_iqdata_conv_sc16q11_dispatch ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, const int32_t * arg3, int32_t * arg4, double * arg5, double * arg6 ) {
    starch_iqdata_conv_sc16q11_regentry *entry = starch_iqdata_conv_sc16q11_select();
    if (!entry)
        abort();

    starch_iqdata_conv_sc16q11 = entry->callable;
    starch_iqdata_conv_sc16q11 ( arg0, arg1, arg2, arg3, arg4, arg5, arg6 );
}

starch_iqdata_conv_sc16q11_ptr starch_iqdata_conv_sc16q11 = starch_iqdata_conv_sc16q11_dispatch;

void starch_iqdata_conv_sc16q11_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_iqdata_conv_sc16q11_regentry *entry;
    for (entry = starch_iqdata_conv_sc16q11_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_iqdata_conv_sc16q11_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_iqdata_conv_sc16q11_registry, entry - starch_iqdata_conv_sc16q11_registry, sizeof(starch_iqdata_conv_sc16q11_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_iqdata_conv_sc16q11 = starch_iqdata_conv_sc16q11_dispatch;
}

starch_iqdata_conv_sc16q11_regentry starch_iqdata_conv_sc16q11_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_armv8_neon_simd", "armv8_neon_simd", starch_iqdata_conv_sc16q11_neon_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "generic_armv8_neon_simd", "armv8_neon_simd", starch_iqdata_conv_sc16q11_generic_armv8_neon_simd, cpu_supports_armv8_simd },
    { 2, "generic_generic", "generic", starch_iqdata_conv_sc16q11_generic_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_iqdata_conv_sc16q11_neon_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "generic_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_iqdata_conv_sc16q11_generic_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 2, "generic_generic", "generic", starch_iqdata_conv_sc16q11_generic_generic, NULL },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "generic_generic", "generic", starch_iqdata_conv_sc16q11_generic_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "avx2_x86_avx2", "x86_avx2", starch_iqdata_conv_sc16q11_avx2_x86_avx2, cpu_supports_avx2 },
    { 1, "generic_x86_avx2", "x86_avx2", starch_iqdata_conv_sc16q11_generic_x86_avx2, cpu_supports_avx2 },
    { 2, "generic_generic", "generic", starch_iqdata_conv_sc16q11_generic_generic, NULL },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for iqdata_conv_uc8 */

starch_iqdata_conv_uc8_regentry * starch_iqdata_conv_uc8_select() {
    for (starch_iqdata_conv_uc8_regentry *entry = starch_iqdata_conv_uc8_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static void starch_iqdata_conv_uc8_dispatch ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, const int32_t * arg3, int32_t * arg4, double * arg5, double * arg6 ) {
    starch_iqdata_conv_uc8_regentry *entry = starch_iqdata_conv_uc8_select();
    if (!entry)
        abort();

    starch_iqdata_conv_uc8 = entry->callable;
    starch_iqdata_conv_uc8 ( arg0, arg1, arg2, arg3, arg4, arg5, arg6 );
}

starch_iqdata_conv_uc8_ptr starch_iqdata_conv_uc8 = starch_iqdata_conv_uc8_dispatch;

void starch_iqdata_conv_uc8_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_iqdata_conv_uc8_regentry *entry;
    for (entry = starch_iqdata_conv_uc8_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_iqdata_conv_uc8_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_iqdata_conv_uc8_registry, entry - starch_iqdata_conv_uc8_registry, sizeof(starch_iqdata_conv_uc8_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_iqdata_conv_uc8 = starch_iqdata_conv_uc8_dispatch;
}

starch_iqdata_conv_uc8_regentry starch_iqdata_conv_uc8_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_armv8_neon_simd", "armv8_neon_simd", starch_iqdata_conv_uc8_neon_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "generic_armv8_neon_simd", "armv8_neon_simd", starch_iqdata_conv_uc8_generic_armv8_neon_simd, cpu_supports_armv8_simd },
    { 2, "generic_generic", "generic", starch_iqdata_conv_uc8_generic_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_iqdata_conv_uc8_neon_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "generic_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_iqdata_conv_uc8_generic_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 2, "generic_generic", "generic", starch_iqdata_conv_uc8_generic_generic, NULL },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "generic_generic", "generic", starch_iqdata_conv_uc8_generic_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "avx2_x86_avx2", "x86_avx2", starch_iqdata_conv_uc8_avx2_x86_avx2, cpu_supports_avx2 },
    { 1, "generic_x86_avx2", "x86_avx2", starch_iqdata_conv_uc8_generic_x86_avx2, cpu_supports_avx2 },
    { 2, "generic_generic", "generic", starch_iqdata_conv_uc8_generic_generic, NULL },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for locate_preamble */

starch_locate_preamble_regentry * starch_locate_preamble_select() {
    for (starch_locate_preamble_regentry *entry = starch_locate_preamble_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static int starch_locate_preamble_dispatch ( const uint16_t * arg0, int arg1, unsigned arg2, uint8_t * arg3 ) {
    starch_locate_preamble_regentry *entry = starch_locate_preamble_select();
    if (!entry)
        abort();

    starch_locate_preamble = entry->callable;
    return starch_locate_preamble ( arg0, arg1, arg2, arg3 );
}

starch_locate_preamble_ptr starch_locate_preamble = starch_locate_preamble_dispatch;

void starch_locate_preamble_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_locate_preamble_regentry *entry;
    for (entry = starch_locate_preamble_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_locate_preamble_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_locate_preamble_registry, entry - starch_locate_preamble_registry, sizeof(starch_locate_preamble_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_locate_preamble = starch_locate_preamble_dispatch;
}

starch_locate_preamble_regentry starch_locate_preamble_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_armv8_neon_simd", "armv8_neon_simd", starch_locate_preamble_neon_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "generic_armv8_neon_simd", "armv8_neon_simd", starch_locate_preamble_generic_armv8_neon_simd, cpu_supports_armv8_simd },
    { 2, "generic_generic", "generic", starch_locate_preamble_generic_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_locate_preamble_neon_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "generic_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_locate_preamble_generic_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 2, "generic_generic", "generic", starch_locate_preamble_generic_generic, NULL },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "generic_generic", "generic", starch_locate_preamble_generic_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "generic_x86_avx2", "x86_avx2", starch_locate_preamble_generic_x86_avx2, cpu_supports_avx2 },
    { 1, "generic_generic", "generic", starch_locate_preamble_generic_generic, NULL },
    { 2, "avx2_x86_avx2", "x86_avx2", starch_locate_preamble_avx2_x86_avx2, cpu_supports_avx2 },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for magnitude_power_uc8 */

starch_magnitude_power_uc8_regentry * starch_magnitude_power_uc8_select() {
    for (starch_magnitude_power_uc8_regentry *entry = starch_magnitude_power_uc8_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static void starch_magnitude_power_uc8_dispatch ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 ) {
    starch_magnitude_power_uc8_regentry *entry = starch_magnitude_power_uc8_select();
    if (!entry)
        abort();

    starch_magnitude_power_uc8 = entry->callable;
    starch_magnitude_power_uc8 ( arg0, arg1, arg2, arg3, arg4 );
}

starch_magnitude_power_uc8_ptr starch_magnitude_power_uc8 = starch_magnitude_power_uc8_dispatch;

void starch_magnitude_power_uc8_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_magnitude_power_uc8_regentry *entry;
    for (entry = starch_magnitude_power_uc8_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_magnitude_power_uc8_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_magnitude_power_uc8_registry, entry - starch_magnitude_power_uc8_registry, sizeof(starch_magnitude_power_uc8_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_magnitude_power_uc8 = starch_magnitude_power_uc8_dispatch;
}

starch_magnitude_power_uc8_regentry starch_magnitude_power_uc8_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_vrsqrte_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_power_uc8_neon_vrsqrte_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "lookup_unroll_4_generic", "generic", starch_magnitude_power_uc8_lookup_unroll_4_generic, NULL },
    { 2, "twopass_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_power_uc8_twopass_armv8_neon_simd, cpu_supports_armv8_simd },
    { 3, "lookup_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_power_uc8_lookup_armv8_neon_simd, cpu_supports_armv8_simd },
    { 4, "lookup_unroll_4_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_power_uc8_lookup_unroll_4_armv8_neon_simd, cpu_supports_armv8_simd },
    { 5, "twopass_generic", "generic", starch_magnitude_power_uc8_twopass_generic, NULL },
    { 6, "lookup_generic", "generic", starch_magnitude_power_uc8_lookup_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_vrsqrte_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_power_uc8_neon_vrsqrte_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "lookup_unroll_4_generic", "generic", starch_magnitude_power_uc8_lookup_unroll_4_generic, NULL },
    { 2, "twopass_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_power_uc8_twopass_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 3, "lookup_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_power_uc8_lookup_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 4, "lookup_unroll_4_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_power_uc8_lookup_unroll_4_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 5, "twopass_generic", "generic", starch_magnitude_power_uc8_twopass_generic, NULL },
    { 6, "lookup_generic", "generic", starch_magnitude_power_uc8_lookup_generic, NULL },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "twopass_generic", "generic", starch_magnitude_power_uc8_twopass_generic, NULL },
    { 1, "lookup_generic", "generic", starch_magnitude_power_uc8_lookup_generic, NULL },
    { 2, "lookup_unroll_4_generic", "generic", starch_magnitude_power_uc8_lookup_unroll_4_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "twopass_x86_avx2", "x86_avx2", starch_magnitude_power_uc8_twopass_x86_avx2, cpu_supports_avx2 },
    { 1, "twopass_generic", "generic", starch_magnitude_power_uc8_twopass_generic, NULL },
    { 2, "lookup_x86_avx2", "x86_avx2", starch_magnitude_power_uc8_lookup_x86_avx2, cpu_supports_avx2 },
    { 3, "lookup_unroll_4_x86_avx2", "x86_avx2", starch_magnitude_power_uc8_lookup_unroll_4_x86_avx2, cpu_supports_avx2 },
    { 4, "lookup_generic", "generic", starch_magnitude_power_uc8_lookup_generic, NULL },
    { 5, "lookup_unroll_4_generic", "generic", starch_magnitude_power_uc8_lookup_unroll_4_generic, NULL },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for magnitude_sc16 */

starch_magnitude_sc16_regentry * starch_magnitude_sc16_select() {
    for (starch_magnitude_sc16_regentry *entry = starch_magnitude_sc16_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static void starch_magnitude_sc16_dispatch ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 ) {
    starch_magnitude_sc16_regentry *entry = starch_magnitude_sc16_select();
    if (!entry)
        abort();

    starch_magnitude_sc16 = entry->callable;
    starch_magnitude_sc16 ( arg0, arg1, arg2 );
}

starch_magnitude_sc16_ptr starch_magnitude_sc16 = starch_magnitude_sc16_dispatch;

void starch_magnitude_sc16_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_magnitude_sc16_regentry *entry;
    for (entry = starch_magnitude_sc16_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_magnitude_sc16_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_magnitude_sc16_registry, entry - starch_magnitude_sc16_registry, sizeof(starch_magnitude_sc16_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_magnitude_sc16 = starch_magnitude_sc16_dispatch;
}

starch_magnitude_sc16_regentry starch_magnitude_sc16_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_vrsqrte_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_sc16_neon_vrsqrte_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "exact_float_generic", "generic", starch_magnitude_sc16_exact_float_generic, NULL },
    { 2, "exact_u32_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_sc16_exact_u32_armv8_neon_simd, cpu_supports_armv8_simd },
    { 3, "exact_float_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_sc16_exact_float_armv8_neon_simd, cpu_supports_armv8_simd },
    { 4, "exact_u32_generic", "generic", starch_magnitude_sc16_exact_u32_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_vrsqrte_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_sc16_neon_vrsqrte_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "exact_float_generic", "generic", starch_magnitude_sc16_exact_float_generic, NULL },
    { 2, "exact_u32_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_sc16_exact_u32_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 3, "exact_float_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_sc16_exact_float_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 4, "exact_u32_generic", "generic", starch_magnitude_sc16_exact_u32_generic, NULL },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "exact_float_generic", "generic", starch_magnitude_sc16_exact_float_generic, NULL },
    { 1, "exact_u32_generic", "generic", starch_magnitude_sc16_exact_u32_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "exact_float_x86_avx2", "x86_avx2", starch_magnitude_sc16_exact_float_x86_avx2, cpu_supports_avx2 },
    { 1, "exact_float_generic", "generic", starch_magnitude_sc16_exact_float_generic, NULL },
    { 2, "exact_u32_x86_avx2", "x86_avx2", starch_magnitude_sc16_exact_u32_x86_avx2, cpu_supports_avx2 },
    { 3, "exact_u32_generic", "generic", starch_magnitude_sc16_exact_u32_generic, NULL },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for magnitude_sc16q11 */

starch_magnitude_sc16q11_regentry * starch_magnitude_sc16q11_select() {
    for (starch_magnitude_sc16q11_regentry *entry = starch_magnitude_sc16q11_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static void starch_magnitude_sc16q11_dispatch ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 ) {
    starch_magnitude_sc16q11_regentry *entry = starch_magnitude_sc16q11_select();
    if (!entry)
        abort();

    starch_magnitude_sc16q11 = entry->callable;
    starch_magnitude_sc16q11 ( arg0, arg1, arg2 );
}

starch_magnitude_sc16q11_ptr starch_magnitude_sc16q11 = starch_magnitude_sc16q11_dispatch;

void starch_magnitude_sc16q11_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_magnitude_sc16q11_regentry *entry;
    for (entry = starch_magnitude_sc16q11_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_magnitude_sc16q11_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_magnitude_sc16q11_registry, entry - starch_magnitude_sc16q11_registry, sizeof(starch_magnitude_sc16q11_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_magnitude_sc16q11 = starch_magnitude_sc16q11_dispatch;
}

starch_magnitude_sc16q11_regentry starch_magnitude_sc16q11_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_vrsqrte_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_sc16q11_neon_vrsqrte_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "exact_float_generic", "generic", starch_magnitude_sc16q11_exact_float_generic, NULL },
    { 2, "exact_u32_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_sc16q11_exact_u32_armv8_neon_simd, cpu_supports_armv8_simd },
    { 3, "exact_float_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_sc16q11_exact_float_armv8_neon_simd, cpu_supports_armv8_simd },
    { 4, "11bit_table_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_sc16q11_11bit_table_armv8_neon_simd, cpu_supports_armv8_simd },
    { 5, "12bit_table_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_sc16q11_12bit_table_armv8_neon_simd, cpu_supports_armv8_simd },
    { 6, "exact_u32_generic", "generic", starch_magnitude_sc16q11_exact_u32_generic, NULL },
    { 7, "11bit_table_generic", "generic", starch_magnitude_sc16q11_11bit_table_generic, NULL },
    { 8, "12bit_table_generic", "generic", starch_magnitude_sc16q11_12bit_table_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_vrsqrte_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_sc16q11_neon_vrsqrte_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "exact_float_generic", "generic", starch_magnitude_sc16q11_exact_float_generic, NULL },
    { 2, "exact_u32_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_sc16q11_exact_u32_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 3, "exact_float_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_sc16q11_exact_float_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 4, "11bit_table_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_sc16q11_11bit_table_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 5, "12bit_table_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_sc16q11_12bit_table_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 6, "exact_u32_generic", "generic", starch_magnitude_sc16q11_exact_u32_generic, NULL },
    { 7, "11bit_table_generic", "generic", starch_magnitude_sc16q11_11bit_table_generic, NULL },
    { 8, "12bit_table_generic", "generic", starch_magnitude_sc16q11_12bit_table_generic, NULL },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "exact_float_generic", "generic", starch_magnitude_sc16q11_exact_float_generic, NULL },
    { 1, "exact_u32_generic", "generic", starch_magnitude_sc16q11_exact_u32_generic, NULL },
    { 2, "11bit_table_generic", "generic", starch_magnitude_sc16q11_11bit_table_generic, NULL },
    { 3, "12bit_table_generic", "generic", starch_magnitude_sc16q11_12bit_table_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "exact_float_x86_avx2", "x86_avx2", starch_magnitude_sc16q11_exact_float_x86_avx2, cpu_supports_avx2 },
    { 1, "exact_float_generic", "generic", starch_magnitude_sc16q11_exact_float_generic, NULL },
    { 2, "exact_u32_x86_avx2", "x86_avx2", starch_magnitude_sc16q11_exact_u32_x86_avx2, cpu_supports_avx2 },
    { 3, "11bit_table_x86_avx2", "x86_avx2", starch_magnitude_sc16q11_11bit_table_x86_avx2, cpu_supports_avx2 },
    { 4, "12bit_table_x86_avx2", "x86_avx2", starch_magnitude_sc16q11_12bit_table_x86_avx2, cpu_supports_avx2 },
    { 5, "exact_u32_generic", "generic", starch_magnitude_sc16q11_exact_u32_generic, NULL },
    { 6, "11bit_table_generic", "generic", starch_magnitude_sc16q11_11bit_table_generic, NULL },
    { 7, "12bit_table_generic", "generic", starch_magnitude_sc16q11_12bit_table_generic, NULL },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for magnitude_uc8 */

starch_magnitude_uc8_regentry * starch_magnitude_uc8_select() {
    for (starch_magnitude_uc8_regentry *entry = starch_magnitude_uc8_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static void starch_magnitude_uc8_dispatch ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 ) {
    starch_magnitude_uc8_regentry *entry = starch_magnitude_uc8_select();
    if (!entry)
        abort();

    starch_magnitude_uc8 = entry->callable;
    starch_magnitude_uc8 ( arg0, arg1, arg2 );
}

starch_magnitude_uc8_ptr starch_magnitude_uc8 = starch_magnitude_uc8_dispatch;

void starch_magnitude_uc8_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_magnitude_uc8_regentry *entry;
    for (entry = starch_magnitude_uc8_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_magnitude_uc8_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_magnitude_uc8_registry, entry - starch_magnitude_uc8_registry, sizeof(starch_magnitude_uc8_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_magnitude_uc8 = starch_magnitude_uc8_dispatch;
}

starch_magnitude_uc8_regentry starch_magnitude_uc8_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_vrsqrte_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_uc8_neon_vrsqrte_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "lookup_unroll_4_generic", "generic", starch_magnitude_uc8_lookup_unroll_4_generic, NULL },
    { 2, "lookup_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_uc8_lookup_armv8_neon_simd, cpu_supports_armv8_simd },
    { 3, "lookup_unroll_4_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_uc8_lookup_unroll_4_armv8_neon_simd, cpu_supports_armv8_simd },
    { 4, "exact_armv8_neon_simd", "armv8_neon_simd", starch_magnitude_uc8_exact_armv8_neon_simd, cpu_supports_armv8_simd },
    { 5, "lookup_generic", "generic", starch_magnitude_uc8_lookup_generic, NULL },
    { 6, "exact_generic", "generic", starch_magnitude_uc8_exact_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_vrsqrte_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_uc8_neon_vrsqrte_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "lookup_unroll_4_generic", "generic", starch_magnitude_uc8_lookup_unroll_4_generic, NULL },
    { 2, "lookup_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_uc8_lookup_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 3, "lookup_unroll_4_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_uc8_lookup_unroll_4_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 4, "exact_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_magnitude_uc8_exact_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 5, "lookup_generic", "generic", starch_magnitude_uc8_lookup_generic, NULL },
    { 6, "exact_generic", "generic", starch_magnitude_uc8_exact_generic, NULL },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "lookup_unroll_4_generic", "generic", starch_magnitude_uc8_lookup_unroll_4_generic, NULL },
    { 1, "lookup_generic", "generic", starch_magnitude_uc8_lookup_generic, NULL },
    { 2, "exact_generic", "generic", starch_magnitude_uc8_exact_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "lookup_unroll_4_x86_avx2", "x86_avx2", starch_magnitude_uc8_lookup_unroll_4_x86_avx2, cpu_supports_avx2 },
    { 1, "lookup_unroll_4_generic", "generic", starch_magnitude_uc8_lookup_unroll_4_generic, NULL },
    { 2, "lookup_x86_avx2", "x86_avx2", starch_magnitude_uc8_lookup_x86_avx2, cpu_supports_avx2 },
    { 3, "exact_x86_avx2", "x86_avx2", starch_magnitude_uc8_exact_x86_avx2, cpu_supports_avx2 },
    { 4, "lookup_generic", "generic", starch_magnitude_uc8_lookup_generic, NULL },
    { 5, "exact_generic", "generic", starch_magnitude_uc8_exact_generic, NULL },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for mean_power_u16 */

starch_mean_power_u16_regentry * starch_mean_power_u16_select() {
    for (starch_mean_power_u16_regentry *entry = starch_mean_power_u16_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static void starch_mean_power_u16_dispatch ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 ) {
    starch_mean_power_u16_regentry *entry = starch_mean_power_u16_select();
    if (!entry)
        abort();

    starch_mean_power_u16 = entry->callable;
    starch_mean_power_u16 ( arg0, arg1, arg2, arg3 );
}

starch_mean_power_u16_ptr starch_mean_power_u16 = starch_mean_power_u16_dispatch;

void starch_mean_power_u16_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_mean_power_u16_regentry *entry;
    for (entry = starch_mean_power_u16_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_mean_power_u16_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_mean_power_u16_registry, entry - starch_mean_power_u16_registry, sizeof(starch_mean_power_u16_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_mean_power_u16 = starch_mean_power_u16_dispatch;
}

starch_mean_power_u16_regentry starch_mean_power_u16_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "u32_armv8_neon_simd", "armv8_neon_simd", starch_mean_power_u16_u32_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "u64_generic", "generic", starch_mean_power_u16_u64_generic, NULL },
    { 2, "float_armv8_neon_simd", "armv8_neon_simd", starch_mean_power_u16_float_armv8_neon_simd, cpu_supports_armv8_simd },
    { 3, "u64_armv8_neon_simd", "armv8_neon_simd", starch_mean_power_u16_u64_armv8_neon_simd, cpu_supports_armv8_simd },
    { 4, "neon_float_armv8_neon_simd", "armv8_neon_simd", starch_mean_power_u16_neon_float_armv8_neon_simd, cpu_supports_armv8_simd },
    { 5, "float_generic", "generic", starch_mean_power_u16_float_generic, NULL },
    { 6, "u32_generic", "generic", starch_mean_power_u16_u32_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "u32_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_mean_power_u16_u32_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "u64_generic", "generic", starch_mean_power_u16_u64_generic, NULL },
    { 2, "float_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_mean_power_u16_float_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 3, "u64_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_mean_power_u16_u64_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 4, "neon_float_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_mean_power_u16_neon_float_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 5, "float_generic", "generic", starch_mean_power_u16_float_generic, NULL },
    { 6, "u32_generic", "generic", starch_mean_power_u16_u32_generic, NULL },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "u32_generic", "generic", starch_mean_power_u16_u32_generic, NULL },
    { 1, "float_generic", "generic", starch_mean_power_u16_float_generic, NULL },
    { 2, "u64_generic", "generic", starch_mean_power_u16_u64_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "u32_x86_avx2", "x86_avx2", starch_mean_power_u16_u32_x86_avx2, cpu_supports_avx2 },
    { 1, "u32_generic", "generic", starch_mean_power_u16_u32_generic, NULL },
    { 2, "float_x86_avx2", "x86_avx2", starch_mean_power_u16_float_x86_avx2, cpu_supports_avx2 },
    { 3, "u64_x86_avx2", "x86_avx2", starch_mean_power_u16_u64_x86_avx2, cpu_supports_avx2 },
    { 4, "float_generic", "generic", starch_mean_power_u16_float_generic, NULL },
    { 5, "u64_generic", "generic", starch_mean_power_u16_u64_generic, NULL },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};

/* dispatcher / registry for upsample12 */

starch_upsample12_regentry * starch_upsample12_select() {
    for (starch_upsample12_regentry *entry = starch_upsample12_registry;
         entry->name;
         ++entry)
    {
        if (entry->flavor_supported && !(entry->flavor_supported()))
            continue;
        return entry;
    }
    return NULL;
}

static int starch_upsample12_dispatch ( const int16_t * arg0, int arg1, int arg2, uint16_t * arg3 ) {
    starch_upsample12_regentry *entry = starch_upsample12_select();
    if (!entry)
        abort();

    starch_upsample12 = entry->callable;
    return starch_upsample12 ( arg0, arg1, arg2, arg3 );
}

starch_upsample12_ptr starch_upsample12 = starch_upsample12_dispatch;

void starch_upsample12_set_wisdom (const char * const * received_wisdom)
{
    /* re-rank the registry based on received wisdom */
    starch_upsample12_regentry *entry;
    for (entry = starch_upsample12_registry; entry->name; ++entry) {
        const char * const *search;
        for (search = received_wisdom; *search; ++search) {
            if (!strcmp(*search, entry->name)) {
                break;
            }
        }
        if (*search) {
            /* matches an entry in the wisdom list, order by position in the list */
            entry->rank = search - received_wisdom;
        } else {
            /* no match, rank after all possible matches, retaining existing order */
            entry->rank = (search - received_wisdom) + (entry - starch_upsample12_registry);
        }
    }

    /* re-sort based on the new ranking */
    qsort(starch_upsample12_registry, entry - starch_upsample12_registry, sizeof(starch_upsample12_regentry), starch_regentry_rank_compare);

    /* reset the implementation pointer so the next call will re-select */
    starch_upsample12 = starch_upsample12_dispatch;
}

starch_upsample12_regentry starch_upsample12_registry[] = {
  
#ifdef STARCH_MIX_AARCH64
    { 0, "neon_armv8_neon_simd", "armv8_neon_simd", starch_upsample12_neon_armv8_neon_simd, cpu_supports_armv8_simd },
    { 1, "generic_armv8_neon_simd", "armv8_neon_simd", starch_upsample12_generic_armv8_neon_simd, cpu_supports_armv8_simd },
    { 2, "generic_generic", "generic", starch_upsample12_generic_generic, NULL },
#endif /* STARCH_MIX_AARCH64 */
  
#ifdef STARCH_MIX_ARM
    { 0, "neon_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_upsample12_neon_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 1, "generic_armv7a_neon_vfpv4", "armv7a_neon_vfpv4", starch_upsample12_generic_armv7a_neon_vfpv4, cpu_supports_armv7_neon_vfpv4 },
    { 2, "generic_generic", "generic", starch_upsample12_generic_generic, NULL },
#endif /* STARCH_MIX_ARM */
  
#ifdef STARCH_MIX_GENERIC
    { 0, "generic_generic", "generic", starch_upsample12_generic_generic, NULL },
#endif /* STARCH_MIX_GENERIC */
  
#ifdef STARCH_MIX_X86
    { 0, "avx2_x86_avx2", "x86_avx2", starch_upsample12_avx2_x86_avx2, cpu_supports_avx2 },
    { 1, "generic_generic", "generic", starch_upsample12_generic_generic, NULL },
    { 2, "generic_x86_avx2", "x86_avx2", starch_upsample12_generic_x86_avx2, cpu_supports_avx2 },
#endif /* STARCH_MIX_X86 */
    { 0, NULL, NULL, NULL, NULL }
};


int starch_read_wisdom (const char * path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    /* reset all ranks to identify entries not listed in the wisdom file; we'll assign ranks at the end to produce a stable sort */
    int rank_count_above_u16 = 0;
    for (starch_count_above_u16_regentry *entry = starch_count_above_u16_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_iqdata_conv_sc16 = 0;
    for (starch_iqdata_conv_sc16_regentry *entry = starch_iqdata_conv_sc16_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_iqdata_conv_sc16q11 = 0;
    for (starch_iqdata_conv_sc16q11_regentry *entry = starch_iqdata_conv_sc16q11_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_iqdata_conv_uc8 = 0;
    for (starch_iqdata_conv_uc8_regentry *entry = starch_iqdata_conv_uc8_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_locate_preamble = 0;
    for (starch_locate_preamble_regentry *entry = starch_locate_preamble_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_magnitude_power_uc8 = 0;
    for (starch_magnitude_power_uc8_regentry *entry = starch_magnitude_power_uc8_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_magnitude_sc16 = 0;
    for (starch_magnitude_sc16_regentry *entry = starch_magnitude_sc16_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_magnitude_sc16q11 = 0;
    for (starch_magnitude_sc16q11_regentry *entry = starch_magnitude_sc16q11_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_magnitude_uc8 = 0;
    for (starch_magnitude_uc8_regentry *entry = starch_magnitude_uc8_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_mean_power_u16 = 0;
    for (starch_mean_power_u16_regentry *entry = starch_mean_power_u16_registry; entry->name; ++entry) {
        entry->rank = 0;
    }
    int rank_upsample12 = 0;
    for (starch_upsample12_regentry *entry = starch_upsample12_registry; entry->name; ++entry) {
        entry->rank = 0;
    }

    char linebuf[512];
    while (fgets(linebuf, sizeof(linebuf), fp)) {
        /* split name and impl on whitespace, handle comments etc */
        char *name = linebuf;
        while (*name && isspace(*name))
            ++name;

        if (!*name || *name == '#')
            continue;

        char *end = name;
        while (*end && !isspace(*end))
            ++end;

        if (!*end)
            continue;
        *end = 0;

        char *impl = end + 1;
        while (*impl && isspace(*impl))
            ++impl;

        if (!*impl)
           continue;

        end = impl;
        while (*end && !isspace(*end))
            ++end;

        *end = 0;

        /* try to find a matching registry entry */
        if (!strcmp(name, "count_above_u16")) {
            for (starch_count_above_u16_regentry *entry = starch_count_above_u16_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_count_above_u16;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "iqdata_conv_sc16")) {
            for (starch_iqdata_conv_sc16_regentry *entry = starch_iqdata_conv_sc16_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_iqdata_conv_sc16;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "iqdata_conv_sc16q11")) {
            for (starch_iqdata_conv_sc16q11_regentry *entry = starch_iqdata_conv_sc16q11_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_iqdata_conv_sc16q11;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "iqdata_conv_uc8")) {
            for (starch_iqdata_conv_uc8_regentry *entry = starch_iqdata_conv_uc8_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_iqdata_conv_uc8;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "locate_preamble")) {
            for (starch_locate_preamble_regentry *entry = starch_locate_preamble_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_locate_preamble;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "magnitude_power_uc8")) {
            for (starch_magnitude_power_uc8_regentry *entry = starch_magnitude_power_uc8_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_magnitude_power_uc8;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "magnitude_sc16")) {
            for (starch_magnitude_sc16_regentry *entry = starch_magnitude_sc16_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_magnitude_sc16;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "magnitude_sc16q11")) {
            for (starch_magnitude_sc16q11_regentry *entry = starch_magnitude_sc16q11_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_magnitude_sc16q11;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "magnitude_uc8")) {
            for (starch_magnitude_uc8_regentry *entry = starch_magnitude_uc8_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_magnitude_uc8;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "mean_power_u16")) {
            for (starch_mean_power_u16_regentry *entry = starch_mean_power_u16_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_mean_power_u16;
                    break;
                }
            }
            continue;
        }
        if (!strcmp(name, "upsample12")) {
            for (starch_upsample12_regentry *entry = starch_upsample12_registry; entry->name; ++entry) {
                if (!strcmp(impl, entry->name)) {
                    entry->rank = ++rank_upsample12;
                    break;
                }
            }
            continue;
        }
    }

    if (ferror(fp)) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    /* assign ranks to unmatched items to (stable) sort them last; re-sort everything */
    {
        starch_count_above_u16_regentry *entry;
        for (entry = starch_count_above_u16_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_count_above_u16;
        }
        qsort(starch_count_above_u16_registry, entry - starch_count_above_u16_registry, sizeof(starch_count_above_u16_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_count_above_u16 = starch_count_above_u16_dispatch;
    }
    {
        starch_iqdata_conv_sc16_regentry *entry;
        for (entry = starch_iqdata_conv_sc16_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_iqdata_conv_sc16;
        }
        qsort(starch_iqdata_conv_sc16_registry, entry - starch_iqdata_conv_sc16_registry, sizeof(starch_iqdata_conv_sc16_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_iqdata_conv_sc16 = starch_iqdata_conv_sc16_dispatch;
    }
    {
        starch_iqdata_conv_sc16q11_regentry *entry;
        for (entry = starch_iqdata_conv_sc16q11_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_iqdata_conv_sc16q11;
        }
        qsort(starch_iqdata_conv_sc16q11_registry, entry - starch_iqdata_conv_sc16q11_registry, sizeof(starch_iqdata_conv_sc16q11_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_iqdata_conv_sc16q11 = starch_iqdata_conv_sc16q11_dispatch;
    }
    {
        starch_iqdata_conv_uc8_regentry *entry;
        for (entry = starch_iqdata_conv_uc8_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_iqdata_conv_uc8;
        }
        qsort(starch_iqdata_conv_uc8_registry, entry - starch_iqdata_conv_uc8_registry, sizeof(starch_iqdata_conv_uc8_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_iqdata_conv_uc8 = starch_iqdata_conv_uc8_dispatch;
    }
    {
        starch_locate_preamble_regentry *entry;
        for (entry = starch_locate_preamble_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_locate_preamble;
        }
        qsort(starch_locate_preamble_registry, entry - starch_locate_preamble_registry, sizeof(starch_locate_preamble_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_locate_preamble = starch_locate_preamble_dispatch;
    }
    {
        starch_magnitude_power_uc8_regentry *entry;
        for (entry = starch_magnitude_power_uc8_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_magnitude_power_uc8;
        }
        qsort(starch_magnitude_power_uc8_registry, entry - starch_magnitude_power_uc8_registry, sizeof(starch_magnitude_power_uc8_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_magnitude_power_uc8 = starch_magnitude_power_uc8_dispatch;
    }
    {
        starch_magnitude_sc16_regentry *entry;
        for (entry = starch_magnitude_sc16_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_magnitude_sc16;
        }
        qsort(starch_magnitude_sc16_registry, entry - starch_magnitude_sc16_registry, sizeof(starch_magnitude_sc16_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_magnitude_sc16 = starch_magnitude_sc16_dispatch;
    }
    {
        starch_magnitude_sc16q11_regentry *entry;
        for (entry = starch_magnitude_sc16q11_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_magnitude_sc16q11;
        }
        qsort(starch_magnitude_sc16q11_registry, entry - starch_magnitude_sc16q11_registry, sizeof(starch_magnitude_sc16q11_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_magnitude_sc16q11 = starch_magnitude_sc16q11_dispatch;
    }
    {
        starch_magnitude_uc8_regentry *entry;
        for (entry = starch_magnitude_uc8_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_magnitude_uc8;
        }
        qsort(starch_magnitude_uc8_registry, entry - starch_magnitude_uc8_registry, sizeof(starch_magnitude_uc8_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_magnitude_uc8 = starch_magnitude_uc8_dispatch;
    }
    {
        starch_mean_power_u16_regentry *entry;
        for (entry = starch_mean_power_u16_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_mean_power_u16;
        }
        qsort(starch_mean_power_u16_registry, entry - starch_mean_power_u16_registry, sizeof(starch_mean_power_u16_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_mean_power_u16 = starch_mean_power_u16_dispatch;
    }
    {
        starch_upsample12_regentry *entry;
        for (entry = starch_upsample12_registry; entry->name; ++entry) {
            if (!entry->rank)
                entry->rank = ++rank_upsample12;
        }
        qsort(starch_upsample12_registry, entry - starch_upsample12_registry, sizeof(starch_upsample12_regentry), starch_regentry_rank_compare);

        /* reset the implementation pointer so the next call will re-select */
        starch_upsample12 = starch_upsample12_dispatch;
    }

    return 0;
}
