PROGNAME=dump1090

# Try to autodetect available libraries if no explicit setting was used

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

CPPFLAGS += -DMODES_DUMP1090_VERSION=\"$(DUMP1090_VERSION)\" -DMODES_DUMP1090_VARIANT=\"dump1090-fa\"

DIALECT = -std=c11
CFLAGS += $(DIALECT) -O2 -g -Wall -Wmissing-declarations -Werror -W -D_DEFAULT_SOURCE -fno-common
LIBS = -lpthread -lm -lrt

ifeq ($(RTLSDR), yes)
  SDR_OBJ += sdr_rtlsdr.o
  CPPFLAGS += -DENABLE_RTLSDR

  ifdef RTLSDR_PREFIX
    CPPFLAGS += -I$(RTLSDR_PREFIX)/include
    ifeq ($(STATIC), yes)
      LIBS_SDR += -L$(RTLSDR_PREFIX)/lib -Wl,-Bstatic -lrtlsdr -Wl,-Bdynamic -lusb-1.0
    else
      LIBS_SDR += -L$(RTLSDR_PREFIX)/lib -lrtlsdr -lusb-1.0
    endif
  else
    CFLAGS += $(shell pkg-config --cflags librtlsdr)
    # some packaged .pc files are massively broken, try to handle it
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
  CFLAGS += $(shell pkg-config --cflags libbladeRF)
  LIBS_SDR += $(shell pkg-config --libs libbladeRF)
endif

ifeq ($(HACKRF), yes)
  SDR_OBJ += sdr_hackrf.o
  CPPFLAGS += -DENABLE_HACKRF
  CFLAGS += $(shell pkg-config --cflags libhackrf)
  LIBS_SDR += $(shell pkg-config --libs libhackrf)
endif

ifeq ($(LIMESDR), yes)
  SDR_OBJ += sdr_limesdr.o
  CPPFLAGS += -DENABLE_LIMESDR
  CFLAGS += $(shell pkg-config --cflags LimeSuite)
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
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

dump1090: dump1090.o anet.o interactive.o mode_ac.o mode_s.o comm_b.o net_io.o crc.o demod_2400.o stats.o cpr.o icao_filter.o track.o util.o convert.o sdr_ifile.o sdr.o ais_charset.o $(SDR_OBJ) $(COMPAT)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS) $(LIBS_SDR) -lncurses

view1090: view1090.o anet.o interactive.o mode_ac.o mode_s.o comm_b.o net_io.o crc.o stats.o cpr.o icao_filter.o track.o util.o ais_charset.o $(COMPAT)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS) -lncurses

faup1090: faup1090.o anet.o mode_ac.o mode_s.o comm_b.o net_io.o crc.o stats.o cpr.o icao_filter.o track.o util.o ais_charset.o $(COMPAT)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f *.o compat/clock_gettime/*.o compat/clock_nanosleep/*.o dump1090 view1090 faup1090 cprtests crctests convert_benchmark

test: cprtests
	./cprtests

cprtests: cpr.o cprtests.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -o $@ $^ -lm

crctests: crc.c crc.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -DCRCDEBUG -o $@ $<

benchmarks: convert_benchmark
	./convert_benchmark

oneoff/convert_benchmark: oneoff/convert_benchmark.o convert.o util.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -o $@ $^ -lm

oneoff/decode_comm_b: oneoff/decode_comm_b.o comm_b.o ais_charset.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -o $@ $^ -lm
