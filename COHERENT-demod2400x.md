# DEMOD_2400X coherent second pass

An experimental second pass for frames that the normal DEMOD_2400X decoder
finds but can't decode. It uses the phase of the signal, which the normal
decoder throws away, to recover some of those frames. On the 60 s Pi 4
RTL-SDR capture it adds about 14% more messages (84,545 → 96,370) for about
27% more demodulation CPU.

Branch: `demod-2400x-dev` (developed on `demod-2400x-coherent`). Built in by
`-DDEMOD_2400X_COHERENT` in the Makefile and on by default; `--no-coherent`
switches it off when running, and the decoder is then byte-for-byte plain
DEMOD_2400X.

## Why it can help

A Mode S bit is two half-microsecond **chips**: a pulse in the first chip is
a 1, a pulse in the second is a 0. The normal decoder measures the
**magnitude** (strength) of each chip and picks the stronger one. Magnitude
ignores phase, so all of the noise in each chip counts against the decision.

A modern transponder transmits from a crystal oscillator, so every pulse in a
frame is a piece of the same steady carrier wave. Seen by the receiver, that
carrier has a fixed starting phase plus a slow, steady rotation, because the
aircraft's frequency and the dongle's are never exactly the same (and there
is Doppler). If we know the phase of the carrier at every chip, we can line
each chip up with it and measure only the part of the signal that points the
right way. Noise pointing the other way is ignored, which roughly halves the
noise in the decision. The theory says up to 3 dB; on the marginal frames
that get rescued we measured about 2 dB.

The difficulty is that the phase, the frequency offset and the data are all
unknown. The method below gets the first two without knowing the data.

## How it works

Everything is integer arithmetic (int16 where it fits, int32 otherwise), with
no FFT, no atan and no phase angles: it is a bank of correlators (matched
filters).

Steps 2–6 are done for each 56-bit half of the frame on its own, by the same
function: bits 0–55, then bits 56–111 (step 7).

### 1. When it runs

In `emitFrame()` (`demod_2400x.c`), after the normal decoder has produced
its bits for a candidate frame:

* if the bits pass the CRC, nothing changes;
* if they fail **and** the magnitude quality of the first 56 bits is at
  least `COHERENT_GATE` (96; at most 176), the frame is decoded again
  coherently by `coherentDecode()`;
* if the coherent bits pass the CRC, they replace the magnitude bits and the
  frame continues through the normal path (timestamp, signal level, output).
  Otherwise the frame is rejected as before.

The quality gate keeps the cost down: frames that are mostly noise are not
worth trying.

### 2. Complex chips

The input is the 2.4 MHz I/Q stream (12-bit, DC removed) that the normal
decoder also uses. The kernel interpolates it to the centre of each chip with
the same 5-phase polyphase filter as `upsample12`, at the 12 MHz position the
normal decoder chose (so the timing is the one the magnitude decoder found
best). This gives two complex numbers per bit, `a` (first chip) and `b`
(second chip), but keeps I and Q instead of taking the magnitude.

The chips are scaled with one shift for the half so the largest fits in 13
bits, then stored as int16.

### 3. A carrier sample from every bit, whatever the data

In each bit exactly one of the two chips has the pulse. The kernel takes the
chip with the larger magnitude (`|a|² ≥ |b|²`) as that bit's **carrier
sample**. This gives 56 carrier samples, one per microsecond, over the
half, without knowing the data.

The choice is a decision, so on a weak bit noise sometimes wins and the
sample is just noise. The alternative, the pair sum `a + b`, always contains
the pulse but also the noise of the empty chip, twice the noise power.
Taking the larger chip is better overall: on the 60 s capture it rescues 14%
more frames than the pair sum (8,608 against 7,530, measured before the two
halves were estimated separately).

### 4. Find the frequency offset

Transponders are allowed ±1 MHz, and many aircraft are 100–240 kHz off 1090 MHz
(see "What the carriers show"), so the search covers **±250 kHz**, in two
stages.

**Stage 1, coarse.** The 56 carrier samples are added in **blocks of 2 bits**
(2 µs), giving 28 block sums. Blocks 2 µs apart can tell frequencies apart up
to ±250 kHz; longer blocks would cancel at large offsets (an 8 µs block turns a
full circle at 125 kHz). Each half of the half (bits 0–27 and 28–55, 14
blocks each) is correlated against **16 wide bins**, 31.25 kHz apart, at
±15.6, ±46.9, … ±234.4 kHz: each block is rotated back by the phase that bin
would have caused by then, the 14 results are added, and the bin where they
add up best (largest power) is that piece's peak. Two pieces give two chances
when the carrier moves during the burst. 16 bins fill two 8-lane vectors.

**Stage 2, fine.** For each peak (one if both pieces agree), the 28 blocks are
rotated back by that bin and added in fours into **7 blocks of 8 bits**. The
remaining offset is within about ±16 kHz, so only the **16 central fine bins**
are searched, 4 kHz apart at ±2, ±6, … ±30 kHz: each block is rotated back by
the phase that fine bin would have caused, the 7 results added, and the
largest power wins. The peak whose fine search finds the stronger carrier is
kept. A parabola through the best fine bin and its two neighbours refines the
frequency to 1/8 of a fine bin (0.5 kHz). The frequency is the wide bin plus
the fine one; both go into the phasors used for slicing, so the chips are
never rotated separately.

No bin, wide or fine, is at 0 Hz: any DC the SDR leaves in the I/Q correlates
perfectly at 0 Hz, and with no bin there it falls halfway between two bins
instead of on one.

The phasors for the 16 wide bins (also transposed, for rotating all 28 blocks
by one bin at once), the 32 fine bins and the 249 fine frequencies are
precomputed tables (`dsp/helpers/demod2400x_coh_tables.h`, generated by
`sim/coh_tables.py`).

### 5. Find the carrier phase and check it's coherent

With the frequency known, every chip can be rotated back to where it would be
at bit 0: a phasor `P[i]` per bit (and `Q[i]` for the second chip, half a bit
later) is built by repeated multiplication from the fine-frequency table.
Summing the rotated carrier samples of the 56 bits (`a` rotated by `P[i]`,
or `b` by `Q[i]`, whichever was chosen) gives the **channel** `h`: one
complex number with the carrier's amplitude and phase.

The same sum also tells us whether the frame is coherent at all:

    rho2 = |h|² / (56 · Σ |rotated carrier sample|²)

`rho2` is 1 for a perfectly steady carrier and drops towards 0 for noise,
for a drifting or chirping transmitter, or for frames with phase modulation
on the pulses. If the first half is below `COHERENT_RHO2` (614/1024 = 0.6)
the kernel stops here and the frame is left alone.

### 6. Slice the bits

For each of the 56 bits of the half:

* rotates `a` by `P[i]` and `b` by `Q[i]`;
* projects each onto `h` (the real part of `chip × conj(h)`), giving `y_a`
  and `y_b`: how much of each chip lines up with the carrier;
* bit = 1 if `y_a > y_b`.

It also computes a quality for the half, like the magnitude one:
`256 · Σ|y_a − y_b| / Σ(|y_a| + |y_b|)`. The second-half quality is used,
as in the normal path, to reject long frames without a full CRC whose second
half is poor.

### 7. The second half, on its own

If the first half passed the rho2 gate, bits 56–111 go through steps 2–6
again with nothing carried over: their own scaling, frequency, phase and
channel. It is always sliced; there is no gate on it.

* **Why separately:** holding the first half's frequency and phase for all 112
  bits extrapolates any small frequency error, and the phase error grows
  along the frame. In the Python model (`sim/coh_seg.py`), on frames whose
  bits are known, that gave 2 or more wrong bits in the second half of 444
  frames out of about 16,600; estimating each half on its own cut it to 8.
  On the 60 s capture it rescues 4.6% more frames (8,608 → 9,003), all of
  them long ones, for about 6% more time in the coherent pass. A short frame
  never depends on bits after it.
* **Whatever the DF says:** the second half is decoded even if the first bit
  says the frame is short, as the magnitude decoder does. Scoring can correct
  the DF bits, and every short DF is one bit from a long one (DF0/4/5/11
  against DF16/20/21/24).

`COHERENT_MIX` (off by default) adds one more try: if the coherent frame
fails the CRC, it is tried again with the magnitude decoder's second half,
then with the magnitude first half. On the 60 s capture that rescues 75 more
frames (+0.8%) for about 1% more coherent-pass time.

## Results

60 s Pi 4 RTL-SDR capture (FlightAware Pro Stick Plus), decoded on a
Raspberry Pi 5 (comparisons with stock dump1090 are in
[README-demod2400x.md](README-demod2400x.md#results)):

| | Without coherent | With coherent |
|---|---|---|
| Messages | 84,545 | 96,370 (+14%) |
| DF17 (ADS-B) | 11,830 | 13,823 (+17%) |
| Global CPR positions | 3,977 | 4,692 (+18%) |
| Messages below −24 dBFS | 5,028 | 8,575 (+71%) |
| Aircraft | 139 | 155 |
| Demodulation CPU | 3.0 s | 3.8 s |

The coherent pass itself takes about 0.95 s for the 60 s of signal. Widening the
search to ±250 kHz added 2.7–4.6% messages on the three
test captures (RTL-SDR a and b, SDRplay at 5 MHz) for 2–9% more demodulation
CPU, the same as an exact float search over ±248 kHz.

Coherent attempts by the magnitude quality of the failed frame:

| Magnitude quality | Attempts | Coherent (rho2 ≥ 0.6) | Rescued |
|---|---|---|---|
| 96–111 | 360,934 | 38,260 | 1,885 |
| 112–127 | 122,946 | 42,218 | 3,356 |
| 128–143 | 61,507 | 32,795 | 2,984 |
| 144–159 | 35,063 | 24,628 | 2,250 |
| 160–175 | 15,283 | 13,645 | 1,304 |
| 176 (the largest) | 365 | 361 | 41 |
| **Total** | **596,098** | **151,907** | **11,820** |

The quality is the mean log-power contrast of the bits, capped at 32 steps per
bit and scaled to about the old 0–256 magnitude contrast, so it can't exceed 176.

Of the 11,820 rescued frames, 11,748 are from aircraft already known from
other messages; 72 are from addresses not seen otherwise. As in the stock
decoder, a message from an unknown address needs confirmation before it
creates an aircraft, so the risk of false aircraft is the same as before.

Rescued frames are timed exactly like the others: on synthetic frames at
known times, every rescued frame has the correct MLAT timestamp. Live on a
Raspberry Pi 4, mlat-client stays synchronised with about 245–260 nearby
receivers.

Most attempts are not coherent: they are noise, overlapping frames, or
aircraft whose transmitters aren't phase-stable.

### Live counts

`stats.json` (in the `--write-json` directory) has the coherent counts in
each period's `local` section, and `--stats` prints them too:

    "coherent":{"tried":596098,"coherent":151907,"rescued":11820}

* `tried`: frames that failed the CRC and passed the quality gate;
* `coherent`: of those, frames whose carrier passed the rho2 gate;
* `rescued`: of those, frames whose coherent bits passed the CRC. They are
  also counted in `accepted`.

## What the carriers show

The coherent pass needs a steady carrier. With the earlier, narrower search it
gave up on about a quarter of aircraft: of the 122 aircraft with 30 or more
frames in the 60 s RTL-SDR capture, 87 were coherent in over 90% of their frames, 32 in under 10%, and
only 3 in between. To see why, the complex chips of every accepted frame were
dumped (`COHERENT_SAMPLES`) and the carrier followed through each burst: the
phase step from one bit's pulse to the next gives the frequency every
microsecond, unambiguously up to ±500 kHz, far beyond what the pass
then searched. Long frames with a bit SNR above 15 dB, aircraft with at least 15 of
them; "drift" is the frequency over the last third of the burst minus the
first third (about 74 µs apart):

| Aircraft (callsign, registration, type) | Coherent frames | Offset | Spread between frames | Drift in a burst | What it is |
|---|---|---|---|---|---|
| VLG8961, EC-OFZ, A320 | 0% | +237.6 kHz | 2.0 kHz | −3.0 kHz | steady, outside the earlier search |
| SHT9K, G-EUUU, A320 | 0% | +178.0 kHz | 1.4 kHz | −2.5 kHz | steady, outside the earlier search |
| EFW29BA, G-EUUT, A320 | 0% | +155.9 kHz | 1.3 kHz | −2.1 kHz | steady, outside the earlier search |
| EZY81QG, G-EJCC, A320 | 0% | +148.2 kHz | 1.2 kHz | −8.4 kHz | steady, outside the earlier search |
| RYR71LE, EI-EKT, B738 | 0% | +136.3 kHz | 1.2 kHz | −3.0 kHz | steady, outside the earlier search |
| VLG6023, EC-MBS, A320 | 0% | +130.9 kHz | 1.0 kHz | −2.4 kHz | steady, outside the earlier search |
| RYR3TU, SP-RZN, B38M | 0% | +118.2 kHz | 1.1 kHz | −1.2 kHz | steady, outside the earlier search |
| EZY79NW, G-EJCA, A320 | 0% | +117.2 kHz | 14.5 kHz | −0.9 kHz | outside the earlier search, less steady |
| BAW586M, G-EUPG, A319 | 0% | +113.8 kHz | 0.8 kHz | −2.3 kHz | steady, outside the earlier search |
| GVCRM (not in the database) | 0% | −114.8 kHz | 0.6 kHz | +2.3 kHz | steady, outside the earlier search |
| KLC85F, PH-NXG, E295 | 0% | +28.4 kHz | 2.3 kHz | −80.0 kHz | settles during the burst |
| G-DOSC, DA42 | 1% | −22.1 kHz | 0.7 kHz | −0.8 kHz | phase wanders |
| G-HLOB, C172 | 1% | −15.2 kHz | 0.9 kHz | −6.4 kHz | phase wanders |
| LGL4732, LX-LEA, E295 | 100% | +2.6 kHz | 1.4 kHz | +23.1 kHz | chirps, still coherent |
| GULFD, G-ULFD, GA7C | 100% | −4.3 kHz | 0.9 kHz | +21.1 kHz | chirps, still coherent |
| EZY85EC, G-UZEF, A20N | 100% | −0.7 kHz | 0.3 kHz | −0.1 kHz | steady (typical) |

Callsigns are from the aircraft's own identification messages in the capture;
registration and type from the aircraft database that ships with dump1090.

* **Most "non-coherent" aircraft are steady, just far off frequency.** Ten of
  the 13 sit 114 to 238 kHz from 1090 MHz (one below). Nine of them are as
  steady as any coherent aircraft, about 1 kHz from frame to frame, and
  inside a burst VLG6023's phase stays within about 18° of a straight line
  (6° for EZY85EC, a typical coherent aircraft). They are mostly A320-family
  and 737 airliners. Transponders are allowed ±1 MHz, so these are within
  spec; they were simply outside the range then searched, and the ±250 kHz
  search now covers them.
* **One transmitter settles during the burst.** KLC85F (PH-NXG, an Embraer
  E195-E2) starts each frame at about +100 kHz for the first 30 µs or so,
  then drops to about −13 kHz and wanders by ±20 kHz: the oscillator is still
  settling after the transmitter keys on. Five frames, frequency every 16 µs:

      +99.3 +102.5 −12.9  +3.0  −2.8 +22.2  kHz
     +100.9  +96.7 −13.8  +9.4 −12.1 +12.6
      +97.6  +99.5 −12.5  −0.4  +1.4  +5.2
     +104.9 +101.2 −16.0  +1.6  −7.0 +17.4
     +100.1  +97.8 −13.3  +4.3  −8.1 +21.0

* **Two have a wandering phase.** Both are light aircraft, G-DOSC (a DA42)
  and G-HLOB (a C172). Their average frequency is steady and well inside the
  search, but their phase drifts slowly through the burst (73–86° spread, with
  almost no sudden jumps): phase noise, not a chirp or a frequency step.
* **Some coherent aircraft chirp.** LGL4732 (LX-LEA, another E195-E2) ramps
  by about 30 kHz across every burst (−17, −5, +4, −1, +9, +12 kHz every 16 µs
  in a typical frame), and GULFD (G-ULFD, a Gulfstream G700) similarly. Each
  56-bit half is estimated on its own, so they still decode coherently. Both
  E195-E2s in the capture move in frequency during the burst.
* **Every transmitter has its own signature.** The steady aircraft mostly hold
  their offset to 0.3–0.7 kHz from frame to frame (up to 1.4 kHz), anywhere
  from −20 to +45 kHz, and chirping or settling transmitters repeat their
  pattern in every frame. Offsets cluster by fleet: of the twelve aircraft near −18 kHz, seven are
  easyJet A319/A320s (G-EZ.., HB-JZY; the others a TUI 737, a Citation, a
  United and a Delta 767, and LOG555), and the six near +28 kHz are Jet2 and
  TUI 737s and an Aer Lingus A320, presumably the same transponder fit. Two
  easyJet A320s registered G-EJ.. are instead in the far-off group, at +117 and
  +148 kHz. The receiver's own error is small: −0.5 kHz median over all
  frames.

This led to the ±250 kHz two-stage search (step 4), which brings the steady but
far-off transmitters into the coherent pass: 2.7–4.6% more messages.

## Limitations

* **Some aircraft are not coherent:** carriers that settle or chirp strongly
  during the burst, or whose phase wanders (see "What the carriers show").
  They fail the rho2 gate and gain nothing (they're decoded exactly as
  before).
* **Frames with phase modulation on the pulses** (the newer 8-PSK overlay
  that some transponders add on top of the normal pulses) can never be
  coherent in this sense, and also fail the gate.
* Frequency offsets beyond about ±250 kHz are not searched.
* The gain is largest on frames just below the CRC threshold; strong frames
  already decode, and very weak ones fail the quality gate.

## Speed

The kernel is a starch function, `coherent_decode`, with three
implementations that give bit-identical results:

| | Full decode | Stopped at the rho2 gate |
|---|---|---|
| Generic C (Pi 5) | 5.0 µs | 1.94 µs |
| NEON, aarch64 (Pi 5) | 2.08 µs | 0.96 µs |
| NEON, ARMv7 (Pi 5, 32-bit) | 2.46 µs | 1.12 µs |
| NEON, ARMv7 (Pi 3B, 32-bit) | 10.8 µs | 5.0 µs |
| Generic C (x86-64) | 4.8 µs | 2.0 µs |
| AVX2 (x86-64) | 1.30 µs | 0.61 µs |

With the earlier, narrower search the same kernel took 1.80 / 0.81 µs (Pi 5 NEON),
3.40 / 1.53 µs (Pi 4), 10.1 / 4.7 µs (Pi 3B), and on x86-64 3.6 / 1.5 µs
(generic) and 1.04 / 0.48 µs (AVX2); the Pi 4 hasn't been re-timed. The NEON
figures are after the A53-oriented rewrite of the NEON kernel (lane multiplies in
the searches, chips by tap set; see NOTES), which took the Pi 3B from 12.15 /
5.73 µs and the Pi 5 from about 2.23 µs.

Most frames stop at the gate, so the cost is dominated by the first half:
interpolating its 112 complex chips (the largest part: over a third on the Pi
3B), converting them to
int16, the frequency search and the correlation. A full decode does all of
that twice.

`starch-benchmark coherent_decode` times and checks it; `-V` checks every
implementation against generic.

## Files

| File | What it holds |
|---|---|
| `demod_2400x.c` | The hook in `emitFrame()`, statistics, the diagnostic and the sample dump (all inside `#ifdef DEMOD_2400X_COHERENT`) |
| `demod_2400x_coherent.c/.h` | `coherentDecode()`: bounds check, calls the kernel; the settings |
| `dsp/helpers/demod2400x_coherent.h` | The method, step by step, and the scalar helpers shared by all implementations |
| `dsp/helpers/demod2400x_coh_tables.h` | Phasor tables (generated) |
| `dsp/impl/demod2400x.generic.c` | Reference implementation: read this one first |
| `dsp/impl/demod2400x.neon.c`, `.avx2.c` | SIMD versions, bit-exact with generic |
| `dsp/impl/demod2400x.benchmark.c` | Benchmark and verification |

## Settings

In `demod_2400x_coherent.h`. Each can be changed for a test build without
editing the file, e.g. `make CPPFLAGS=-DCOHERENT_STATS=1` (use `CPPFLAGS`,
not `CFLAGS`, which would replace `-O3`).

| Define | Default | Meaning |
|---|---|---|
| `COHERENT_GATE` | 96 | Minimum magnitude quality (first 56 bits, at most 176) to try |
| `COHERENT_RHO2` | 614 | Minimum carrier coherence of the first half, in 1/1024 (0.6) |
| `COHERENT_MIX` | 0 | 1: if the coherent frame fails, try it with each half from the magnitude bits |
| `COHERENT_STATS` | 0 | 1: print the statistics above at exit |
| `COHERENT_DIAG` | 0 | 1: also decode frames that already passed, and count bit errors of both decoders |
| `COHERENT_DUMP` | 0 | 1 (with `COHERENT_DIAG`): one line per frame with frequency and errors |
| `COHERENT_SAMPLES` | `NULL` | File name for the chip-sample dump (for the `sim/` scripts) |
| `COHERENT_SAMPLES_GATE` | 257 | Also dump failed frames of at least this quality (257: none) |

Example: decode a recording and print the statistics.

```
make CPPFLAGS=-DCOHERENT_STATS=1
./dump1090 --ifile capture.uc8 --iformat UC8 --quiet --stats
```

## Development tools (`sim/`)

The method was developed offline in Python on dumps of the complex chips,
then ported to integer C.

* `coh_samples.py`: reader for the `COHERENT_SAMPLES` dump. Build with e.g.
  `make CPPFLAGS='-DCOHERENT_SAMPLES=\"chips.bin\" -DCOHERENT_SAMPLES_GATE=96u'`.
  Keep test runs short: the dump is about 2 KB per frame.
* `coh_acq.py chips.bin [records]`: the floating-point model of the first
  version (pair sums), with the variants compared during development (window
  lengths, FFT, block sizes).
* `coh_larger.py chips.bin [records]`: carrier samples from the larger chip
  (as the kernel does now) against the pair sum `a + b`.
* `coh_seg.py chips.bin [records]`: one estimate for all 112 bits against
  each half (or 28- or 14-bit segment) estimated on its own, and block length
  8 against 4 or 2.
* `coh_snr.py chips.bin [records]`: decision SNR of the magnitude and
  coherent decoders on frames whose bits are known.
* `coh_judge.py`: CRC check used by the scripts to judge a rescue.
* `coh_tables.py`: generates `demod2400x_coh_tables.h`.

## Tried and dropped

* **Tracking the phase through the frame** (decision-directed loop): fails on
  chirping transmitters and adds nothing on crystal ones.
* **FFT instead of the correlator bank**: same answers, no cheaper for 7
  points, and floating point.
* **Blocks of 14 bits** instead of 8: aliases at the larger offsets. Blocks
  of 8 perform like the full 56-bit correlator.
* **Fitting a polynomial to the phase**, and using the second-half quality to
  choose a 112-bit estimation window: both worse.
* **Half-band frequency search**: no better, dropped.
* **Pair sums `a + b` as carrier samples** (the first version): 14% fewer
  rescues than taking the larger chip.
* **Shorter independent segments** (4 × 28 or 8 × 14 bits): fewer errors on
  strong frames, but on the weak frames that need rescuing the estimate is
  too noisy: 4% and 12% fewer rescues than two halves (`coh_seg.py`).
* **33 bins with one at 0 Hz** (the first layout, 3.9 kHz apart): the same
  results within about 0.4% of rescues on the 60 s capture (spacings of 3.9,
  4.0 and 4.03 kHz differ by about as much, with no trend), but a bin at 0 Hz
  is where leftover DC lands, and 33 bins need 5 vectors instead of 4.
* **Blocks of 4 bits** instead of 8 in the frequency search: the same answers
  (+0.5% rescues in the model) for twice the search work.
* **Keeping the magnitude second half when its quality2 is high**: worse at
  every threshold (quality2 ≥ 200: −2 rescues, ≥ 160: −247, ≥ 120: −1,464).
  The coherent second half is better even when the magnitude one looks clean.
