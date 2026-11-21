#!/bin/sh
# Build the libFuzzer harnesses with clang + ASan + UBSan.
#
#   fuzz/build.sh [outdir]      (default outdir: fuzz/out)
#
# Then e.g.:  fuzz/out/fuzz_decode -max_total_time=600 fuzz/out/corpus_decode
set -e

SRC=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$SRC/fuzz/out}
CC=${CC:-clang}
SAN=${SAN:--fsanitize=address,undefined -fno-sanitize-recover=undefined}
mkdir -p "$OUT/obj/dsp/generated" "$OUT/obj/dsp/helpers"

CPPFLAGS="-I$SRC -D_POSIX_C_SOURCE=200112L -D_DEFAULT_SOURCE -DMODES_DUMP1090_VERSION=\"fuzz\" -DMODES_DUMP1090_VARIANT=\"dump1090-fa\""
case "$(uname -m)" in
    x86_64|amd64) MIX=x86; CPPFLAGS="$CPPFLAGS -DSTARCH_MIX_X86" ;;
    aarch64|arm64) MIX=aarch64; CPPFLAGS="$CPPFLAGS -DSTARCH_MIX_AARCH64" ;;
    arm*) MIX=arm; CPPFLAGS="$CPPFLAGS -DSTARCH_MIX_ARM" ;;
    *) MIX=generic; CPPFLAGS="$CPPFLAGS -DSTARCH_MIX_GENERIC" ;;
esac
CFLAGS="-std=c11 -O1 -g -fno-omit-frame-pointer $SAN -fsanitize=fuzzer-no-link"

OBJS=""
for f in mode_ac mode_s comm_b crc stats cpr icao_filter track util ais_charset anet demod_2400 convert cpu adaptive sdr_stub; do
    $CC $CPPFLAGS $CFLAGS -c "$SRC/$f.c" -o "$OUT/obj/$f.o"
    OBJS="$OBJS $OUT/obj/$f.o"
done
$CC $CPPFLAGS $CFLAGS -c "$SRC/dsp/helpers/tables.c" -o "$OUT/obj/dsp/helpers/tables.o"
OBJS="$OBJS $OUT/obj/dsp/helpers/tables.o"

# starch-generated DSP flavours for this architecture (see dsp/generated/makefile.$MIX)
STARCH_SRCS=$(sed -n 's/^STARCH_OBJS *:= *//p; s/^STARCH_OBJS *+= *//p' "$SRC/dsp/generated/makefile.$MIX" | tr ' ' '\n' | grep '\.o$' | sed 's/\.o$/.c/')
for s in $STARCH_SRCS; do
    extra=$(grep "STARCH_COMPILE.* $s " "$SRC/dsp/generated/makefile.$MIX" | tr ' ' '\n' | grep '^-' | grep -v '^-o$' | tr '\n' ' ')
    o="$OUT/obj/$(echo "$s" | sed 's/\.c$/.o/')"
    $CC $CPPFLAGS $CFLAGS $extra -c "$SRC/$s" -o "$o"
    OBJS="$OBJS $o"
done

for h in fuzz_decode fuzz_net fuzz_demod; do
    $CC $CPPFLAGS $CFLAGS -c "$SRC/fuzz/$h.c" -o "$OUT/obj/$h.o"
    $CC $CPPFLAGS $CFLAGS -c "$SRC/fuzz/fuzz_common.c" -o "$OUT/obj/fuzz_common.o"
    $CC $CFLAGS -fsanitize=fuzzer -o "$OUT/$h" "$OUT/obj/$h.o" "$OUT/obj/fuzz_common.o" $OBJS -lm -lpthread
done
echo "built: $OUT/fuzz_decode $OUT/fuzz_net $OUT/fuzz_demod"
