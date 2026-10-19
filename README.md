# dump1090-fa Debian/Raspbian packages

dump1090-fa is a ADS-B, Mode S, and Mode 3A/3C demodulator and decoder that
will receive and decode aircraft transponder messages received via
a directly connected software defined radio, or from data provided over a
network connection.

It is the successor to
[dump1090-mutability](https://github.com/mutability/dump1090) and is
maintained by [FlightAware](http://flightaware.com/).

It can provide a display of locally received aircraft data in a terminal or
via a browser map. Together with [PiAware](http://flightaware.com/adsb/piaware)
it can be used to contribute crowd-sourced flight tracking data to FlightAware.

It is designed to build as a Debian package, but should also be buildable on
many other Linux or Unix-like systems.

## DEMOD_2400X demodulator (this branch)

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

### Results

A 60 s capture from a Raspberry Pi 4 with a FlightAware Pro Stick Plus
(gain 49.6 dB), with an outdoor antenna on a 3 m pole and 12 m of LMR400,
decoded by stock dump1090-fa 11.1 and by this branch with the same options
(`--fix`, receiver position set):

| | Stock | DEMOD_2400X | Change |
|---|---|---|---|
| Usable messages | 73,146 | 84,369 | +15% |
| DF17 (ADS-B) | 10,352 | 11,789 | +14% |
| Global CPR positions | 2,807 | 3,773 | +34% |
| Aircraft | 136 | 141 | +5 |

The gain is concentrated in weak signals. Messages by signal strength:

| RSSI (dBFS) | Stock | DEMOD_2400X | Change |
|---|---|---|---|
| −30 to −27 | 131 | 307 | +134% |
| −27 to −24 | 2,347 | 4,199 | +79% |
| −24 to −21 | 7,111 | 9,409 | +32% |
| −21 to −18 | 9,010 | 10,732 | +19% |
| −18 to −15 | 12,213 | 13,707 | +12% |
| −15 to −10 | 18,226 | 19,924 | +9% |
| −10 to −5 | 9,952 | 10,707 | +8% |
| above −5 | 14,151 | 15,378 | +9% |

Position messages by distance from the receiver:

| Distance | Stock | DEMOD_2400X | Change | Aircraft (stock / new) |
|---|---|---|---|---|
| 0–50 km | 643 | 799 | +24% | 12 / 13 |
| 50–100 km | 1,225 | 1,523 | +24% | 30 / 30 |
| 100–150 km | 746 | 948 | +27% | 33 / 33 |
| 150–200 km | 356 | 473 | +33% | 22 / 22 |
| 200–250 km | 93 | 133 | +43% | 8 / 14 |
| 250–300 km | 11 | 23 | +109% | 2 / 5 |

Weak synthetic frames (2,000 DF17 frames with a random ±100 kHz carrier
offset, decoded out of 2,000): 689 (stock) against 1,406 (DEMOD_2400X).

### Timing (MLAT)

On synthetic frames at known 12 MHz ticks, covering all 5 sub-sample
phases:

| | Stock | DEMOD_2400X |
|---|---|---|
| Bias across phases (clean) | 0 to +2 ticks | none |
| Jitter, noisy frames (σ) | 0.41–0.48 ticks | < 0.2 ticks |

One tick is 83.3 ns. Running live on the Pi 4, mlat-client stays synchronized
with about 260 nearby receivers, as with stock.

### CPU

Demodulation time for the 60 s capture:

| | Stock | DEMOD_2400X |
|---|---|---|
| Raspberry Pi 5 (Cortex-A76) | 2.0 s | 4.3 s |
| Raspberry Pi 4 (Cortex-A72) | 3.9 s | 10.6 s |

The Pi 4 times were measured while the live receiver was also running on
the same Pi, so both are probably slightly high. Live on the Pi 4 this is
about 31% of one core. These results come from
one receiver and a one-minute capture; longer side-by-side runs are in
progress.

## Building under bullseye, buster, or stretch

```bash
$ sudo apt-get install build-essential fakeroot debhelper librtlsdr-dev pkg-config libncurses5-dev libbladerf-dev libhackrf-dev liblimesuite-dev libsoapysdr-dev devscripts
$ ./prepare-build.sh bullseye    # or buster, or stretch
$ cd package-bullseye            # or buster, or stretch
$ dpkg-buildpackage -b --no-sign
```

## Building with limited dependencies

(Supported for bullseye and buster builds only)

The package supports some build profiles to allow building without all
required SDR libraries being present. This will produce a package with
limited SDR support only.

Pass `--build-profiles` to `dpkg-buildpackage` with a comma-separated list of
profiles. The list of profiles should include `custom` and zero or more of
`rtlsdr`, `bladerf`, `hackrf`, `limesdr`, 'soapysdr' depending on what you want:

```bash
$ dpkg-buildpackage -b --no-sign --build-profiles=custom,rtlsdr          # builds with rtlsdr support only
$ dpkg-buildpackage -b --no-sign --build-profiles=custom,rtlsdr,bladerf  # builds with rtlsdr and bladeRF support
$ dpkg-buildpackage -b --no-sign --build-profiles=custom                 # builds with _no_ SDR support (network support only)
```


## Building manually

You can probably just run "make" after installing the required dependencies.
Binaries are built in the source directory; you will need to arrange to
install them (and a method for starting them) yourself.

``make BLADERF=no`` will disable bladeRF support and remove the dependency on
libbladeRF.

``make RTLSDR=no`` will disable rtl-sdr support and remove the dependency on
librtlsdr.

``make HACKRF=no`` will disable HackRF support and remove the dependency on 
libhackrf.

``make LIMESDR=no`` will disable LimeSDR support and remove the dependency on
libLimeSuite.

``make SOAPYSDR=no`` will disable SoapySDR support and remove the dependency on
libSoapySDR.

## Building on OSX

Minimal testing on Mojave 10.14.6, YMMV.

```
$ brew install librtlsdr
$ brew install libbladerf
$ brew install hackrf
$ brew install pkg-config
$ make
```

## Building on FreeBSD

Minimal testing on 12.1-RELEASE, YMMV.

```
# pkg install gmake
# pkg install pkgconf
# pkg install rtl-sdr
# pkg install bladerf
# pkg install hackrf
$ gmake
```

## Generating wisdom files

dump1090-fa uses [starch](https://github.com/flightaware/starch) to build
multiple versions of the DSP code and choose the fastest supported by the
hardware at runtime. The implementations chosen can been seen by running
`dump1090-fa --version`.

The implementations used are controlled by "wisdom files", a list of
implementations to use in order of priority. For each DSP function, the first
implementation listed that's supported by the current hardware is used.
By default dump1090-fa provides compiled-in wisdom for [x86](wisdom.x86),
[ARM 32-bit](wisdom.arm), and [ARM 64-bit](wisdom.aarch64). If the defaults
are not suitable for your hardware or if you're building on a different
architecture, you may want to generate your own external wisdom file.

Ideally, to get stable results, you want to do this on an idle system
with CPU frequency scaling disabled. Running the benchmarks will take
some time (10s of minutes).

### Package installs

Run `/usr/share/dump1090-fa/generate-wisdom`. Wait.

Follow the instructions to copy the resulting wisdom file to `/etc/dump1090-fa/wisdom.local`.

Restart dump1090.

### Manual installs

Run `make wisdom.local`. Wait.

Copy the resulting `wisdom.local` file somewhere appropriate.

Update the dump1090-fa command-line options to include `--wisdom /path/to/wisdom.local`
