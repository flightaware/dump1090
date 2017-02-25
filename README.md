# dump1090-fa Debian/Raspbian packages

This is a fork of [dump1090-mutability](https://github.com/mutability/dump1090)
customized for use within [FlightAware](http://flightaware.com)'s
[PiAware](http://flightaware.com/adsb/piaware) software.

It is designed to build as a Debian package.

## Building under jessie

### Dependencies - bladeRF

You will need a build of libbladeRF. You can build packages from source:

````
$ git clone https://github.com/Nuand/bladeRF.git
$ cd bladeRF
$ dpkg-buildpackage -b
````

Or Nuand has some build/install instructions including an Ubuntu PPA
at https://github.com/Nuand/bladeRF/wiki/Getting-Started:-Linux

Or FlightAware provides armhf packages as part of the piaware repository;
see https://flightaware.com/adsb/piaware/install

### Dependencies - rtlsdr

This is packaged with jessie. "sudo apt-get install librtlsdr-dev"

### Dependencies - Airspy

While libairspy is available via apt-get, unfortunately it is an old version that does not provide some of the functionality used by this package. As such, you should build manually:

````
$ git clone https://github.com/airspy/host.git
$ cd host
$ mkdir build
$ cd build
$ cmake ../ -DINSTALL_UDEV_RULES=ON
$ make
$ sudo make install
$ sudo ldconfig
````

You will also need the sox resampler library:

````
$ sudo apt-get install libsoxr-dev
````

If you've already installed the rtlsdr dependencies as detailed above, you should be all set. If not, you will also need the usb-1.0 development package if not already installed:

````
$ sudo apt-get install libusb-1.0-0-dev
````

### Actually building it

Nothing special, just build it ("dpkg-buildpackage -b")

## Building under wheezy

First run "prepare-wheezy-tree.sh". This will create a package tree in
package-wheezy/. Build in there ("dpkg-buildpackage -b")

The wheezy build does not include bladeRF or Airspy support.

## Building manually

You can probably just run "make" after installing the required dependencies.
Binaries are built in the source directory; you will need to arrange to
install them (and a method for starting them) yourself.

"make BLADERF=no" will disable bladeRF support and remove the dependency on
libbladeRF.

"make RTLSDR=no" will disable rtl-sdr support and remove the dependency on
librtlsdr.

"make AIRSPY=no" will disable Airspy support and remove the dependency on
libairspy.

You may use a combination of these options to exclude support for more than one device if you wish.
