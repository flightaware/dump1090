PROGNAME=dump1090

DISTDIR = build
DISTFILES = dump1090 view1090 dump1090.sh COPYING LICENSE README.md README-json.md README-IFD.md public_html data

RTLSDR ?= yes
BLADERF ?= yes
MONGOC ?= yes

CPPFLAGS += -DMODES_DUMP1090_VERSION=\"$(DUMP1090_VERSION)\" -DMODES_DUMP1090_VARIANT=\"dump1090-fa\"

DIALECT = -std=c11
CFLAGS += $(DIALECT) -O2 -g -Wall -Werror -W -D_DEFAULT_SOURCE
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

ifeq ($(MONGOC), yes)
  SDR_OBJ += mongo_conn.o
  CPPFLAGS += -DENABLE_MONGOC
  CFLAGS += $(shell pkg-config --cflags libmongoc-static-1.0)
  LIBS_SDR += $(shell pkg-config --libs libmongoc-static-1.0)
endif

.PHONY: dist

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
	rm -rf *.o compat/clock_gettime/*.o compat/clock_nanosleep/*.o dump1090 view1090 faup1090 cprtests crctests convert_benchmark $(DISTDIR)

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

dist: all
	mkdir -p $(DISTDIR)
	for F in $(DISTFILES); do cp -r $$F $(DISTDIR)/; done

