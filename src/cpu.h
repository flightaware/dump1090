#ifndef DUMP1090_CPU_H
#define DUMP1090_CPU_H

// x86
int cpu_supports_avx(void);
int cpu_supports_avx2(void);

// ARM
int cpu_supports_armv7_neon_vfpv4(void);

// AARCH64
int cpu_supports_armv8_simd(void);
int cpu_supports_armv8_simd_sve(void);

#endif
