# Use of generative AI on this branch

This branch (`demod-2400x-dev`, which carries the work of `demod-2400x` and
`demod-2400x-coherent` over to upstream `dev`) contains
code and text written with Claude Code (Anthropic, Claude Opus models),
working under my direction.

## Who did what

The DEMOD_2400X design is mine: 12 MHz polyphase upsampling of the I/Q, a
magnitude preamble metric at every 12 MHz position, keeping I/Q in the FIFO
for later coherent processing, and the split into starch kernels. So is the
initial implementation.

Claude Code wrote code to my specification and review, and ran tests and
measurements I asked for. I made the design decisions along the way, for
example integer arithmetic over floating point, generic kept as the plain
reference implementation, equal coefficient sums across phases, the
upsampler rounding shift, and which experiments to drop.

Commits with a `Co-Authored-By: Claude` trailer were written with Claude
Code. Some earlier commits without the trailer also contain AI-written code,
notably the vectorised `locate_preamble`, the exact integer square root, the
NEON `upsample12` and the DC removal in the input converters
(`3a7afd3`, `5a51c13`, `d70f5d4`, `0f244bc`). The `sim/` tools,
README-demod2400x.md and this file were also written with Claude Code.

The coherent second pass (`-DDEMOD_2400X_COHERENT`) follows my specification:
a separate file called from `emitFrame()` for frames the magnitude bits
don't decode, the frequency and phase estimated for each 56-bit half on its
own (and the second half decoded whatever the DF says), a
correlator (matched filter) in blocks of 8 bits rather than an FFT or phase
angles, integer arithmetic (int16 where possible), transmitters without a
stable carrier left alone, and each bit's carrier sample taken from the chip
with the larger magnitude. Claude Code wrote the Python models, the kernel
and its SIMD versions, and ran the comparisons, under my review.
COHERENT-demod2400x.md was also written with Claude Code. The later
two-stage search over ±250 kHz (coarse bins 31.25 kHz apart, then fine
bins 4 kHz apart around the best one, none at DC) and the NEON
rewrite of the coherent kernel for the Cortex-A53 were written by Claude Code
to my choices; the NEON version gives exactly the same results as generic.

The run-time options `--no-demod2400x` and `--no-coherent` (both on by
default) and the fallback to the stock demodulator are also my design: hand
buffers to the stock demodulator while DEMOD_2400X is behind with live input,
never with a file, and convert the I/Q buffer to magnitudes only once, in a
single array whose pointer is reset before each buffer. Claude Code wrote the
code and ran the tests.

## How it was checked

* Every SIMD kernel is checked bit-for-bit against the generic version by
  `starch-benchmark -V` (`make test`), on AArch64, ARMv7 and x86 AVX2,
  including full-range worst-case input and a 64-bit check of the generic
  upsampler.
* Decoded output was compared with stock dump1090-fa on an RTL-SDR
  recording (FlightAware Pro Stick Plus), message by message (contents,
  signal level and timestamps). An SDRplay recording was used to check that
  each change gave the same or more messages than the version before.
* Synthetic frames at known times (`sim/`) check sensitivity and MLAT
  timestamp accuracy.
* With `--no-demod2400x`, and in a build without the defines, the output is
  byte-identical to upstream `dev`'s stock build (`--raw --mlat`, messages
  and timestamps, with and without `--modeac`) on RTL-SDR and SDRplay
  recordings.
* The fallback was tested with the demodulator's core deliberately starved,
  on a Pi 3B and a Pi 5: it switched to stock and back, with no dropped
  buffers.
* The build has run live on my Raspberry Pi 4 receiver feeding FlightAware,
  with MLAT synchronized as with the stock build.

## DCO

I certify the [DCO](https://developercertificate.org/), clause (a), for the
whole branch. The AI output was produced at my direction for this change,
written against this codebase, and reviewed by me before it was committed;
it was not copied from other projects. Anthropic's terms assign to me
whatever rights Anthropic has in the output, so I have the right to submit it
under the project's licence (GPL-2.0-or-later).
