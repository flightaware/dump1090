#!/bin/bash
DUMP1090_VERSION=7.2 RTLSDR=yes BLADERF=no HACKRF=no LIMESDR=no MONGOC=yes make -j8
strip -s dump1090
