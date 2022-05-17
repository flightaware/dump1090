# Adaptive gain configuration

dump1090-fa can optionally tune the receiver gain automatically to try to
pick a gain value for the particular hardware and RF environment without
manual tuning. This README covers some of the background for why this is
useful, and how to configure dump1090 to enable this feature.

## Background

In general, more receiver gain means better reception. Most ADS-B transmitters
within line of sight transmit with enough power that their messages can
potentially be decoded, but if the receiver gain setting is too low then very
weak signals may still be too weak to be decoded even after amplification.
Adding extra receiver gain helps in this case.

However, there are two problems with just adding more gain to a wideband SDR.
First, _everything_ is amplified, not only the signals of interest. Noise and
RF interference is also amplified. At high gain settings or in noisy RF
environments, this can interfere with receiving the ADS-B signals themselves.

Second, there is a wide range of possible ADS-B signal strengths. There can be
a 60dB or more difference between the weakest signals (a distant aircraft at
the limit of receiver range) and the strongest signals (a nearby aircraft on
the ramp 100m from the receiver). Increasing the receiver gain to handle the
weakest signals can mean that the strongest signals overload the receiver.
A rtlsdr receiver only has about 30-35dB of dynamic range available at a
particular gain setting, so there is no single gain setting that can
simultaneously handle both the weakest and strongest signals.

Adaptive gain tries to deal with these cases by changing the receiver gain
on the fly to handle the signal and noise levels that are currently being seen
without human intervention. It is not perfect and it's not a substitute for
hand-tuning of gain settings, but it aims at picking a reasonable setting for
cases where individual hand-tuning isn't possible.

## Where to configure adaptive gain options

How to configure adaptive gain varies depending on how you have installed
dump1090.

If you are using a PiAware sdcard image, adaptive gain can be configured by
editing `/boot/piaware-config.txt` or by using the `piaware-config` command.

If you are using the Debian package, adaptive gain can be configured by editing
`/etc/default/dump1090-fa`.

If running dump1090 directly, adaptive gain options are set directly by
command-line options.

## Default settings

For new PiAware or Debian package installations, adaptive dynamic range mode
is enabled by default and adaptive burst mode is disabled by default.

For _upgrades_ of PiAware or the Debian package from versions older than 6.0,
both adaptive gain modes are disabled by default.

These defaults can be overridden as described below.

## Adaptive gain in dynamic range mode

The dynamic range adaptive gain mode attempts to set the receiver gain to
maintain a given dynamic range - that is, it tries to set the gain so that
general noise is at or below a given level. This takes into account different
or changing RF environments and different receiver hardware (antenna,
preamplifiers, etc) that affects the overall gain of the system, and usually
will pick a reasonable gain setting without intervention.

To enable this mode:

* Set `adaptive-dynamic-range yes` in piaware-config; or
* Set `ADAPTIVE_DYNAMIC_RANGE=yes` in `/etc/default/dump1090-fa`; or
* Pass the `--adaptive-range` option on the command line.

The default settings for dynamic range will use a dynamic range target chosen
based on SDR type (e.g. 30dB for rtlsdr receivers). This is usually a good
default. To override this target:

* Set `adaptive-dynamic-range-target` in piaware-config; or
* Set `ADAPTIVE_DYNAMIC_RANGE_TARGET` in `/etc/default/dump1090-fa`; or
* Pass the `--adaptive-range-target` option on the command line.

## Adaptive gain in "burst" / loud signal mode

The "burst" adaptive gain mode listens for loud bursts of signal that were
_not_ successfully decoded as ADS-B messages, but which have approximately
the right timing to be possible messages that were lost due to receiver
overloading. When enough overly-loud signals are heard in a short period of
time, dump1090 will _reduce_ the receiver gain to try to allow them to be
received.

This is a more situational setting. It may allow reception of loud nearby
aircraft (e.g. if you are close to an airport). The tradeoff is that when
there are nearby aircraft, overall receiver range may be reduced. Whether
this is a good tradeoff depends on the aircraft you're interested in.
By default, adaptive gain burst mode is disabled.

To enable burst mode:

* Set `adaptive-burst yes` in piaware-config; or
* Set `ADAPTIVE_BURST=yes` in `/etc/default/dump1090-fa`; or
* Pass the `--adaptive-burst` option on the command line.

This mode is more experimental than the dynamic range mode and tweaking of
the advanced burst options may be needed depending on your local installation.
In particular, `--adaptive-burst-loud-rate` and `adaptive-burst-quiet-rate`
may need adjusting. Feedback on what works for you and what doesn't would
be appreciated!

Burst mode and dynamic range mode can be enabled at the same time.

## Limiting the gain range

If you know in advance approximately what the gain setting should be, so
you want to allow adaptive gain to change the gain only within a certain range,
you can set minimum and maximum gain settings in dB. Adaptive gain will only
adjust the gain within this range. To set this:

* Set `adaptive-min-gain` and `adaptive-max-gain` in piaware-config; or
* Set `ADAPTIVE_MIN_GAIN` and `ADAPTIVE_MAX_GAIN` in `/etc/default/dump1090-fa`; or
* Pass the `--adaptive-min-gain` and `--adaptive-max-gain` options on the command line.

If you know approximately where the gain should be, then a good starting point would be
to set the max and min adaptive gain to +/- 10dB around your gain setting.

## Reducing the CPU cost of adaptive gain

The measurements needed to adjust gain have a CPU cost, and on slower
devices it may be useful to reduce the amount of work that adaptive gain does.
This can be done by adjusting the adaptive gain duty cycle. This is a
percentage that controls what fraction of incoming data adaptive gain inspects.
100% means that every sample is inspected. Lower values reduce CPU use, with
a tradeoff that adaptive gain has a less accurate picture of the RF
environment. The default duty cycle is 50% on "fast" CPUs and 10% on "slow"
CPUs (where currently "slow" means "armv6 architecture", for example the
Pi Zero or Pi 1). To reduce the duty cycle further:

* Set `slow-cpu yes` in piaware-config; or
* Set `SLOW_CPU=yes` in `/etc/default/dump1090-fa`; or
* Pass the `--adaptive-duty-cycle` option on the command line

## Advanced options

There are a number of advanced options that are only supported as
command-line options or via the EXTRA_OPTIONS setting in
`/etc/default/dump1090-fa`. They tweak settings that require some knowledge of
dump1090 internals to make sense of, so YMMV.

For a complete list of options, run `dump1090-fa --help` and look at the
adaptive gain section.

## Device support

Currently, adaptive gain is only supported on rtlsdr devices. Support for other
SDRs is planned for the future.

If you're a developer and want to add support for your SDR, you'll need
to implement the gain control API used in `sdr.[ch]`. See `sdr_rtlsdr.c`
(`rtlsdrGetGain`, `rtlsdrSetGain`, etc) for examples.

## Comparison with wiedehopf's auto-gain scripts

There is an [auto-gain script](https://github.com/wiedehopf/adsb-scripts/wiki/Automatic-gain-optimization-for-readsb-and-dump1090-fa)
written by [wiedehopf](https://github.com/wiedehopf) that aims to solve similar
problems. The implementation approaches are quite different, and which one works best
for you will depend on the problem you're trying to solve.

The major differences between adaptive gain and the auto-gain script are:

 * Adaptive gain works on short-term data (seconds or minutes) and can react to
   changes in a similar timeframe. It tries to set an appropriate gain for the
   _current_ environment without much regard for longer-term trends. The auto-gain
   script looks at data over longer timeframes (1+ days) and reacts more slowly,
   but takes into account data across that whole period.

 * Adaptive gain in dynamic range mode looks at an estimate of the noise floor
   to decide whether to increase gain. Adaptive gain in burst mode looks at the
   signal strength of samples that were _not_ successfully decoded to decide when
   to reduce gain. The auto-gain scripts look at the signal strength of _successfully_
   decoded messages to decide when to increase or decrease gain. These are each measuring
   something different, and which is most effective will depend on the exact RF
   environment.

 * Adaptive gain burst mode and the auto-gain script superfically measure similar things,
   but the difference in measurement timeframe mean they react quite differently. Burst mode
   tries to react to transient loud signals i.e. temporarily nearby aircraft. The auto-gain
   script uses the loud-messages fraction as an estimate for messages lost to excessive
   gain over the long term, not only the transient case.

 * Adaptive gain has a few less moving parts (i.e. no external scripts, no config changes over
   time) as it's built directly into dump1090 itself.
