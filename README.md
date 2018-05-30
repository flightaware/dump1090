# dump1090-fa Debian/Raspbian packages

This is a fork of [dump1090-mutability](https://github.com/mutability/dump1090)
customized for use within [FlightAware](http://flightaware.com)'s
[PiAware](http://flightaware.com/adsb/piaware) software.

It is designed to build as a Debian package.

## Building under jessie

### Dependencies - bladeRF

You will need a build of libbladeRF. You can build packages from source:

$ git clone https://github.com/Nuand/bladeRF.git
$ cd bladeRF
$ dpkg-buildpackage -b

Previously, the dump1090 webmap used Google's map API. As of July 2016, Google's policy on keyless use of their
API has changed and it's no longer practical to use that API. To avoid having a completely nonfunctional
map on new installs that have not been grandfathered, dump1090 now uses the OpenLayers map API.

This means:

* The default view now uses OpenStreetMap tiles;
* Google's maps are not available even with an API key (Google does not allow use of their imagery via
  third-party APIs such as OpenLayers);
* There are a couple of new layers - Bing and Mapzen - that can be enabled by providing an API key
  in config.js. See the comments in config.js for details.

# Manual installation

To install from packages directly:

You will need a librtlsdr0 package for Raspbian.
There is no standard build of this.
I have built suitable packages that are available from 
[this release page](https://github.com/mutability/librtlsdr/releases)

Then you will need the dump1090-mutability package itself from
[this release page](https://github.com/mutability/dump1090/releases)

Install the packages with dpkg.

# Configuration

By default it'll only ask you whether to start automatically and assume sensible defaults for everything else.
Notable defaults that are perhaps not what you'd first expect:

* All network ports are bound to the localhost interface only.
  If you need remote access to the ADS-B data ports, you will want to change this to bind to the wildcard address.
* The internal HTTP server is disabled. I recommend using an external webserver (see below).
  You can reconfigure to enable the internal one if you don't want to use an external one.

To reconfigure, either use `dpkg-reconfigure dump1090-mutability` or edit `/etc/default/dump1090-mutability`. Both should be self-explanatory.

## External webserver configuration

This is the recommended configuration; a dedicated webserver is almost always going to be better and more secure than the collection of hacks that is the dump1090 webserver.
It works by having dump1090 write json files to a path under `/run` once a second (this is on tmpfs and will not write to the sdcard).
Then an external webserver is used to serve both the static html/javascript files making up the map view, and the json files that provide the dynamic data.

Or Nuand has some build/install instructions including an Ubuntu PPA
at https://github.com/Nuand/bladeRF/wiki/Getting-Started:-Linux

Or FlightAware provides armhf packages as part of the piaware repository;
see https://flightaware.com/adsb/piaware/install

### Dependencies - rtlsdr

This is packaged with jessie. "sudo apt-get install librtlsdr-dev"

### Actually building it

Nothing special, just build it ("dpkg-buildpackage -b")

## Building under wheezy

First run "prepare-wheezy-tree.sh". This will create a package tree in
package-wheezy/. Build in there ("dpkg-buildpackage -b")

The wheezy build does not include bladeRF support.

## Building manually

You can probably just run "make" after installing the required dependencies.
Binaries are built in the source directory; you will need to arrange to
install them (and a method for starting them) yourself.

"make BLADERF=no" will disable bladeRF support and remove the dependency on
libbladeRF.

"make RTLSDR=no" will disable rtl-sdr support and remove the dependency on
librtlsdr.

````
$ sudo apt-get install librtlsdr-dev libusb-1.0-0-dev pkg-config debhelper
$ dpkg-buildpackage -b
````

Or you can use debuild/pdebuild. I find building via qemubuilder quite effective for building images for Raspbian (it's actually faster to build on an emulated ARM running on my PC than to build directly on real hardware).

Here's the pbuilder config I use to build the Raspbian packages:

````
MIRRORSITE=http://mirrordirector.raspbian.org/raspbian/
PDEBUILD_PBUILDER=cowbuilder
BASEPATH=/var/cache/pbuilder/armhf-raspbian-wheezy-base.cow
DISTRIBUTION=wheezy
OTHERMIRROR="deb http://repo.mutability.co.uk/raspbian wheezy rpi"
ARCHITECTURE=armhf
DEBOOTSTRAP=qemu-debootstrap
DEBOOTSTRAPOPTS="--variant=buildd --keyring=/usr/share/keyrings/raspbian-archive-keyring.gpg"
COMPONENTS="main contrib non-free rpi"
EXTRAPACKAGES="eatmydata debhelper fakeroot"
ALLOWUNTRUSTED="no"
APTKEYRINGS=("/home/oliver/ppa/mutability.gpg")
````
 
>**Note about Bias-t support:**
 Bias-t support is available for RTL-SDR.com V3 dongles. If you wish to enable bias-t support, you must insure that you are building this package with a version of librtlsdr that supports this capability. You can find suitable source packages [here](https://github.com/rtlsdrblog/rtl_biast) and [here](https://github.com/librtlsdr/librtlsdr/tree/master/src). To enable the necessary support code when building, be sure to include preprocessor define macro HAVE_RTL_BIAST.
