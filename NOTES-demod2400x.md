# DEMOD_2400X development notes

What was built, why, and what was tried and dropped. Results are in
[README-demod2400x.md](README-demod2400x.md). Times are for the 60 s Pi 4
RTL-SDR capture decoded on a Raspberry Pi 5 unless stated otherwise.

## Test data

* **SDRplay RSP1B recordings**: 2.4 MHz SC16 I/Q, IFGR 30, RFGR 0, made with
  a SoapySDR Python script. The early ones (17.8 s, then 60 s), used for the
  early development and speed work, didn't set the bandwidth, so the IF
  filter was 1.536 MHz (what SoapySDRPlay picks for a 2.4 MHz sample rate),
  which smears the pulses. The later 60 s one is at 5 MHz, dump1090's SDRplay
  default; recorded a minute apart, 5 MHz decoded 2.8 times as many messages
  as 1.536 MHz. The stock SoapySDR driver fails on the RSP1B (the stream doesn't
  start, and the gain runs backwards); that is fixed separately, not on this
  branch.
* **RTL-SDR recording**: 60 s at 2.4 MHz, UC8, from the live receiver: a
  Raspberry Pi 4 with the FlightAware Pro Stick Plus (blue, internal LNA and
  SAW filter) at gain 49.6 dB, outdoor antenna on a 3 m pole, 12 m of
  LMR400. Used for the stock comparison, the later tuning and CPU figures.
* **Synthetic frames** from `sim/gen_timing.c` (based on `gen_data.c`):
  DF17 frames at known 12 MHz ticks with noise and a random carrier offset,
  for sensitivity and MLAT timestamp accuracy.
* **Live**: the same Pi 4 receiver feeding FlightAware, including MLAT.

## Signal path

1. **Input conversion** (`iqdata_conv_uc8/sc16/sc16q11`, `convert.c`).
   Converts the SDR samples to 12-bit signed I/Q and removes DC in the same
   pass. The DC filter holds its correction for blocks of 16 samples
   (`IQ_DC_BLOCK`) and updates a running mean with a time constant of 4096
   samples, so SIMD versions need no serial loop. The filter state lives in
   `convert.c` (`converter_state`) and is passed in and out (`dc_in`,
   `dc_out`), never static, which also keeps benchmark verification
   repeatable. Mean level and power are computed after DC removal. The FIFO
   holds I/Q (`magbuf_sample()`), so later stages, such as a coherent
   demodulator, can reuse it.
2. **Upsampling** (`upsample12`). A 5-phase, 8-tap polyphase FIR takes
   2.4 MHz I/Q to 12 MHz, rounds by `UPSAMPLE_SHIFT` and stores an 8-bit log
   of the power, `L ≈ 8·log2(I² + Q² + 1)` (`demod_log8()`): the bits of the
   float of P + 1, shifted right by 20, minus 127·8, i.e. 8 × the exponent plus
   the top 3 mantissa bits. No square root, and one byte per sample. Output is
   stored as 6 rows, one per 12 MHz tick within a 0.5 µs chip.
3. **Preamble search** (`locate_preamble`). In logs the ratio becomes a
   difference: `(12·L(minp) − ΣL(gaps)) / 2`, clamped to a byte, from the
   smallest of the 4 preamble pulses and the 12 gaps, i.e. the smallest pulse
   against the geometric mean of the gaps. No divide; below `PEAMBLE_THRES`
   (86) it is set to 0.
4. **Candidates** (`demod_2400x.c`). Peaks are scanned 8 metric bytes at a
   time (a SWAR test against the threshold). For each peak, the first 56 bits
   are decoded at the peak and its two neighbours, and the best quality wins
   (`bestPhase`). The second 56 bits are always decoded too, because CRC
   correction can change the DF field, including the bit that selects a
   56- or 112-bit frame. A long frame other than DF17/18 is rejected if its
   second half quality is low.
5. **Bit decoding** (`decode_bits`). Bit = 1 if the first chip of the pair is
   larger. Quality = 5.5 × the mean of min(|La − Lb|, 32) over the bits (11/16
   of the sum per byte), so at most 176: roughly the old 256·Σ|a−b| / Σ(a+b),
   so the gates keep their meaning.
6. **Coherent second pass** (`demod_2400x_coherent.c`, the `coherent_decode`
   kernel). A frame whose bits fail the CRC, if the quality of its first 56
   bits is at least `COHERENT_GATE` (96), is decoded again from the I/Q kept
   in the FIFO, using the carrier phase. If the coherent bits pass the CRC
   they replace the original ones. See [Coherent second pass](#coherent-second-pass).
7. **Output** (`emitFrame`). Scored and decoded by the stock code. Then:
   * Fine arrival time: the decoded chips are correlated with the signal at
     `pos−2 … pos+2` and the best offset is used for the MLAT timestamp. The
     correlation turns the log bytes back into magnitudes (a 256-entry table),
     so weak noisy chips don't count as much as strong ones.
   * Signal level from the I/Q, with each sample's power capped at full
     scale as the stock magnitude kernels do.
   * Adaptive gain gets 2.4 MHz magnitudes and each decoded message, fed the
     same way as the stock demodulator (only when adaptive gain is on).

## Decisions and why

| Change | Why | Effect |
|---|---|---|
| DC removal in the converter | One pass over the data; FIFO keeps clean I/Q for later use | Fixed an overlap write-offset bug on the way; 3325 → 3332 messages on the SDRplay recording |
| Always decode 112 bits | CRC correction can fix the DF bits, so length is only known after scoring | No lost long frames |
| Skip the metric divide below threshold (superseded: the 8-bit log metric has no divide) | `floor(n/d) ≥ T ⇔ n ≥ T·d`; peaks below T are never used | ~1570 → ~1490 ms (SDRplay recording) |
| AVX2 divide/sqrt corrected both ways (now only the converter's mean-level square root is left; the 8-bit log removed the others) | `-ffast-math` lets the compiler use reciprocal estimates; results were one too small on real Intel hardware | Validation passes on Intel |
| Fine arrival time | Bit quality is flat over ~3 ticks on a clean pulse, so `pos` was biased (0 to +2 ticks by phase) and jittery (σ 0.88) | Exact on synthetic frames; MLAT synced with ~260 receivers live |
| Least-squares coefficients | The windowed sinc had uneven delay and gain across phases | Weak synthetic frames 1205 → 1405 |
| All phases sum to 128 | Less rounding error; still fits the overflow bound | Delay error 0.046 → 0.028 samples |
| `DATA_THRES` 80 → 65 | More messages, no extra CPU or junk tracks | +0.2% messages. Still 65 with the capped log quality, which is scaled to the old contrast; there 50, 65 and 80 give 104,924, 104,915 and 104,779 messages (RTL-SDR, 60 s) and the same 11,920 (SDRplay) |
| `UPSAMPLE_SHIFT` 5 → 4 | One more bit for weak signals; power still < 2³¹ | Results unchanged within noise |
| Mirror-symmetric upsampler (NEON, AVX2) | Phase 4−c is phase c reversed, so pairs share sums `x[k] ± x[7−k]`: half the multiplies | `upsample12` 2.48 → 1.54 ms per call |
| `decode_bits` starch kernel | Called 5 times per candidate, ~2M candidates a minute | 82 → 26 ns per call; demod 4.8 → 4.3 s |
| Adaptive gain fed | It was not called at all by the new demodulator | Same gain decisions as stock |
| Signal power capped at full scale | Clipped frames reported up to +1.1 dBFS | RSSI matches stock (mean +0.01 dB, σ 0.2 dB) |
| ARMv7 NEON: no moves out of NEON registers in the hot paths | The in-order Cortex-A53 stalls on them. `upsample12` stored each output straight to its phase row (single-lane stores, row pointers stepping on; the pattern repeats every 24 windows) instead of a scalar 5-way interleave through a buffer (since replaced by the partial transpose, below); `coherent_decode` uses the AArch64 chip method (I/Q interleaved, chip pairs folded) instead of two horizontal sums per chip | Pi 3B: `upsample12` 11.42 → 10.27 ms, `coherent_decode` 11.8 → 10.1 µs (5.5 → 4.7 at the gate); with the next row, a 20 s decode 11.95 → 11.41 s (−4.5%), same messages and aircraft. AArch64 unchanged |
| `locate_preamble` NEON: threshold test in 16-bit lanes (superseded: the 8-bit log metric has no divide, so no pre-check) | minp·(256 − T) ≥ T·(gap + 1) with the gap summed in saturating 16-bit adds can only pass wrongly, never reject wrongly; the few blocks that pass get the exact 32-bit metric, so the output is still bit-exact | At threshold 50: Pi 5 1.14 → 1.03 ms, Pi 3B 10.69 → 10.13 ms; threshold 0 (everything passes) is slower, 1.58 → 2.37 ms |
| Preamble metric without the factor 2 on `minp` (`PREAMBLE_KK`), `PEAMBLE_THRES` 50 → 28 (superseded by the 8-bit log metric, threshold 86) | Both test the same ratio `minp / (gap + 1)`, so only the threshold changes (50 ≙ 27.7); one multiply less | Candidates −7% (RTL-SDR), −9% (SDRplay); messages −0.1%, +0.07%, same aircraft; CPU −2.9%, −3.1% on the Pi 5. Threshold 24 gave the SDRplay a 26th aircraft |
| Upsampler square root within ±1 (no exact fix-ups; superseded: the 8-bit log has no square root and is exact) | Only the benchmark needed exact magnitudes | `upsample12` −10.7% on the Pi 3B (ARMv7), −6% on the Pi 5; CPU −2.8% (RTL-SDR), −3.0% (SDRplay) on the Pi 5; same aircraft and positions, messages ±1 |
| Demodulate in 32768-sample pieces (`DEMOD_BLOCK`): smaller buffers for L2, inside the demodulator | The 12 MHz data for a whole FIFO buffer was 1.3 MB; a piece's fits in L2 between upsample, preamble search and decodes. Done inside `demodulate2400x()`, as if the FIFO buffers were 32768 samples, because the FIFO itself can't shrink: the RTL-SDR, HackRF, LimeSDR and bladeRF drivers put each transfer (up to 131072 samples) into one buffer and drop what doesn't fit, and the stock demodulator would change too | Output byte-identical; CPU −4.4% (RTL-SDR), −8.4% (SDRplay) on the Pi 5 |
| 8-bit log power instead of the magnitude (`upsample12`, `locate_preamble`, `decode_bits`) | The square root (ARMv7 has no vector one) and the metric divide were the costliest steps; in logs the metric ratio is a difference. The float of P + 1 gives the log in 4 instructions; bytes halve the 12 MHz data and put 16 candidates in a NEON register. The pre-check and exact metric path go: every position gets the whole metric. Quality is capped at 32 steps per bit, or noise chips (the log of noise has a long tail) inflate weak frames' quality: twice the coherent attempts on the RTL-SDR, 8× on the SDRplay, and 3% fewer weak synthetic frames. `fineTiming` correlates magnitudes from a table, or weak frames' timestamps were ±1 tick in 4% of cases | Pi 5 demodulation −22% / −20% / −29% (RTL-SDR a / b / SDRplay 5 MHz), messages +0.6% / +0.7% / +0.1%, aircraft with 3+ messages 140 / 141 / 19 (dev 141 / 141 / 19); Pi 3B 20 s decode 11.75 → 8.96 s (−24%), messages +0.7%, aircraft 130 = 130; MLAT timestamps on synthetic frames identical to dev. Pi 3B kernels: `upsample12` 10.29 → 8.43 ms, `locate_preamble` 10.14 → 5.21 ms (any threshold), `decode_bits` 302 → 245 ns |
| ARMv7 `upsample12`: partial transpose instead of single-byte stores | With byte outputs, `vtrn.8` and `vtrn.16` on the phase registers give `[c0 c1 c2 c3]` of windows k and k + 4; `ph[4]` shifted by 8k (as 32-bit lanes) and `vtrn.32` add c4, giving each window's 5 outputs in one register (11 shuffles per 8 windows). Stored 8 bytes at a time at `5w`, overlapping, they are in tick order, and the AArch64 `vld3q_u16` split into the 6 rows is shared, replacing 40 lane stores per 8 windows. A 3-way split between registers doesn't exist (`vuzp` is 2-way), so the 248-byte stack buffer stays; it lives in L1 | Pi 3B `upsample12` 8.46 → 7.18 ms (−15%), 20 s decode 8.97 → 8.47 s (−5.6%), identical output; AArch64 unchanged |
| Coherent frequency search over ±250 kHz, in two stages | Most aircraft the coherent pass gave up on have a steady carrier 100–240 kHz off (transponders are allowed ±1 MHz); 8-bit block sums cancel at those offsets, so more bins alone can't reach them. Stage 1: 2-bit blocks, 16 wide bins 31.25 kHz apart for each 28-bit piece; stage 2: the 16 central fine bins (±30 kHz) around each piece's peak, the stronger kept. The wide bin goes into the slicing phasors, so chips are never de-rotated; regrouping is vectorised through transposed tables | Same as an exact float search over ±248 kHz. Messages +2.7% / +4.0% / +4.6% (RTL-SDR a / b, SDRplay 5 MHz), coherent rescues +26..45%, aircraft +5 / +3 / 0; demodulation CPU +7.0% / +8.6% / +2.3% on the Pi 5, +5.8% on the Pi 3B; weak synthetic frames (random ±100 kHz offsets) 1,592 → 1,710 |
| `coherent_decode` NEON rewritten for the in-order Cortex-A53 | Profiled per stage on the Pi 3B: the chip interpolation was over a third of the time, one dependent chain per chip (load, 4 multiply-accumulates, add) with a divide by 5 for its address; the searches broadcast each block before multiplying. Now the searches multiply by a register lane (`vqrdmulh` is symmetric, so the same products); chips are worked by tap set (6k mod 5 = k mod 5), coefficients in registers, 4 chips interleaved, their windows from 7 loads and `vext`, the max in the same loop; the int16 conversion narrows and transposes with `vtrn` instead of `vld4`, with only the lower clamp (nothing exceeds 2^13 − 1 after the shift). Tried and dropped: both bin vectors per pass (ARMv7 runs out of q registers: stage 1 870 → 1,260 ns), a stage-1 gate (ρ₁ separates noise poorly with 14 blocks: at no loss it keeps 88% of RTL-SDR attempts) | Bit-exact (`-V` on AArch64 and ARMv7; decoder output byte-identical). Pi 3B 12.15 → 10.81 µs full (−11%), 5.73 → 5.01 µs at the gate; Pi 5 2.23 → 2.08 µs (−7%). The full-capture CPU tables were measured before this change |
| Fall back to the stock demodulator while DEMOD_2400X is behind (live input) | When the CPU can't keep up the FIFO fills and the driver drops whole buffers, so every message in them is lost; stock decodes most of them for a third to a half of the CPU (Pi 3B 8.6 s against 24.0 s, Pi 5 2.1 against 3.8). From about 2/3 of the FIFO queued until it is down to 1/3, each buffer goes to stock, on its magnitudes (`getMagnitude()`, shared with Mode A/C and adaptive gain, made once per buffer); a switch is a gap (`MAGBUF_DISCONTINUOUS`) for the demodulator taking over, and is logged. How far behind comes from the buffer's age (its sample start time to now, less its length, in buffer lengths), so `fifo.c` is unchanged. 16 FIFO buffers instead of 12 for more slack (stock keeps 12). Not for `--ifile`: a file is read only as fast as it is demodulated, so its FIFO is always full but never drops, and tests must not fall back (`-DDEMOD_FALLBACK_TEST` allows it) | File output byte-identical to dev (default, `--modeac`, `--no-demod2400x`). Throttled 20 s on a Pi 3B core: alone, or shared with one busy loop, no fallback and the same frames; shared with two, 25% of samples on stock, 9 switches, frames −6.4%. Live on the Pi 4: no switches, 24% of a core |

### Overflow bound

Input is 12-bit (|x| ≤ 2048). Each filter output is at most
`2048·UPSAMPLE_ABS_SUM >> UPSAMPLE_SHIFT`, and the power I² + Q² must stay
below 2³¹ for signed 32-bit arithmetic, and P + 1 must stay a positive int32 for
AVX2's signed float conversion. With
`UPSAMPLE_ABS_SUM` 244 (the largest Σ|tap|, printed by `sim/ls_coeffs.py`) and
shift 4, the worst case is 1.95·10⁹. A `_Static_assert` in
`demod2400x_coeffs.h` enforces it, and the benchmark runs worst-case input.

### Bit-exactness

Generic is the reference; NEON and AVX2 must match it exactly, which
`starch-benchmark -V` checks. That means exact methods only:

* Log power in the upsampler: the integer to float conversion rounds to
  nearest in C, NEON (`vcvtq_f32_u32`) and AVX2 (`_mm256_cvtepi32_ps`) alike,
  so all versions give the same bytes. The linear mantissa is at most 0.69 of
  a step below `8·log2`; exact floor-log versions decoded no better and cost
  about as much as the square root they replaced (see "Tried and dropped").
* The preamble metric is integer sums, differences and saturations, with no
  divide. The quality is an integer sum and one exact integer division per
  call (by the number of bytes).
* Square root, only in the input converter's mean level (`mean_level`, used
  by the stock Mode A/C demodulator's noise estimate): a float square root
  (on ARMv7 the `vrsqrte` estimate and one Newton step), then integer checks
  in both directions.
* The mirror trick is exact integer arithmetic. Generic keeps the direct form.

## Tried and dropped

* **Exact floor-log mantissa** for the 8-bit log (`floor(8·log2(1 + m/32))`):
  as a `vtbl` on 5 or 4 mantissa bits, an exact quadratic
  (`(4m + ((m·(16 − m)) >> 4) + 1) >> 3`) or a 32-bit mask of 0/1
  corrections (`0x08CCCC88`, `o = (m + 2c) >> 2`); each cost about what
  dropping the square root saved (`upsample12` 1.50–1.56 ms against 1.53 ms
  with the square root, Pi 5), and decoding was the same as with the linear
  mantissa (within 45 messages a minute).
* **16-bit power for the log** (`P = SAT65535(I² + Q²) + 1`, 4-bit exponent):
  48 dB is too narrow for one fixed shift. The RTL-SDR needed it high
  (strong frames saturate), the SDRplay, 28 dB quieter, low (weak frames
  lose their resolution). The 32-bit power with a 5-bit exponent covers both.
* **Log quality without a cap**: see the 8-bit log row in "Decisions and why".
* **2.4 MHz pre-detector gate** (skip upsampling where nothing is
  happening): real frames occur at 0.75–1.25 times the mean magnitude, so a
  gate cheap enough to help misses frames.
* **Sharpened upsampler** (`coeffs.py` parameter `a`): amplifies noise more
  than it sharpens pulses; weak frames 1205 → 616 at `a = 0.1`.
* **Lower `PEAMBLE_THRES`** (old scale, with the factor 2): 50 → 40 gives +1.2% messages for +64% CPU;
  at 30, false aircraft appear.
* **Upsampling only around detected preambles** (with 16-bit magnitudes): the 6-phase metric needs
  the full 12 MHz data, and a 2.4 MHz four-pulse pre-test passes 42% of
  positions (95% of 64-sample blocks); upsampling on demand for the decodes
  alone is break-even.
* **Other preamble metrics** (fewer candidates for the same messages, to
  save CPU), tried on the earlier ratio metric, before the 8-bit log. Tested
  on a 60 s RTL-SDR and a 60 s SDRplay recording:
  * Noise-floor bias in the denominator (25th percentile of the buffer):
    +0.4% messages at a third fewer candidates on both, but needs a floor
    estimate and tuning constants.
  * Sum of the 4 pulses instead of the smallest, or the second smallest:
    fewer messages at any threshold.
  * Gap sum less its largest 1 or 2 chips (stray pulses from other replies
    only add energy): up to +3.3% messages on the SDRplay, none on the
    RTL-SDR, and the detector costs ~30% more. Fewer candidates save little
    (~0.4 µs each), so no CPU gain without losing messages.
* **ARMv7 `decode_bits` with the bytes reduced together** (8 bytes narrowed
  to 8-bit lanes and summed with `vpadd`, one store): faster for long runs
  but 303 → 320 ns for the 7 bytes actually decoded.
* **ARMv7 `vtbl4` for the upsampler's 5-way interleave**: only 0.2 ms of
  the 1.37 ms the scalar interleave costs (a 4-register `vtbl` is about 8
  cycles on the Cortex-A53); storing straight to the rows replaced it, and
  with byte outputs the partial transpose replaced that.
* **Soft-decision bit flipping** (flip the weakest bits when the CRC fails):
  `--fix` already repairs any single bit, so this only helps 2-bit errors,
  at a false-decode risk. Not pursued.

## Testing

* `make test`: `cprtests` and `starch-benchmark -V`. Each kernel is checked
  against generic, including full-range worst-case input for `upsample12`
  and a 64-bit check of the generic upsampler.
* Cross-builds: ARMv7 (`ARCH=armv7l CC=arm-linux-gnueabihf-gcc
  LDFLAGS=-static`, runs natively on AArch64) and x86
  (`ARCH=x86_64 CC=x86_64-linux-gnu-gcc`, run with `qemu-x86_64 -cpu max`,
  correctness only; QEMU computes reciprocal estimates exactly, so it can't
  reproduce `-ffast-math` problems).
* Real data: stock and DEMOD_2400X on the same capture, compared message by
  message (contents, RSSI, timestamps).
* `--no-demod2400x` against upstream `dev`'s stock build: identical messages
  and 12 MHz timestamps (`--raw --mlat`) and identical statistics apart from
  the wall-clock CPR counts, for UC8, SC16 and SC16Q11 input and with
  `--modeac`.
* Sanitizers: a `-fsanitize=address,undefined` build decodes UC8, SC16 and
  SC16Q11 input in every mode (default, `--no-coherent`, `--no-demod2400x`,
  `--modeac`, adaptive gain) and runs `starch-benchmark -V` with no
  AddressSanitizer reports. UBSan reports only upstream code: `1 << df` with
  `df` 31 in `mode_s.c` and `demod_2400.c` (an `int` shift; harmless in
  practice).
* gcc and clang builds give byte-identical decoder output, timestamps
  included.
* Synthetic data: `sim/gen_timing.c`. Frames need a carrier offset: at
  exactly 0 Hz the DC filter removes part of the signal.
* Adaptive gain with `--ifile` needs a gain table; for testing, a fake R820T
  table was patched into a scratch copy of `sdr.c` (not committed).

## For reviewers

### Where to start

* `demod_2400x.c`: the demodulator (buffer handling, peak search, scoring,
  fine timing, signal level) and the hook for the coherent pass.
* `dsp/impl/demod2400x.generic.c`: the plain C reference of every kernel.
  The NEON and AVX2 files must give exactly the same results.
* `demod_2400x_coherent.c/.h` and `dsp/helpers/demod2400x_coherent.h`: the
  coherent pass, described in [COHERENT-demod2400x.md](COHERENT-demod2400x.md).

### Changes to existing upstream files

Everything is inside `#ifdef DEMOD_2400X` (or `DEMOD_2400X_COHERENT`) unless
stated.

| File | Change | Why |
|---|---|---|
| `Makefile` | Adds `-DDEMOD_2400X -DUSE_MAGNITUDE -DDEMOD_2400X_COHERENT`, the new objects, default version `11.2x` | Builds the new demodulator in |
| `dump1090.c` | Options `--no-demod2400x` and `--no-coherent` (both on by default); `fifo_create()` with I/Q (16 buffers) or magnitudes (12, as upstream); `demodulate2400x()` or `demodulate2400()`; `demodBehind()`: the stock demodulator on the I/Q buffer's magnitudes while DEMOD_2400X is behind with live input | Chooses the demodulator when running; falls back instead of dropping buffers when the CPU can't keep up |
| `dump1090.h` | `Modes.demod_2400x`, `Modes.coherent`; `MODES_MAG_BUFFERS_2400X` (16); includes `demod_2400x.h` | |
| `fifo.c/h` | Not under `#ifdef`: `fifo_create()` takes `dtype_iq`, and the buffers and overlap copy are twice as large when it is 1; `magbuf_sample()` addresses sample n (`fifo_values_per_sample` is 1 for magnitudes, as before) | The FIFO carries interleaved I/Q for DEMOD_2400X, so it (and the coherent pass) can use the phase |
| `sdr_*.c` | Not under `#ifdef`, one line each: write at `magbuf_sample(outbuf, overlap)` (the same address as before with magnitudes) | Same, for every SDR driver and `--ifile` |
| `convert.c` | I/Q converters (`convert_iq_*`, calling the `iqdata_conv_*` kernels: 12-bit signed I/Q with DC removed) beside the stock ones; `init_converter()` picks by `Modes.demod_2400x` | DC is removed once, in the converter, so the FIFO holds clean I/Q |
| `demod_2400.c` | `getMagnitude()`: the magnitudes of the I/Q buffer, made once per buffer in one array (the main loop sets `demod_magnitude` to NULL before each buffer); used by `demodulate2400AC()`, by `demodulate2400iq()` (the stock demodulator on an I/Q buffer, for the fallback) and by DEMOD_2400X's adaptive gain | Mode A/C and the fallback use the stock demodulator, which expects magnitudes; one conversion serves all three |
| `stats.c/h`, `net_io.c` | Three coherent counters; `--stats` lines and `"coherent":{...}` in `stats.json` `local`, only while the coherent pass runs; `samples_fallback` (samples demodulated by stock while DEMOD_2400X was behind), in `--stats` only | Live monitoring of the coherent pass and the fallback. Consumers that ignore unknown fields are unaffected |
| `dsp/starchgen.py` | Seven new starch functions | The kernels |
| `wisdom.*` | Kernel order for the new functions | starch picks the fastest version |
| `README.md` | A short section pointing at these documents | |

`--no-demod2400x` gives byte-identical output to upstream `dev`'s stock build
(messages and timestamps with `--raw --mlat`, for UC8, SC16 and SC16Q11 input
and with `--modeac`), and so does a build without the defines, which then
doesn't have the two options.

### Generated and derived files

* `dsp/generated/*` is produced by `python3 dsp/starchgen.py .`; review
  `dsp/starchgen.py` and `dsp/impl/*` instead.
* `dsp/helpers/demod2400x_coeffs.h` (upsampler taps) comes from
  `sim/ls_coeffs.py`; `dsp/helpers/demod2400x_coh_tables.h` (phasors) from
  `sim/coh_tables.py`.

### How to check it

* `make test`: CPR tests, and `starch-benchmark -V`, which checks every
  NEON and AVX2 kernel against generic (including worst-case input).
* Decode a recording with stock and with this branch and compare (see
  [Testing](#testing)); `--throttle` for positions and tracks.

### Open points

* DEMOD_2400X and the coherent pass are on by default, so that standard
  installations get the benefit without changing their configuration;
  `--no-demod2400x` gives byte-identical stock output for anyone who needs it,
  and `--no-coherent` drops the second pass. The cost is CPU: on a Pi 3B the
  default uses about 40% of one core for demodulation against 14% for stock
  (see [Raspberry Pi 3B](#raspberry-pi-3b)). A Pi 3B running it needs a
  heatsink: if it throttles to 600 MHz it needs about twice the CPU and may
  fall behind. `vcgencmd get_throttled` shows whether it has throttled (0x0 if
  never). When DEMOD_2400X falls behind with live input it hands buffers to
  the stock demodulator until it catches up, instead of dropping them.
* The default version string is `11.2x`; upstream's is `unknown` (packages
  set it).
* `--dcfilter` is still not supported (as upstream), although the converters
  now remove DC anyway.
* Mode A/C (`--modeac`) with DEMOD_2400X goes through `getMagnitude()`, which
  keeps one static buffer, shared with the fallback and adaptive gain. With `--no-demod2400x` its output is identical to
  stock, and it runs clean under the sanitizers.
* Statics: `getMagnitude()`'s buffer and `demod_magnitude`, `demodBehind()`'s
  state, the coherent statistics, sample-dump
  state and `fineTiming()`'s magnitude table (filled on first use) in
  `demod_2400x.c`, and two diagnostic globals in `demod_2400x_coherent.c`.
  All are used from the demodulator thread only.
* The stock SoapySDR driver fails on an SDRplay RSP1B (the stream doesn't
  start, and the gain runs backwards); fixed separately, on
  `sdrplay-gain-dev`, not here.
* Upstream code shifts `1 << df` with `df` up to 31 in `mode_s.c` and
  `demod_2400.c` (undefined for an `int`, found by UBSan); left as upstream.

## Coherent second pass

Built in with `-DDEMOD_2400X_COHERENT` (in the Makefile) and on by default;
`--no-coherent` switches it off, and the decoder is then byte-for-byte plain
DEMOD_2400X. The method, results and settings are in
[COHERENT-demod2400x.md](COHERENT-demod2400x.md); in short:

* **Why:** the normal decoder compares the power of the two chips of each bit
  and ignores the phase, so all of the noise in both chips counts against the
  decision. Correlating the chips with the carrier keeps only the noise in
  phase with it.
* **When:** only for frames that fail the CRC with a first-half quality of at
  least `COHERENT_GATE` (96), and the kernel stops early if the carrier isn't
  coherent (rho2 below `COHERENT_RHO2`, 0.6): noise, overlapping frames, and
  transmitters whose frequency or phase moves during the burst (see "What
  the carriers show" in the COHERENT doc).
* **How:** the I/Q kept in the FIFO is interpolated to complex chips at the
  position the normal decoder chose; each 56-bit half gets its own frequency
  and phase, estimated from each bit's stronger chip; then the bits are
  sliced against the carrier. The second half is decoded whatever the DF
  says. The frequency search covers ±250 kHz in two stages: 16 wide bins
  31.25 kHz apart on 2-bit blocks, for each 28-bit piece, then 16 fine bins
  4 kHz apart around each piece's peak.
* **Results** (60 s RTL-SDR capture): 84,545 → 96,370 messages (+14%), 139
  → 155 aircraft, for 3.0 → 3.8 s of demodulation on the Pi 5. Of 596,098
  attempts, 151,907 had a coherent carrier and 11,820 were rescued; rescued
  frames are timed like the others.
* **Kernel:** `coherent_decode`, a starch function with generic, NEON and
  AVX2 versions, bit-exact; 2.08 µs per full decode on the Pi 5 (0.96 µs
  when it stops at the gate), 10.8 / 5.0 µs on the Pi 3B, 1.30 / 0.61 µs
  with AVX2 on x86-64.

## Raspberry Pi 3B

A Pi 3 Model B (Rev 1.2, Cortex-A53 at 1.2 GHz) running Volumio, a 32-bit OS,
so the ARMv7 NEON kernels. A static ARMv7 build of the dev branch, decoding
the first 20 s of the Pi 4 capture (`--fix`); the messages are identical to
the Pi 5's.

| Run | Messages | Demodulation per 20 s | One core |
|---|---|---|---|
| `--no-demod2400x` (stock) | 24,345 | 2.9 s | 14% |
| `--no-coherent` | 28,126 | 6.1 s | 31% |
| default (DEMOD_2400X + coherent) | 32,025 | 8.0 s | 40% |

Kernel times on the Pi 3B (ARMv7 NEON) against the Pi 5 (AArch64 NEON):

| Kernel | Pi 3B | Pi 5 |
|---|---|---|
| `upsample12` | 7.17 ms | 1.27 ms |
| `locate_preamble` (any threshold) | 5.26 ms | 0.50 ms |
| `decode_bits` (7 bytes) | 253 ns | 26 ns |
| `coherent_decode` (full / stopped at the gate) | 10.8 / 5.0 µs | 2.08 / 0.96 µs |

So a Pi 3B can run it live, at roughly 45% of one core once USB input and
the rest are added (on the Pi 4 they add about 4 points). The upsampler and
preamble search, over half of it, cost the same whatever the traffic. Earlier
builds (square root, divide, single-byte stores) needed 63% of a core, and at
600 MHz (thermal throttling) could not keep up, so a heatsink is still
advisable. The steps that got it here are in "Decisions and why".

With throttled file input on core 3 (a test build that lets files fall back),
the 20 s of capture a:

| Core 3 shared with | Samples on stock | Switches | Frames (correct CRC) |
|---|---|---|---|
| nothing | 0 | 0 | 30,836 |
| 1 busy loop | 0 | 0 | 30,836 |
| 2 busy loops | 25% | 9 | 28,873 (−6.4%) |

So it keeps up even with half a core, and with a third it loses a few percent
of frames instead of whole buffers.

The static `dump1090` for ARMv7 needs `-lncurses`, which the cross-compiler
doesn't have; ncurses is only used by interactive mode, so the test build
linked a stub library (not committed).

## Gotchas

* `dsp/generated/*.o` don't depend on `dsp/helpers/*.h`. After a header edit,
  run `rm dsp/generated/*.o`, or generic keeps old code and verification
  compares against stale results.
* After adding a starch function or changing a wisdom file, rerun
  `python3 dsp/starchgen.py .`; the wisdom order is compiled into
  `dispatcher.c`.
* After changing the version string, run `make clean`: objects don't depend
  on the Makefile.
* starch finds `STARCH_IMPL` by regex, so implementations can't be generated
  by macros.
* starch doesn't pass a kernel's return value to its verifier, so
  `decode_bits` returns its quality through a pointer.

## Possible next steps

* An early DF check before decoding the second half and scoring.
