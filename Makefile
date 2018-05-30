PROGNAME=dump1090

RTLSDR ?= yes
BLADERF ?= yes
UNAME ?= Linux

ifndef DUMP1090_VERSION
DUMP1090_VERSION=$(shell git describe --always --tags --match=v*)
endif

CFLAGS += -DMODES_DUMP1090_VERSION=\"$(DUMP1090_VERSION)\" -DMODES_DUMP1090_VARIANT=\"dump1090-paulyc\"
CFLAGS += -O2 -g -Wall -Werror -W -Wno-unknown-warning-option -Wno-format-truncation
CFLAGS += -fPIC -Wl,visibility=default -Wl,export-dynamic
LIBS += -lpthread -lm -lncurses
LIB_VERSION_MAJOR=0
LIB_VERSION_MINOR=1

ifdef CROSS_COMPILE
  CFLAGS += --host=$HOST --sysroot=$SYSROOT
  LDFLAGS += -pie
endif

ifndef CC
  ifeq ($(UNAME), Linux)
    CC=gcc
  endif
  ifeq ($(UNAME), Darwin)
    CC=clang
  endif
endif

ifeq ($(RTLSDR), yes)
  SDR_OBJ += sdr_rtlsdr.o
  CFLAGS += -DENABLE_RTLSDR

  ifdef RTLSDR_PREFIX
    CFLAGS += -I$(RTLSDR_PREFIX)/include
    LDFLAGS += -L$(RTLSDR_PREFIX)/lib
  else
    CFLAGS += $(shell pkg-config --cflags librtlsdr)
    LDFLAGS += $(shell pkg-config --libs-only-L librtlsdr)
  endif

  ifdef LIBUSB_PREFIX
    CFLAGS += -I$(LIBUSB_PREFIX)/include
    LDFLAGS += -L$(LIBUSB_PREFIX)/lib
  else
    CFLAGS += $(shell pkg-config --cflags libusb-1.0)
    LDFLAGS += $(shell pkg-config --libs-only-L libusb-1.0)
  endif

  ifeq ($(STATIC), yes)
    LIBS_SDR += -Wl,-Bstatic -lrtlsdr -Wl,-Bdynamic -lusb-1.0
  else
    LIBS_SDR += -lrtlsdr -lusb-1.0
  endif
endif

ifeq ($(BLADERF), yes)
  SDR_OBJ += sdr_bladerf.o
  CFLAGS += -DENABLE_BLADERF

  ifdef BLADERF_PREFIX
    CFLAGS += -I$(BLADERF_PREFIX)/include
    LDFLAGS += -L$(BLADERF_PREFIX)/lib
  else
    CFLAGS += $(shell pkg-config --cflags libbladeRF)
    LDFLAGS += $(shell pkg-config --libs-only-L libbladeRF)
  endif

  ifeq ($(STATIC), yes) 
    LIBS_SDR += -Wl,-Bstatic -lbladeRF
  else 
    LIBS_SDR += -lbladeRF 
  endif
endif 

ifeq ($(UNAME), Linux)
  LIBS+=-lrt
  CFLAGS+=-std=c11 -D_DEFAULT_SOURCE
  LIB_EXT=so
  LDFLAGS_SHARED += -shared -Wl,-soname,lib1090.so.0
endif
ifeq ($(UNAME), Darwin)
  UNAME_R := $(shell uname -r)
  ifeq ($(shell expr "$(UNAME_R)" : '1[012345]\.'),3)
    CFLAGS+=-std=c11 -DMISSING_GETTIME -DMISSING_NANOSLEEP
    COMPAT+=compat/clock_gettime/clock_gettime.o compat/clock_nanosleep/clock_nanosleep.o
  else
  # Darwin 16 (OS X 10.12) supplies clock_gettime() and clockid_t
    CFLAGS+=-std=c11 -DMISSING_NANOSLEEP -DCLOCKID_T
    COMPAT+=compat/clock_nanosleep/clock_nanosleep.o
  endif
  LIB_EXT=dylib
  LDFLAGS_SHARED += -dynamiclib -current_version 0.1 -compatibility_version 0.1
endif

all: dump1090 view1090 faup1090

%.o: %.c *.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

lib1090.so.0: anet.o interactive.o mode_ac.o mode_s.o net_io.o crc.o demod_2400.o stats.o cpr.o icao_filter.o track.o util.o convert.o sdr_ifile.o sdr.o dump1090.o view1090.o faup1090.o $(SDR_OBJ) $(COMPAT)
	gcc $(LDFLAGS_SHARED) -o $@ $(LDFLAGS) $(LIBS_SDR) $^

dump1090: dump1090-main.o lib1090.so.0
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS) $(LIBS_SDR)

view1090: view1090-main.o lib1090.so.0
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS) $(LIBS_SDR)

faup1090: faup1090-main.o lib1090.so.0
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS) $(LIBS_SDR)

clean:
	rm -f *.o compat/clock_gettime/*.o compat/clock_nanosleep/*.o dump1090 view1090 faup1090 cprtests crctests convert_benchmark lib1090.*

test: cprtests
	./cprtests

cprtests: cpr.o cprtests.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -o $@ $^ -lm

crctests: crc.c crc.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -DCRCDEBUG -o $@ $<

benchmarks: convert_benchmark
	./convert_benchmark

convert_benchmark: convert_benchmark.o convert.o util.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -o $@ $^ -lm
