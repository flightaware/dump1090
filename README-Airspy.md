# Building with Airspy support

The debian packaging for Airspy support has not yet been implemented, but manually building is pretty straightforward:

## Building under jessie

To enable support for Airspy devices, you will need the airspy, sox resampler, and usb-1.0 libs. For the first two, you should be able to do this:

````
$ sudo apt-get install libairspy-dev
$ sudo apt-get install libsoxr-dev
````

If you've already installed the librtlsdr-dev package and it's prerequisites, you should be all set. If not, you will also need the usb-1.0 lib if not already installed:

````
$ sudo apt-get install libusb-1.0-0-dev
````

To build dump1090 with support for all devices (rtlsdr, bladerf, and airspy), simply do this:

````
$ make
````

To exclude building support for a device (bladerf in this example), try this:

````
$ make BLADERF=no
````

## Building under wheezy

The airspy lib appears to be available in the wheezy-backports repository, so assuming you are setup to install packagaes from wheezy-backports, you should be able to do this:

````
$ sudo apt-get -t wheezy-backports install libairspy-dev
````

The sox resampler lib does not appear to be directly available in wheezy, so you will need to download the source from [here](https://sourceforge.net/projects/soxr/files/) and build as per the package instructions.

If not already installed, the usb-1.0 lib is also available in wheezy:

````
$ sudo apt-get install libusb-1.0-0-dev
````

Now just use 'make' as detailed above.


