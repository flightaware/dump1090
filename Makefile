PROGNAME=dump1090

RTLSDR ?= yes
BLADERF ?= yes
AIRSPY ?= yes

CC=gcc
CPPFLAGS += -DMODES_DUMP1090_VERSION=\"$(DUMP1090_VERSION)\" -DMODES_DUMP1090_VARIANT=\"dump1090-fa\"

ifneq ($(HTMLPATH),"")
	CPPFLAGS += -DHTMLPATH=\"$(HTMLPATH)\"
endif

DIALECT = -std=c11
CFLAGS += $(DIALECT) -O2 -g -Wall -Werror -W -D_DEFAULT_SOURCE
LIBS = -lpthread -lm -lrt

ifeq ($(RTLSDR), yes)
  SDR_OBJ += sdr_rtlsdr.o
  CPPFLAGS += -DENABLE_RTLSDR

  ifdef RTLSDR_PREFIX
    CPPFLAGS += -I$(RTLSDR_PREFIX)/include
    LDFLAGS += -L$(RTLSDR_PREFIX)/lib
  else
    CFLAGS += $(shell pkg-config --cflags librtlsdr)
    LDFLAGS += $(shell pkg-config --libs-only-L librtlsdr)
  endif

  ifeq ($(STATIC), yes)
    LIBS_SDR += -Wl,-Bstatic -lrtlsdr -Wl,-Bdynamic -lusb-1.0
  else
    LIBS_SDR += -lrtlsdr -lusb-1.0
  endif
endif

ifeq ($(BLADERF), yes)
  SDR_OBJ += sdr_bladerf.o
  CPPFLAGS += -DENABLE_BLADERF
  CFLAGS += $(shell pkg-config --cflags libbladeRF)
  LIBS_SDR += $(shell pkg-config --libs libbladeRF)
endif

ifeq ($(AIRSPY), yes)
  SDR_OBJ += sdr_airspy.o
  CPPFLAGS += -DENABLE_AIRSPY
  ifeq ($(RTLSDR), no)
    CFLAGS += $(shell pkg-config --cflags libusb-1.0)
    LIBS_SDR += $(shell pkg-config --libs libusb-1.0)
  endif
  CFLAGS += $(shell pkg-config --cflags libairspy soxr)
  LIBS_SDR += $(shell pkg-config --libs libairspy soxr)
endif

all: dump1090 view1090

%.o: %.c *.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

dump1090: dump1090.o anet.o interactive.o mode_ac.o mode_s.o net_io.o crc.o demod_2400.o stats.o cpr.o icao_filter.o track.o util.o convert.o sdr_ifile.o sdr.o $(SDR_OBJ) $(COMPAT)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS) $(LIBS_SDR) -lncurses

view1090: view1090.o anet.o interactive.o mode_ac.o mode_s.o net_io.o crc.o stats.o cpr.o icao_filter.o track.o util.o $(COMPAT)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LIBS) -lncurses

faup1090: faup1090.o anet.o mode_ac.o mode_s.o net_io.o crc.o stats.o cpr.o icao_filter.o track.o util.o $(COMPAT)
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

convert_benchmark: convert_benchmark.o convert.o util.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -g -o $@ $^ -lm
