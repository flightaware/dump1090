#include "cpu.h"

#include <stdbool.h>

#ifdef ENABLE_CPUFEATURES
#include "cpu_features_macros.h"
#endif

//
// x86
//

#ifdef CPU_FEATURES_ARCH_X86
#include "cpuinfo_x86.h"

static X86Info *x86_info()
{
    static bool valid = false;
    static X86Info cache;

    if (!valid) {
        cache = GetX86Info();
        valid = true;
    }

    return &cache;
}

#endif

int cpu_supports_avx(void)
{
#ifdef CPU_FEATURES_ARCH_X86
    return x86_info()->features.avx;
#else
    return 0;
#endif
}

int cpu_supports_avx2(void)
{
#ifdef CPU_FEATURES_ARCH_X86
    return x86_info()->features.avx2;
#else
    return 0;
#endif
}

//
// ARM
//

#ifdef CPU_FEATURES_ARCH_ARM
#include "cpuinfo_arm.h"

static ArmInfo *arm_info()
{
    static bool valid = false;
    static ArmInfo cache;

    if (!valid) {
        cache = GetArmInfo();
        valid = true;
    }

    return &cache;
}

#endif

int cpu_supports_armv7_neon_vfpv4(void)
{
#ifdef CPU_FEATURES_ARCH_ARM
    return arm_info()->architecture >= 7 && arm_info()->features.neon && arm_info()->features.vfpv4 && arm_info()->features.vfpd32;
#else
    return 0;
#endif
}

//
// AARCH64
//

#ifdef CPU_FEATURES_ARCH_AARCH64
#include "cpuinfo_aarch64.h"

static Aarch64Info *aarch64_info()
{
    static bool valid = false;
    static Aarch64Info cache;

    if (!valid) {
        cache = GetAarch64Info();
        valid = true;
    }

    return &cache;
}

#endif

int cpu_supports_armv8_simd(void)
{
#ifdef CPU_FEATURES_ARCH_AARCH64
    return aarch64_info()->features.asimd;
#else
    return 0;
#endif
}
