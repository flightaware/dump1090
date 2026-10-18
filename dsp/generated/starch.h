
/* starch generated code. Do not edit. */

#include "dsp-types.h"
#include "cpu.h"

/* mixes */

/* ARM, 64-bit */
#ifdef STARCH_MIX_AARCH64
#ifdef STARCH_MIX
#  error Multiple starch mixes enabled, only one should be enabled
#endif
#define STARCH_MIX_NAME "aarch64"
#define STARCH_FLAVOR_ARMV8_NEON_SIMD
#define STARCH_FLAVOR_GENERIC
#endif /* STARCH_MIX_AARCH64 */

/* ARM, 32-bit */
#ifdef STARCH_MIX_ARM
#ifdef STARCH_MIX
#  error Multiple starch mixes enabled, only one should be enabled
#endif
#define STARCH_MIX_NAME "arm"
#define STARCH_FLAVOR_ARMV7A_NEON_VFPV4
#define STARCH_FLAVOR_GENERIC
#endif /* STARCH_MIX_ARM */

/* Generic build, compiler defaults only */
#ifdef STARCH_MIX_GENERIC
#ifdef STARCH_MIX
#  error Multiple starch mixes enabled, only one should be enabled
#endif
#define STARCH_MIX_NAME "generic"
#define STARCH_FLAVOR_GENERIC
#endif /* STARCH_MIX_GENERIC */

/* x86-64 */
#ifdef STARCH_MIX_X86
#ifdef STARCH_MIX
#  error Multiple starch mixes enabled, only one should be enabled
#endif
#define STARCH_MIX_NAME "x86"
#define STARCH_FLAVOR_X86_AVX2
#define STARCH_FLAVOR_GENERIC
#endif /* STARCH_MIX_X86 */


/* entry points and registries */

typedef void (* starch_magnitude_uc8_ptr) ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
extern starch_magnitude_uc8_ptr starch_magnitude_uc8;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_magnitude_uc8_ptr callable;
    int (*flavor_supported)();
} starch_magnitude_uc8_regentry;

extern starch_magnitude_uc8_regentry starch_magnitude_uc8_registry[];
starch_magnitude_uc8_regentry * starch_magnitude_uc8_select();
void starch_magnitude_uc8_set_wisdom( const char * const * received_wisdom );

typedef void (* starch_magnitude_power_uc8_ptr) ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
extern starch_magnitude_power_uc8_ptr starch_magnitude_power_uc8;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_magnitude_power_uc8_ptr callable;
    int (*flavor_supported)();
} starch_magnitude_power_uc8_regentry;

extern starch_magnitude_power_uc8_regentry starch_magnitude_power_uc8_registry[];
starch_magnitude_power_uc8_regentry * starch_magnitude_power_uc8_select();
void starch_magnitude_power_uc8_set_wisdom( const char * const * received_wisdom );

typedef void (* starch_magnitude_sc16_ptr) ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
extern starch_magnitude_sc16_ptr starch_magnitude_sc16;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_magnitude_sc16_ptr callable;
    int (*flavor_supported)();
} starch_magnitude_sc16_regentry;

extern starch_magnitude_sc16_regentry starch_magnitude_sc16_registry[];
starch_magnitude_sc16_regentry * starch_magnitude_sc16_select();
void starch_magnitude_sc16_set_wisdom( const char * const * received_wisdom );

typedef void (* starch_magnitude_sc16q11_ptr) ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
extern starch_magnitude_sc16q11_ptr starch_magnitude_sc16q11;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_magnitude_sc16q11_ptr callable;
    int (*flavor_supported)();
} starch_magnitude_sc16q11_regentry;

extern starch_magnitude_sc16q11_regentry starch_magnitude_sc16q11_registry[];
starch_magnitude_sc16q11_regentry * starch_magnitude_sc16q11_select();
void starch_magnitude_sc16q11_set_wisdom( const char * const * received_wisdom );

typedef void (* starch_mean_power_u16_ptr) ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
extern starch_mean_power_u16_ptr starch_mean_power_u16;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_mean_power_u16_ptr callable;
    int (*flavor_supported)();
} starch_mean_power_u16_regentry;

extern starch_mean_power_u16_regentry starch_mean_power_u16_registry[];
starch_mean_power_u16_regentry * starch_mean_power_u16_select();
void starch_mean_power_u16_set_wisdom( const char * const * received_wisdom );

typedef void (* starch_count_above_u16_ptr) ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 );
extern starch_count_above_u16_ptr starch_count_above_u16;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_count_above_u16_ptr callable;
    int (*flavor_supported)();
} starch_count_above_u16_regentry;

extern starch_count_above_u16_regentry starch_count_above_u16_registry[];
starch_count_above_u16_regentry * starch_count_above_u16_select();
void starch_count_above_u16_set_wisdom( const char * const * received_wisdom );

typedef void (* starch_iqdata_conv_uc8_ptr) ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
extern starch_iqdata_conv_uc8_ptr starch_iqdata_conv_uc8;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_iqdata_conv_uc8_ptr callable;
    int (*flavor_supported)();
} starch_iqdata_conv_uc8_regentry;

extern starch_iqdata_conv_uc8_regentry starch_iqdata_conv_uc8_registry[];
starch_iqdata_conv_uc8_regentry * starch_iqdata_conv_uc8_select();
void starch_iqdata_conv_uc8_set_wisdom( const char * const * received_wisdom );

typedef void (* starch_iqdata_conv_sc16_ptr) ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
extern starch_iqdata_conv_sc16_ptr starch_iqdata_conv_sc16;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_iqdata_conv_sc16_ptr callable;
    int (*flavor_supported)();
} starch_iqdata_conv_sc16_regentry;

extern starch_iqdata_conv_sc16_regentry starch_iqdata_conv_sc16_registry[];
starch_iqdata_conv_sc16_regentry * starch_iqdata_conv_sc16_select();
void starch_iqdata_conv_sc16_set_wisdom( const char * const * received_wisdom );

typedef void (* starch_iqdata_conv_sc16q11_ptr) ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
extern starch_iqdata_conv_sc16q11_ptr starch_iqdata_conv_sc16q11;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_iqdata_conv_sc16q11_ptr callable;
    int (*flavor_supported)();
} starch_iqdata_conv_sc16q11_regentry;

extern starch_iqdata_conv_sc16q11_regentry starch_iqdata_conv_sc16q11_registry[];
starch_iqdata_conv_sc16q11_regentry * starch_iqdata_conv_sc16q11_select();
void starch_iqdata_conv_sc16q11_set_wisdom( const char * const * received_wisdom );

typedef int (* starch_upsample12_ptr) ( const int16_t * arg0, int arg1, int arg2, uint16_t * arg3 );
extern starch_upsample12_ptr starch_upsample12;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_upsample12_ptr callable;
    int (*flavor_supported)();
} starch_upsample12_regentry;

extern starch_upsample12_regentry starch_upsample12_registry[];
starch_upsample12_regentry * starch_upsample12_select();
void starch_upsample12_set_wisdom( const char * const * received_wisdom );

typedef int (* starch_locate_preamble_ptr) ( const uint16_t * arg0, int arg1, uint8_t * arg2 );
extern starch_locate_preamble_ptr starch_locate_preamble;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_locate_preamble_ptr callable;
    int (*flavor_supported)();
} starch_locate_preamble_regentry;

extern starch_locate_preamble_regentry starch_locate_preamble_registry[];
starch_locate_preamble_regentry * starch_locate_preamble_select();
void starch_locate_preamble_set_wisdom( const char * const * received_wisdom );


/* flavors and prototypes */

#ifdef STARCH_FLAVOR_ARMV7A_NEON_VFPV4
int cpu_supports_armv7_neon_vfpv4 (void);
void starch_magnitude_power_uc8_twopass_armv7a_neon_vfpv4 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_power_uc8_lookup_armv7a_neon_vfpv4 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_power_uc8_lookup_unroll_4_armv7a_neon_vfpv4 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_iqdata_conv_uc8_arith_armv7a_neon_vfpv4 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
int starch_upsample12_generic_armv7a_neon_vfpv4 ( const int16_t * arg0, int arg1, int arg2, uint16_t * arg3 );
int starch_locate_preamble_generic_armv7a_neon_vfpv4 ( const uint16_t * arg0, int arg1, uint8_t * arg2 );
void starch_magnitude_sc16_exact_u32_armv7a_neon_vfpv4 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16_exact_float_armv7a_neon_vfpv4 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_iqdata_conv_sc16_arith_armv7a_neon_vfpv4 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_sc16q11_exact_u32_armv7a_neon_vfpv4 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_exact_float_armv7a_neon_vfpv4 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_11bit_table_armv7a_neon_vfpv4 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_12bit_table_armv7a_neon_vfpv4 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_iqdata_conv_sc16q11_arith_armv7a_neon_vfpv4 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_mean_power_u16_float_armv7a_neon_vfpv4 ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_mean_power_u16_u32_armv7a_neon_vfpv4 ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_mean_power_u16_u64_armv7a_neon_vfpv4 ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_count_above_u16_generic_armv7a_neon_vfpv4 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 );
void starch_magnitude_uc8_lookup_armv7a_neon_vfpv4 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_uc8_lookup_unroll_4_armv7a_neon_vfpv4 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_uc8_exact_armv7a_neon_vfpv4 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_mean_power_u16_neon_float_armv7a_neon_vfpv4 ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_magnitude_uc8_neon_vrsqrte_armv7a_neon_vfpv4 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_count_above_u16_neon_armv7a_neon_vfpv4 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 );
void starch_magnitude_power_uc8_neon_vrsqrte_armv7a_neon_vfpv4 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_sc16_neon_vrsqrte_armv7a_neon_vfpv4 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_neon_vrsqrte_armv7a_neon_vfpv4 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
int starch_upsample12_neon_armv7a_neon_vfpv4 ( const int16_t * arg0, int arg1, int arg2, uint16_t * arg3 );
int starch_locate_preamble_neon_armv7a_neon_vfpv4 ( const uint16_t * arg0, int arg1, uint8_t * arg2 );
#endif /* STARCH_FLAVOR_ARMV7A_NEON_VFPV4 */

int starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_ARMV8_NEON_SIMD
int cpu_supports_armv8_simd (void);
void starch_magnitude_power_uc8_twopass_armv8_neon_simd ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_power_uc8_lookup_armv8_neon_simd ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_power_uc8_lookup_unroll_4_armv8_neon_simd ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_iqdata_conv_uc8_arith_armv8_neon_simd ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
int starch_upsample12_generic_armv8_neon_simd ( const int16_t * arg0, int arg1, int arg2, uint16_t * arg3 );
int starch_locate_preamble_generic_armv8_neon_simd ( const uint16_t * arg0, int arg1, uint8_t * arg2 );
void starch_magnitude_sc16_exact_u32_armv8_neon_simd ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16_exact_float_armv8_neon_simd ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_iqdata_conv_sc16_arith_armv8_neon_simd ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_sc16q11_exact_u32_armv8_neon_simd ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_exact_float_armv8_neon_simd ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_11bit_table_armv8_neon_simd ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_12bit_table_armv8_neon_simd ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_iqdata_conv_sc16q11_arith_armv8_neon_simd ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_mean_power_u16_float_armv8_neon_simd ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_mean_power_u16_u32_armv8_neon_simd ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_mean_power_u16_u64_armv8_neon_simd ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_count_above_u16_generic_armv8_neon_simd ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 );
void starch_magnitude_uc8_lookup_armv8_neon_simd ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_uc8_lookup_unroll_4_armv8_neon_simd ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_uc8_exact_armv8_neon_simd ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_mean_power_u16_neon_float_armv8_neon_simd ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_magnitude_uc8_neon_vrsqrte_armv8_neon_simd ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_count_above_u16_neon_armv8_neon_simd ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 );
void starch_magnitude_power_uc8_neon_vrsqrte_armv8_neon_simd ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_sc16_neon_vrsqrte_armv8_neon_simd ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_neon_vrsqrte_armv8_neon_simd ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
int starch_upsample12_neon_armv8_neon_simd ( const int16_t * arg0, int arg1, int arg2, uint16_t * arg3 );
int starch_locate_preamble_neon_armv8_neon_simd ( const uint16_t * arg0, int arg1, uint8_t * arg2 );
#endif /* STARCH_FLAVOR_ARMV8_NEON_SIMD */

int starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_GENERIC
void starch_magnitude_power_uc8_twopass_generic ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_power_uc8_lookup_generic ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_power_uc8_lookup_unroll_4_generic ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_iqdata_conv_uc8_arith_generic ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
int starch_upsample12_generic_generic ( const int16_t * arg0, int arg1, int arg2, uint16_t * arg3 );
int starch_locate_preamble_generic_generic ( const uint16_t * arg0, int arg1, uint8_t * arg2 );
void starch_magnitude_sc16_exact_u32_generic ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16_exact_float_generic ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_iqdata_conv_sc16_arith_generic ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_sc16q11_exact_u32_generic ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_exact_float_generic ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_11bit_table_generic ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_12bit_table_generic ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_iqdata_conv_sc16q11_arith_generic ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_mean_power_u16_float_generic ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_mean_power_u16_u32_generic ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_mean_power_u16_u64_generic ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_count_above_u16_generic_generic ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 );
void starch_magnitude_uc8_lookup_generic ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_uc8_lookup_unroll_4_generic ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_uc8_exact_generic ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
#endif /* STARCH_FLAVOR_GENERIC */

int starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_X86_AVX2
int cpu_supports_avx2 (void);
void starch_magnitude_power_uc8_twopass_x86_avx2 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_power_uc8_lookup_x86_avx2 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_power_uc8_lookup_unroll_4_x86_avx2 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_iqdata_conv_uc8_arith_x86_avx2 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
int starch_upsample12_generic_x86_avx2 ( const int16_t * arg0, int arg1, int arg2, uint16_t * arg3 );
int starch_locate_preamble_generic_x86_avx2 ( const uint16_t * arg0, int arg1, uint8_t * arg2 );
void starch_magnitude_sc16_exact_u32_x86_avx2 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16_exact_float_x86_avx2 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_iqdata_conv_sc16_arith_x86_avx2 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_magnitude_sc16q11_exact_u32_x86_avx2 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_exact_float_x86_avx2 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_11bit_table_x86_avx2 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_sc16q11_12bit_table_x86_avx2 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_iqdata_conv_sc16q11_arith_x86_avx2 ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );
void starch_mean_power_u16_float_x86_avx2 ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_mean_power_u16_u32_x86_avx2 ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_mean_power_u16_u64_x86_avx2 ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );
void starch_count_above_u16_generic_x86_avx2 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 );
void starch_magnitude_uc8_lookup_x86_avx2 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_uc8_lookup_unroll_4_x86_avx2 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
void starch_magnitude_uc8_exact_x86_avx2 ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );
int starch_upsample12_avx2_x86_avx2 ( const int16_t * arg0, int arg1, int arg2, uint16_t * arg3 );
int starch_locate_preamble_avx2_x86_avx2 ( const uint16_t * arg0, int arg1, uint8_t * arg2 );
#endif /* STARCH_FLAVOR_X86_AVX2 */

int starch_read_wisdom (const char * path);

