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
