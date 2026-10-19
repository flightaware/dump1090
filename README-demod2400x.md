# DEMOD_2400X demodulator

This branch adds a new Mode S demodulator for 2.4 MHz SDR input
(`demod_2400x.c`, enabled with `-DDEMOD_2400X -DUSE_MAGNITUDE` in the
Makefile; the build reports version `11.1x`). In outline:

* DC removal and conversion to 12-bit I/Q in the input converter, so the
  FIFO keeps I/Q samples for later processing.
* A 5-phase polyphase FIR upsamples to 12 MHz (least-squares fractional-delay
  coefficients, all integer arithmetic), then takes the magnitude.
* A preamble metric at every 12 MHz position, and the 112 bits are decoded
  from the best of the neighbouring positions.
* The arrival time is refined by correlating the decoded chips with the
  signal, so MLAT timestamps are unbiased across sub-sample phases.
* Adaptive gain and signal level statistics behave as in the stock
  demodulator.

The heavy kernels (`iqdata_conv_*`, `upsample12`, `locate_preamble`,
`decode_bits`) are starch kernels with generic, ARM NEON (ARMv7 and AArch64)
and x86 AVX2 versions. All versions give bit-identical results, which
`starch-benchmark -V` checks, including on full-range worst-case input.

## Results

A 60 s capture from a Raspberry Pi 4 with a FlightAware Pro Stick Plus
(gain 49.6 dB), with an outdoor antenna on a 3 m pole and 12 m of LMR400,
decoded by stock dump1090-fa 11.1 and by this branch with the same options
(`--fix`, receiver position set):

| | Stock | DEMOD_2400X | Change |
|---|---|---|---|
| Usable messages | 73,146 | 84,389 | +15% |
| DF17 (ADS-B) | 10,352 | 11,795 | +14% |
| Global CPR positions | 2,807 | 3,750 | +34% |
| Aircraft | 136 | 141 | +5 |

The gain is concentrated in weak signals. Messages by signal strength:

| RSSI (dBFS) | Stock | DEMOD_2400X | Change |
|---|---|---|---|
| −30 to −27 | 131 | 302 | +131% |
| −27 to −24 | 2,347 | 4,205 | +79% |
| −24 to −21 | 7,111 | 9,407 | +32% |
| −21 to −18 | 9,010 | 10,759 | +19% |
| −18 to −15 | 12,213 | 13,703 | +12% |
| −15 to −10 | 18,226 | 19,924 | +9% |
| −10 to −5 | 9,952 | 10,710 | +8% |
| above −5 | 14,151 | 15,375 | +9% |

Position messages by distance from the receiver:

| Distance | Stock | DEMOD_2400X | Change | Aircraft (stock / new) |
|---|---|---|---|---|
| 0–50 km | 643 | 792 | +23% | 12 / 13 |
| 50–100 km | 1,225 | 1,520 | +24% | 30 / 30 |
| 100–150 km | 746 | 957 | +28% | 33 / 33 |
| 150–200 km | 356 | 472 | +33% | 22 / 22 |
| 200–250 km | 93 | 130 | +40% | 8 / 13 |
| 250–300 km | 11 | 24 | +118% | 2 / 5 |

Weak synthetic frames (2,000 DF17 frames with a random ±100 kHz carrier
offset, decoded out of 2,000): 689 (stock) against 1,404 (DEMOD_2400X).
The generator and checker are in [sim/](sim/).

## Timing (MLAT)

On synthetic frames at known 12 MHz ticks, covering all 5 sub-sample
phases:

| | Stock | DEMOD_2400X |
|---|---|---|
| Bias across phases (clean) | 0 to +2 ticks | none |
| Jitter, noisy frames (σ) | 0.41–0.48 ticks | < 0.2 ticks |

One tick is 83.3 ns. Running live on the Pi 4, mlat-client stays synchronized
with about 260 nearby receivers, as with stock.

## CPU

Demodulation time for the 60 s capture:

| | Stock | DEMOD_2400X |
|---|---|---|
| Raspberry Pi 5 (Cortex-A76) | 2.0 s | 4.3 s (gcc 14), 4.1 s (clang 19) |
| Raspberry Pi 4 (Cortex-A72) | 3.9 s | 10.6 s |

The Pi 4 times were measured while the live receiver was also running on
the same Pi, so both are probably slightly high. Live on the Pi 4 this is
about 31% of one core. These results come from
one receiver and a one-minute capture; longer side-by-side runs are in
progress.

## Compilers

Built with the same options as the GitHub workflow
(`make RTLSDR=yes BLADERF=yes HACKRF=yes LIMESDR=yes SOAPYSDR=yes all`,
then `make test`):

| Compiler | Target | Warnings | `make test` |
|---|---|---|---|
| gcc 14.2 | AArch64 (Pi 5) | 0 | pass (65 validations) |
| clang 19.1 | AArch64 (Pi 5) | 0 | pass (65 validations) |
| gcc 14.2 (cross) | ARMv7 | 0 | `starch-benchmark -V` passes, run natively on AArch64 |
| gcc 14.2 (cross) | x86-64 AVX2 | 0 | pass under qemu-x86_64 |
| clang 19.1 (cross) | x86-64 AVX2 | 0 | pass under qemu-x86_64 (59 validations) |

The gcc and clang AArch64 builds give byte-identical decoder output,
timestamps included. On x86, clang with `-ffast-math` compiles every AVX2
divide and square root to a reciprocal estimate (`vrcpps`/`vrsqrtps`), so the
kernels correct the estimate in both directions to stay exact. QEMU computes
these estimates exactly, so x86 correctness also needs a run on real
hardware. The AVX2 kernels passed on an Intel CPU before the mirrored
upsampler and `decode_bits` were added; those still need an Intel run.
