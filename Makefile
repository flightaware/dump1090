PROGNAME=dump1090

DUMP1090_VERSION ?= unknown

CPPFLAGS += -DMODES_DUMP1090_VERSION=\"$(DUMP1090_VERSION)\" -DMODES_DUMP1090_VARIANT=\"dump1090-fa\"

DIALECT = -std=c11
DUMP1090_CFLAGS = $(DIALECT) -O3 -g -Wall -Wmissing-declarations -Werror -W -D_DEFAULT_SOURCE -fno-common $(CFLAGS)
LIBS = -lpthread -lm
SDR_OBJ = sdr.o fifo.o sdr_ifile.o

# Try to autodetect available libraries via pkg-config if no explicit setting was used
PKGCONFIG=$(shell pkg-config --version >/dev/null 2>&1 && echo "yes" || echo "no")
ifeq ($(PKGCONFIG), yes)
  ifndef RTLSDR
    ifdef RTLSDR_PREFIX
      RTLSDR := yes
    else
      RTLSDR := $(shell pkg-config --exists librtlsdr && echo "yes" || echo "no")
    endif
  endif

  ifndef BLADERF
    BLADERF := $(shell pkg-config --exists libbladeRF && echo "yes" || echo "no")
  endif

  ifndef HACKRF
    HACKRF := $(shell pkg-config --exists libhackrf && echo "yes" || echo "no")
  endif

  ifndef LIMESDR
    LIMESDR := $(shell pkg-config --exists LimeSuite && echo "yes" || echo "no")
  endif
else
  # pkg-config not available. Only use explicitly enabled libraries.
  RTLSDR ?= no
  BLADERF ?= no
  HACKRF ?= no
  LIMESDR ?= no
endif

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
  DUMP1090_CFLAGS += -D_DEFAULT_SOURCE
  LIBS += -lrt
  LIBS_USB += -lusb-1.0
endif

ifeq ($(UNAME), Darwin)
  ifneq ($(shell sw_vers -productVersion | egrep '^10\.([0-9]|1[01])\.'),) # Mac OS X ver <= 10.11
    DUMP1090_DUMP1090_CFLAGS += -DMISSING_GETTIME
    COMPAT += compat/clock_gettime/clock_gettime.o
  endif
  DUMP1090_CFLAGS += -DMISSING_NANOSLEEP
  COMPAT += compat/clock_nanosleep/clock_nanosleep.o
  LIBS_USB += -lusb-1.0
endif

ifeq ($(UNAME), OpenBSD)
  DUMP1090_CFLAGS += -DMISSING_NANOSLEEP
  COMPAT += compat/clock_nanosleep/clock_nanosleep.o
  LIBS_USB += -lusb-1.0
endif

ifeq ($(UNAME), FreeBSD)
  DUMP1090_CFLAGS += -D_DEFAULT_SOURCE
  LIBS += -lrt
  LIBS_USB += -lusb
endif

RTLSDR ?= yes
BLADERF ?= yes

ifeq ($(RTLSDR), yes)
  SDR_OBJ += sdr_rtlsdr.o
  CPPFLAGS += -DENABLE_RTLSDR

  ifdef RTLSDR_PREFIX
    CPPFLAGS += -I$(RTLSDR_PREFIX)/include
    ifeq ($(STATIC), yes)
      LIBS_SDR += -L$(RTLSDR_PREFIX)/lib -Wl,-Bstatic -lrtlsdr -Wl,-Bdynamic $(LIBS_USB)
    else
      LIBS_SDR += -L$(RTLSDR_PREFIX)/lib -lrtlsdr $(LIBS_USB)
    endif
  else
    # some packaged .pc files are massively broken, try to handle it

    # FreeBSD's librtlsdr.pc includes -std=gnu89 in cflags
    RTLSDR_DUMP1090_CFLAGS := $(shell pkg-config --cflags librtlsdr)
    DUMP1090_CFLAGS += $(filter-out -std=%,$(RTLSDR_DUMP1090_CFLAGS))

    # some linux librtlsdr packages return a bare -L with no path in --libs
    # which horribly confuses things because it eats the next option on the command line
    RTLSDR_LFLAGS := $(shell pkg-config --libs-only-L librtlsdr)
    ifeq ($(RTLSDR_LFLAGS),-L)
      LIBS_SDR += $(shell pkg-config --libs-only-l --libs-only-other librtlsdr)
    else
      LIBS_SDR += $(shell pkg-config --libs librtlsdr)
    endif
  endif
endif

ifeq ($(BLADERF), yes)
  SDR_OBJ += sdr_bladerf.o
  CPPFLAGS += -DENABLE_BLADERF
  DUMP1090_CFLAGS += $(shell pkg-config --cflags libbladeRF)
  LIBS_SDR += $(shell pkg-config --libs libbladeRF)
endif

ifeq ($(HACKRF), yes)
  SDR_OBJ += sdr_hackrf.o
  CPPFLAGS += -DENABLE_HACKRF
  DUMP1090_CFLAGS += $(shell pkg-config --cflags libhackrf)
  LIBS_SDR += $(shell pkg-config --libs libhackrf)
endif

ifeq ($(LIMESDR), yes)
  SDR_OBJ += sdr_limesdr.o
  CPPFLAGS += -DENABLE_LIMESDR
  DUMP1090_CFLAGS += $(shell pkg-config --cflags LimeSuite)
  LIBS_SDR += $(shell pkg-config --libs LimeSuite)
endif

all: showconfig dump1090 view1090

showconfig:
	@echo "Building with:" >&2
	@echo "  Version string:  $(DUMP1090_VERSION)" >&2
	@echo "  RTLSDR support:  $(RTLSDR)" >&2
	@echo "  BladeRF support: $(BLADERF)" >&2
	@echo "  HackRF support:  $(HACKRF)" >&2
	@echo "  LimeSDR support: $(LIMESDR)" >&2

all: dump1090 view1090

%.o: %.c *.h
	$(CC) $(CPPFLAGS) $(DUMP1090_CFLAGS) -c $< -o $@

dump1090: dump1090.o anet.o interactive.o mode_ac.o mode_s.o comm_b.o net_io.o crc.o demod_2400.o stats.o cpr.o icao_filter.o track.o util.o convert.o ais_charset.o $(SDR_OBJ) $(COMPAT)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS) $(LIBS_SDR) -lncurses

view1090: view1090.o anet.o interactive.o mode_ac.o mode_s.o comm_b.o net_io.o crc.o stats.o cpr.o icao_filter.o track.o util.o ais_charset.o $(COMPAT)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS) -lncurses

faup1090: faup1090.o anet.o mode_ac.o mode_s.o comm_b.o net_io.o crc.o stats.o cpr.o icao_filter.o track.o util.o ais_charset.o $(COMPAT)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f *.o oneoff/*.o compat/clock_gettime/*.o compat/clock_nanosleep/*.o dump1090 view1090 faup1090 cprtests crctests convert_benchmark

test: cprtests
	./cprtests

cprtests: cpr.o cprtests.o
	$(CC) $(CPPFLAGS) $(DUMP1090_CFLAGS) -g -o $@ $^ -lm

crctests: crc.c crc.h
	$(CC) $(CPPFLAGS) $(DUMP1090_CFLAGS) -g -DCRCDEBUG -o $@ $<

benchmarks: oneoff/convert_benchmark
	oneoff/convert_benchmark

oneoff/convert_benchmark: oneoff/convert_benchmark.o convert.o util.o
	$(CC) $(CPPFLAGS) $(DUMP1090_CFLAGS) -g -o $@ $^ -lm -lpthread

oneoff/decode_comm_b: oneoff/decode_comm_b.o comm_b.o ais_charset.o
	$(CC) $(CPPFLAGS) $(DUMP1090_CFLAGS) -g -o $@ $^ -lm
