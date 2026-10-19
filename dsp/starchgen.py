#!/usr/bin/env python3

import os
import sys
import glob

top_dir = sys.argv[1]
starch_dir = os.path.join(top_dir, 'starch')
sys.path.append(starch_dir)
import starch

gen = starch.Generator(runtime_dir = top_dir,
                       output_dir = os.path.join(top_dir, 'dsp', 'generated'))

gen.add_include('"dsp-types.h"')
gen.add_include('"cpu.h"')

gen.add_function(
    name = 'magnitude_uc8',
    argtypes = ['const uc8_t *', 'uint16_t *', 'unsigned'])
gen.add_function(
    name = 'magnitude_power_uc8',
    argtypes = ['const uc8_t *', 'uint16_t *', 'unsigned', 'double *', 'double *'])
gen.add_function(
    name = 'magnitude_sc16',
    argtypes = ['const sc16_t *', 'uint16_t *', 'unsigned'])
gen.add_function(
    name = 'magnitude_sc16q11',
    argtypes = ['const sc16_t *', 'uint16_t *', 'unsigned'])
gen.add_function(
    name = 'mean_power_u16',
    argtypes = ['const uint16_t *', 'unsigned', 'double *', 'double *'])
gen.add_function(
    name = 'count_above_u16',
    argtypes = ['const uint16_t *', 'unsigned', 'uint16_t', 'unsigned *'])
gen.add_function(
   name = 'iqdata_conv_uc8',
    argtypes = ['const uc8_t *', 'uint16_t *', 'unsigned', 'const int32_t *', 'int32_t *', 'double *', 'double *'])
gen.add_function(
    name = 'iqdata_conv_sc16',
    argtypes = ['const sc16_t *', 'uint16_t *', 'unsigned', 'const int32_t *', 'int32_t *', 'double *', 'double *'])
gen.add_function(
    name = 'iqdata_conv_sc16q11',
    argtypes = ['const sc16_t *', 'uint16_t *', 'unsigned', 'const int32_t *', 'int32_t *', 'double *', 'double *'])
gen.add_function(
   name = 'upsample12',
   argtypes = ['const int16_t *', 'int', 'int', 'uint16_t *'],
   returntype = 'int')
gen.add_function(
   name = 'locate_preamble',
   argtypes = ['const uint16_t *', 'int', 'unsigned', 'uint8_t *'],
   returntype = 'int')       

gen.add_flavor(
    name = 'generic',
    description = 'Generic build, default compiler options',
    compile_flags = [])
armv7a_neon_vfpv4 = gen.add_flavor(
    name = 'armv7a_neon_vfpv4',
    description = 'ARMv7-A, NEON, VFPv4',
    compile_flags = ['-march=armv7-a+neon-vfpv4', '-mfpu=neon-vfpv4', '-ffast-math'],
    test_function = 'cpu_supports_armv7_neon_vfpv4')
armv8_neon_simd = gen.add_flavor(
    name = 'armv8_neon_simd',
    description = 'ARMv8-A, NEON, SIMD',
    compile_flags = ['-march=armv8-a+simd', '-ffast-math'],
    test_function = 'cpu_supports_armv8_simd')
gen.add_flavor(
    name = 'x86_avx2',
    description = 'x86 with AVX2',
    compile_flags = ['-mavx2', '-ffast-math'],
    test_function = 'cpu_supports_avx2')

gen.add_mix(
    name = 'generic',
    description = 'Generic build, compiler defaults only',
    flavors = ['generic'],
    wisdom_file = 'wisdom.generic')
gen.add_mix(
    name = 'arm',
    description = 'ARM, 32-bit',
    flavors = ['armv7a_neon_vfpv4', 'generic'],
    wisdom_file = 'wisdom.arm')
gen.add_mix(
    name = 'aarch64',
    description = 'ARM, 64-bit',
    flavors = ['armv8_neon_simd', 'generic'],
    wisdom_file = 'wisdom.aarch64')
gen.add_mix(
    name = 'x86',
    description = 'x86-64',
    flavors = ['x86_avx2', 'generic'],
    wisdom_file = 'wisdom.x86')

gen.scan_impls('dsp/impl/*.generic.c')
gen.scan_impls('dsp/impl/*.neon.c', flavors=[armv7a_neon_vfpv4, armv8_neon_simd])
gen.scan_impls('dsp/impl/*.avx2.c', flavors=['x86_avx2'])
gen.scan_benchmarks('dsp/impl/*.benchmark.c')

gen.generate()
