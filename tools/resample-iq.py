#!/usr/bin/env python3
#
# Convert an IQ recording to the 2.4 MS/s input that dump1090-fa expects
# (dump1090 --ifile FILE --iformat UC8|SC16, or tools/compare-builds.sh --iq).
#
#   tools/resample-iq.py IN OUT --in-format ci16_le --in-rate 8e6 --out-format sc16
#
# Input formats: uc8 (rtl_sdr), ci16_le / sc16 (signed 16-bit I/Q), cf32_le.
# Output formats: sc16 (keeps the dynamic range) or uc8 (8-bit like an
# RTL-SDR; --uc8-peak sets which input magnitude percentile maps to full
# scale, so strong aircraft come out near full scale and the noise floor at
# a few counts, as with a real dongle).
#
# Resampling uses scipy.signal.resample_poly (anti-aliasing FIR filter), so
# the result is band-limited to what a 2.4 MS/s receiver would see.
#
# Needs numpy and scipy.

import argparse
from fractions import Fraction
import sys

import numpy as np
from scipy.signal import resample_poly


def read(path, fmt):
    if fmt == 'uc8':
        raw = np.fromfile(path, dtype=np.uint8).astype(np.float32) - 127.5
    elif fmt in ('ci16_le', 'sc16'):
        raw = np.fromfile(path, dtype='<i2').astype(np.float32)
    elif fmt == 'cf32_le':
        raw = np.fromfile(path, dtype='<f4')
    else:
        sys.exit('unknown input format ' + fmt)
    raw = raw[: len(raw) // 2 * 2]
    return raw[0::2] + 1j * raw[1::2]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('input')
    ap.add_argument('output')
    ap.add_argument('--in-format', required=True, choices=['uc8', 'ci16_le', 'sc16', 'cf32_le'])
    ap.add_argument('--in-rate', type=float, required=True, help='input sample rate in samples/second')
    ap.add_argument('--out-rate', type=float, default=2.4e6)
    ap.add_argument('--out-format', choices=['sc16', 'uc8'], default='sc16')
    ap.add_argument('--uc8-peak', type=float, default=99.99,
                    help='uc8 only: magnitude percentile that maps to full scale (default 99.99)')
    args = ap.parse_args()

    x = read(args.input, args.in_format).astype(np.complex64)
    ratio = Fraction(args.out_rate / args.in_rate).limit_denominator(1000)
    print(f'{len(x)} samples at {args.in_rate / 1e6:g} MS/s -> x{ratio.numerator}/{ratio.denominator}', file=sys.stderr)
    y = resample_poly(x, ratio.numerator, ratio.denominator).astype(np.complex64)
    mag = np.abs(y)

    if args.out_format == 'sc16':
        scale = 30000.0 / max(float(mag.max()), 1e-9)
        out = np.empty(2 * len(y), dtype='<i2')
        out[0::2] = np.clip(np.rint(y.real * scale), -32768, 32767)
        out[1::2] = np.clip(np.rint(y.imag * scale), -32768, 32767)
    else:
        scale = 127.0 / max(float(np.percentile(mag, args.uc8_peak)), 1e-9)
        out = np.empty(2 * len(y), dtype=np.uint8)
        out[0::2] = np.clip(np.rint(y.real * scale + 127.4), 0, 255)
        out[1::2] = np.clip(np.rint(y.imag * scale + 127.4), 0, 255)
        print(f'uc8: median magnitude {np.median(mag) * scale:.1f} counts, '
              f'{100.0 * np.mean(mag * scale > 127):.3f}% of samples clipped', file=sys.stderr)

    out.tofile(args.output)
    print(f'wrote {len(y)} samples at {args.out_rate / 1e6:g} MS/s ({args.out_format}) to {args.output}', file=sys.stderr)


if __name__ == '__main__':
    main()
