# Use of generative AI on this branch

This branch (`demod-2400x`) contains code and text written with Claude Code
(Anthropic, Claude Opus models), working under my direction.

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

## How it was checked

* Every SIMD kernel is checked bit-for-bit against the generic version by
  `starch-benchmark -V` (`make test`), on AArch64, ARMv7 and x86 AVX2,
  including full-range worst-case input and a 64-bit check of the generic
  upsampler.
* Decoded output was compared with stock dump1090-fa on real RTL-SDR
  captures, message by message (contents, signal level and timestamps).
* Synthetic frames at known times (`sim/`) check sensitivity and MLAT
  timestamp accuracy.
* The build has run live on my Raspberry Pi 4 receiver feeding FlightAware,
  with MLAT synchronized as with the stock build.
