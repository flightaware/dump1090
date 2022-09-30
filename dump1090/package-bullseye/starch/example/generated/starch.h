
/* starch generated code. Do not edit. */

#include <stdint.h>

/* mixes */

/* ARM */
#ifdef STARCH_MIX_ARM
#define STARCH_FLAVOR_ARMV7A_VFPV4
#define STARCH_FLAVOR_ARMV7A_VFPV3
#define STARCH_FLAVOR_GENERIC
#define STARCH_MIX_ALIGNMENT 16
#endif /* STARCH_MIX_ARM */

/* Generic build, compiler defaults only */
#ifdef STARCH_MIX_GENERIC
#define STARCH_FLAVOR_GENERIC
#define STARCH_MIX_ALIGNMENT 1
#endif /* STARCH_MIX_GENERIC */

/* x64-64 */
#ifdef STARCH_MIX_X86_64
#define STARCH_FLAVOR_X86_64_AVX2
#define STARCH_FLAVOR_X86_64_AVX
#define STARCH_FLAVOR_GENERIC
#define STARCH_MIX_ALIGNMENT 32
#endif /* STARCH_MIX_X86_64 */


#ifdef STARCH_MIX_ALIGNMENT
#define STARCH_ALIGNMENT STARCH_MIX_ALIGNMENT
#define STARCH_IS_ALIGNED(_ptr) (((uintptr_t)(_ptr) & (STARCH_MIX_ALIGNMENT-1)) == 0)
#else
/* mix not defined, alignment is unknown, treat everything as unaligned */
#define STARCH_IS_ALIGNED(_ptr) (0)
#endif


/* entry points and registries */

typedef void (* starch_subtract_n_ptr) ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
extern starch_subtract_n_ptr starch_subtract_n;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_subtract_n_ptr callable;
    int (*flavor_supported)();
} starch_subtract_n_regentry;

extern starch_subtract_n_regentry starch_subtract_n_registry[];
starch_subtract_n_regentry * starch_subtract_n_select();
void starch_subtract_n_set_wisdom( const char * const * received_wisdom );

typedef void (* starch_subtract_n_aligned_ptr) ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
extern starch_subtract_n_aligned_ptr starch_subtract_n_aligned;

typedef struct {
    int rank;
    const char *name;
    const char *flavor;
    starch_subtract_n_aligned_ptr callable;
    int (*flavor_supported)();
} starch_subtract_n_aligned_regentry;

extern starch_subtract_n_aligned_regentry starch_subtract_n_aligned_registry[];
starch_subtract_n_aligned_regentry * starch_subtract_n_aligned_select();
void starch_subtract_n_aligned_set_wisdom( const char * const * received_wisdom );

/* flavors and prototypes */

#ifdef STARCH_FLAVOR_ARMV7A_VFPV3
int supports_neon_vfpv3 (void);
void starch_subtract_n_generic_armv7a_vfpv3 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_generic_armv7a_vfpv3 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_unroll_4_armv7a_vfpv3 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_unroll_4_armv7a_vfpv3 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_bad_implementation_armv7a_vfpv3 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_bad_implementation_armv7a_vfpv3 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_neon_intrinsics_armv7a_vfpv3 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_neon_intrinsics_armv7a_vfpv3 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
#endif /* STARCH_FLAVOR_ARMV7A_VFPV3 */

int starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_ARMV7A_VFPV4
int supports_neon_vfpv4 (void);
void starch_subtract_n_generic_armv7a_vfpv4 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_generic_armv7a_vfpv4 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_unroll_4_armv7a_vfpv4 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_unroll_4_armv7a_vfpv4 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_bad_implementation_armv7a_vfpv4 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_bad_implementation_armv7a_vfpv4 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_neon_intrinsics_armv7a_vfpv4 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_neon_intrinsics_armv7a_vfpv4 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
#endif /* STARCH_FLAVOR_ARMV7A_VFPV4 */

int starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_GENERIC
void starch_subtract_n_generic_generic ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_unroll_4_generic ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_bad_implementation_generic ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
#endif /* STARCH_FLAVOR_GENERIC */

int starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_X86_64_AVX
int supports_x86_avx (void);
void starch_subtract_n_generic_x86_64_avx ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_generic_x86_64_avx ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_unroll_4_x86_64_avx ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_unroll_4_x86_64_avx ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_bad_implementation_x86_64_avx ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_bad_implementation_x86_64_avx ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
#endif /* STARCH_FLAVOR_X86_64_AVX */

int starch_read_wisdom (const char * path);

#ifdef STARCH_FLAVOR_X86_64_AVX2
int supports_x86_avx2 (void);
void starch_subtract_n_generic_x86_64_avx2 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_generic_x86_64_avx2 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_unroll_4_x86_64_avx2 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_unroll_4_x86_64_avx2 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_bad_implementation_x86_64_avx2 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
void starch_subtract_n_aligned_bad_implementation_x86_64_avx2 ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, uint16_t * arg3 );
#endif /* STARCH_FLAVOR_X86_64_AVX2 */

int starch_read_wisdom (const char * path);

