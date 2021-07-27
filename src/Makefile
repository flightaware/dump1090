PROGNAME=dump1090

DUMP1090_VERSION ?= unknown

CPPFLAGS += -I. -DMODES_DUMP1090_VERSION=\"$(DUMP1090_VERSION)\" -D_DEFAULT_SOURCE -DMODES_DUMP1090_VARIANT=\"dump1090-fa\"

DIALECT = -std=c11
CFLAGS += $(DIALECT) -O3 -g -Wall -Wmissing-declarations -Werror -W -fno-common
LIBS = -lpthread -lm
SDR_OBJ = cpu.o sdr.o fifo.o sdr_ifile.o dsp/helpers/tables.o

BIN_PATH = ./build
OBJ_PATH = ./build/obj/
MKDIR_P = mkdir -p
CPU_FEATURES_PATH = ../external/cpu_features

ifeq ($(OS),Windows_NT) 
  DETECTED_OS := Windows
else
  DETECTED_OS := $(shell sh -c 'uname 2>/dev/null || echo Unknown')
endif

ifeq ($(DETECTED_OS), Linux)
  LIBS += -lrt
  LIBS_USB += -lusb-1.0
  LIBS_CURSES := -lncurses
  CPUFEATURES ?= yes
endif

ifeq ($(DETECTED_OS), Darwin)
  ifneq ($(shell sw_vers -productVersion | egrep '^10\.([0-9]|1[01])\.'),) # Mac OS X ver <= 10.11
    CPPFLAGS += -DMISSING_GETTIME
    COMPAT += compat/clock_gettime/clock_gettime.o
  endif
  CPPFLAGS += -DMISSING_NANOSLEEP
  COMPAT += compat/clock_nanosleep/clock_nanosleep.o
  LIBS_USB += -lusb-1.0
  LIBS_CURSES := -lncurses
  CPUFEATURES ?= yes
endif

ifeq ($(DETECTED_OS), OpenBSD)
  CPPFLAGS += -DMISSING_NANOSLEEP
  COMPAT += compat/clock_nanosleep/clock_nanosleep.o
  LIBS_USB += -lusb-1.0
  LIBS_CURSES := -lncurses
  CPUFEATURES ?= yes
endif

# TODO: Does google/cpu_features support FreeBSD and NetBSD?

ifeq ($(DETECTED_OS), FreeBSD)
  LIBS += -lrt
  LIBS_USB += -lusb
  LIBS_CURSES := -lncurses
endif

ifeq ($(DETECTED_OS), NetBSD)
  LIBS += -lrt
  LIBS_USB += -lusb-1.0
  LIBS_CURSES := -lcurses
endif

ifeq ($(DETECTED_OS), Windows)
  # TODO: Perhaps copy the DLL files to the output folder if the OS is Windows?
  CPPFLAGS += -DMISSING_TIME_R_FUNCS -DMISSING_ALIGNED_ALLOC_WIN -DMISSING_CURSES_H_NCURSES
  LIBS += -lws2_32 -lsystre
  LIBS_USB += -lusb-1.0
  LIBS_CURSES := -lncurses
  CPUFEATURES ?= yes
endif


##
## SDR Libraries
##

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
    # some linux librtlsdr packages return a bare -I/ with no path in --cflags
    RTLSDR_CFLAGS := $(shell pkg-config --cflags librtlsdr)
    RTLSDR_CFLAGS := $(filter-out -std=%,$(RTLSDR_CFLAGS))
    RTLSDR_CFLAGS := $(filter-out -I/,$(RTLSDR_CFLAGS))
    CFLAGS += $(RTLSDR_CFLAGS)

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

##
## END SDR Libraries
##


##
## starch (runtime DSP code selection) mix, architecture-specific
##

CPUFEATURES ?= no
ifeq ($(CPUFEATURES),yes)
  include Makefile.cpufeatures
  CPPFLAGS += -DENABLE_CPUFEATURES -I"$(CPU_FEATURES_PATH)/include"
endif

ARCH ?= $(shell uname -m)
ifneq ($(CPUFEATURES),yes)
  # need to be able to detect CPU features at runtime to enable any non-standard compiler flags
  STARCH_MIX := generic
  CPPFLAGS += -DSTARCH_MIX_GENERIC
else
  ifeq ($(ARCH),x86_64)
    # AVX, AVX2
    STARCH_MIX := x86
    CPPFLAGS += -DSTARCH_MIX_X86
  else ifeq ($(findstring arm,$(ARCH)),arm)
    # ARMv7 NEON
    STARCH_MIX := arm
    CPPFLAGS += -DSTARCH_MIX_ARM
  else ifeq ($(findstring aarch,$(ARCH)),aarch)
    STARCH_MIX := aarch64
    CPPFLAGS += -DSTARCH_MIX_AARCH64
  else
    STARCH_MIX := generic
    CPPFLAGS += -DSTARCH_MIX_GENERIC
  endif
endif
all: showconfig dump1090 view1090 starch-benchmark

STARCH_COMPILE := $(CC) $(CPPFLAGS) $(CFLAGS) -c
include dsp/generated/makefile.$(STARCH_MIX)

##
## END starch
##

showconfig:
	@echo "Building with:" >&2
	@echo "  Version string:  $(DUMP1090_VERSION)" >&2
	@echo "  DSP mix:         $(STARCH_MIX)" >&2
	@echo "  RTLSDR support:  $(RTLSDR)" >&2
	@echo "  BladeRF support: $(BLADERF)" >&2
	@echo "  HackRF support:  $(HACKRF)" >&2
	@echo "  LimeSDR support: $(LIMESDR)" >&2

$(BIN_PATH) $(OBJ_PATH):
	@${MKDIR_P} $@

%.o: %.c *.h
	@${MKDIR_P} $(dir $(OBJ_PATH)$@)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $(OBJ_PATH)$@

dump1090: dump1090.o anet.o interactive.o mode_ac.o mode_s.o comm_b.o net_io.o crc.o demod_2400.o stats.o cpr.o icao_filter.o track.o util.o convert.o ais_charset.o adaptive.o $(SDR_OBJ) $(COMPAT) $(CPUFEATURES_OBJS) $(STARCH_OBJS)
	$(CC) -g -o $(BIN_PATH)/$@ $(addprefix $(OBJ_PATH), $^) $(LDFLAGS) $(LIBS) $(LIBS_SDR) $(LIBS_CURSES)

view1090: view1090.o anet.o interactive.o mode_ac.o mode_s.o comm_b.o net_io.o crc.o stats.o cpr.o icao_filter.o track.o util.o ais_charset.o sdr_stub.o$(COMPAT)
	$(CC) -g -o $(BIN_PATH)/$@ $(addprefix $(OBJ_PATH), $^) $(LDFLAGS) $(LIBS) $(LIBS_CURSES)

faup1090: faup1090.o anet.o mode_ac.o mode_s.o comm_b.o net_io.o crc.o stats.o cpr.o icao_filter.o track.o util.o ais_charset.o sdr_stub.o $(COMPAT)
	$(CC) -g -o $(BIN_PATH)/$@ $(addprefix $(OBJ_PATH), $^) $(LDFLAGS) $(LIBS)

starch-benchmark: cpu.o dsp/helpers/tables.o $(CPUFEATURES_OBJS) $(STARCH_OBJS) $(STARCH_BENCHMARK_OBJ)
	$(CC) -g -o $(BIN_PATH)/$@ $(addprefix $(OBJ_PATH), $^) $(LDFLAGS) $(LIBS)

clean:
	@rm -rf build

test: cprtests
	$(BIN_PATH)/cprtests

cprtests: cpr.o cprtests.o
	$(CC) -g -o $(BIN_PATH)/$@ $(addprefix $(OBJ_PATH), $^) -lm

crctests: crc.c crc.h
	$(CC) -g -DCRCDEBUG -o $(BIN_PATH)/$@ -I. $<

benchmarks: oneoff/convert_benchmark
	$(BIN_PATH)/oneoff/convert_benchmark

oneoff/convert_benchmark: oneoff/convert_benchmark.o convert.o util.o dsp/helpers/tables.o cpu.o $(CPUFEATURES_OBJS) $(STARCH_OBJS)
	$(CC) -g -o $(BIN_PATH)/$@ $(addprefix $(OBJ_PATH), $^) -lm -lpthread

oneoff/decode_comm_b: oneoff/decode_comm_b.o comm_b.o ais_charset.o
	$(CC) -g -o $(BIN_PATH)/$@ $(addprefix $(OBJ_PATH), $^) -lm

oneoff/dsp_error_measurement: oneoff/dsp_error_measurement.o dsp/helpers/tables.o cpu.o $(CPUFEATURES_OBJS) $(STARCH_OBJS)
	$(CC) -g -o $(BIN_PATH)/$@ $(addprefix $(OBJ_PATH), $^) -lm

oneoff/uc8_capture_stats: oneoff/uc8_capture_stats.o
	$(CC) -g -o $(BIN_PATH)/$@ $(addprefix $(OBJ_PATH), $^) -lm

starchgen:
	dsp/starchgen.py .

.PHONY: wisdom.local
wisdom.local: starch-benchmark
	$(BIN_PATH)/starch-benchmark -i 5 -o wisdom.local mean_power_u16 mean_power_u16_aligned magnitude_uc8 magnitude_uc8_aligned
	$(BIN_PATH)/starch-benchmark -i 5 -r wisdom.local -o wisdom.local
