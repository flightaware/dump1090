#!/bin/sh
# Build fuzz/replay.c (deterministic replay driver) with the normal
# optimisation flags, reusing the object files from a top-level 'make'.
#
#   make RTLSDR=no BLADERF=no HACKRF=no LIMESDR=no SOAPYSDR=no dump1090
#   fuzz/build-replay.sh [output-binary]     (default: fuzz/replay)
set -e
SRC=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$SRC/fuzz/replay}
CC=${CC:-cc}
cd "$SRC"
case "$(uname -m)" in
    x86_64|amd64) MIX=-DSTARCH_MIX_X86 ;;
    aarch64|arm64) MIX=-DSTARCH_MIX_AARCH64 ;;
    arm*) MIX=-DSTARCH_MIX_ARM ;;
    *) MIX=-DSTARCH_MIX_GENERIC ;;
esac
STARCH_OBJS=$(ls dsp/generated/flavor.*.o dsp/generated/dispatcher.o)
$CC -I. -D_POSIX_C_SOURCE=200112L -D_DEFAULT_SOURCE $MIX -DMODES_DUMP1090_VERSION=\"replay\" -DMODES_DUMP1090_VARIANT=\"dump1090-fa\" \
    ${CFLAGS:--O3 -g} -std=c11 -fno-common -Wall -W -o "$OUT" fuzz/replay.c sdr_stub.c \
    mode_ac.o mode_s.o comm_b.o crc.o stats.o cpr.o icao_filter.o track.o util.o ais_charset.o anet.o \
    demod_2400.o convert.o cpu.o adaptive.o dsp/helpers/tables.o $STARCH_OBJS \
    -Wl,--wrap=mstime,--wrap=clock_gettime -lm -lpthread -lrt
echo "built $OUT"
