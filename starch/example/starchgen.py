#!/usr/bin/env python3

import sys
import os
import glob

example_dir = os.path.dirname(sys.argv[0])
starch_dir = os.path.join(example_dir, '..')
sys.path.append(starch_dir)
import starch

gen = starch.Generator(runtime_dir = example_dir,
                       output_dir = os.path.join(example_dir, 'generated'))

gen.add_include('<stdint.h>')

gen.add_function(name = 'subtract_n', argtypes = ['const uint16_t *', 'unsigned', 'uint16_t', 'uint16_t *'], aligned=True)

gen.add_feature(name='neon',
                description='ARM NEON v2')

gen.add_flavor(name = 'generic',
               description = 'Generic build, default compiler options',
               compile_flags = [])
gen.add_flavor(name = 'armv7a_vfpv3',
               description = 'ARMv7-A, NEON, VFPv3',
               compile_flags = ['-march=armv7-a+neon-vfpv3', '-mfpu=neon-vfpv3', '-ffast-math'],
               features = ['neon'],
               test_function = 'supports_neon_vfpv3',
               alignment=16)
gen.add_flavor(name = 'armv7a_vfpv4',
               description = 'ARMv7-A, NEON, VFPv4',
               compile_flags = ['-march=armv7-a+neon-vfpv4', '-mfpu=neon-vfpv4', '-ffast-math'],
               features = ['neon'],
               test_function = 'supports_neon_vfpv4',
               alignment=16)
gen.add_flavor(name = 'x86_64_avx',
               description = 'x86-64 with AVX',
               compile_flags = ['-mavx', '-ffast-math'],
               features = [],
               test_function = 'supports_x86_avx',
               alignment=32)
gen.add_flavor(name = 'x86_64_avx2',
               description = 'x86-64 with AVX2',
               compile_flags = ['-mavx2', '-ffast-math'],
               features = [],
               test_function = 'supports_x86_avx2',
               alignment=32)

gen.add_mix(name = 'generic',
            description = 'Generic build, compiler defaults only',
            flavors = ['generic'])

gen.add_mix(name = 'arm',
            description = 'ARM',
            flavors = ['armv7a_vfpv4', 'armv7a_vfpv3', 'generic'],
            wisdom = {
                'subtract_n': ['neon_intrinsics_armv7a_vfpv4', 'neon_intrinsics_armv7a_vfpv3', 'generic_generic']
            })
gen.add_mix(name = 'x86_64',
            description = 'x64-64',
            flavors = ['x86_64_avx2', 'x86_64_avx', 'generic'],
            wisdom = {
                'subtract_n': ['generic_x86_64_avx2', 'generic_x86_64_avx', 'unroll_4_generic']
            })

for pattern in ['impl/*.c', 'benchmark/*.c']:
    for c_file in glob.glob(pattern):
        gen.scan_file(c_file)

gen.generate()
