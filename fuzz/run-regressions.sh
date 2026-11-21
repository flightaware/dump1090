#!/bin/sh
# Build the fuzz harnesses with strict ASan+UBSan (every UBSan check fatal)
# and run them over the regression inputs in fuzz/regressions/ plus any
# extra corpus directories given as arguments. Exits non-zero on failure.
#
#   fuzz/run-regressions.sh [outdir]
set -e
SRC=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$SRC/fuzz/out-regress}
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all" "$SRC/fuzz/build.sh" "$OUT" >/dev/null
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
status=0
for h in decode net demod; do
    dir="$SRC/fuzz/regressions/$h"
    [ -d "$dir" ] || continue
    if "$OUT/fuzz_$h" -runs=0 "$dir" >"$OUT/regress_$h.log" 2>&1; then
        echo "PASS: fuzz_$h on $(ls "$dir" | wc -l) regression input(s)"
    else
        echo "FAIL: fuzz_$h (see $OUT/regress_$h.log)"
        grep -E "ERROR|runtime error" "$OUT/regress_$h.log" | head -5
        status=1
    fi
done
exit $status
