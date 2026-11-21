#!/bin/bash
#
# Build the current tree and a base git ref, run both on the same synthetic
# traffic, and check that every decoded output is byte-for-byte identical.
# Optionally time both builds on the same input.
#
#   tools/compare-builds.sh [--base REF] [--small] [--timing ROUNDS] [--iq FILE [--iformat FMT]] [--workdir DIR]
#
#   --base REF        git ref to compare against (default: origin/master, else master)
#   --small           smaller inputs (for slow or emulated machines)
#   --timing ROUNDS   also time both builds, alternating, ROUNDS runs each
#   --iq FILE         also compare (and time) on a real 2.4MS/s UC8 IQ recording,
#                     e.g. one made with: rtl_sdr -f 1090000000 -s 2400000 -n 288000000 FILE
#   --iformat FMT     sample format of the --iq recording: UC8 (default, rtl_sdr), SC16 or SC16Q11
#   --workdir DIR     where to build and write data (default: a new temp dir)
#
# Needs: a C toolchain, git, python3 with numpy (for tools/gen-traffic.py).
# The replay driver (fuzz/replay.c) is taken from the current tree and built
# against both trees; it uses a simulated clock, so its output depends only
# on its input.
#
# Exit status is 0 if all outputs match, 1 otherwise.

set -e

TOP=$(cd "$(dirname "$0")/.." && pwd)
BASE=
SMALL=0
ROUNDS=0
IQ_EXTRA=
IQ_FORMAT=UC8
WORK=

while [ $# -gt 0 ]; do
    case "$1" in
        --base) BASE=$2; shift 2 ;;
        --small) SMALL=1; shift ;;
        --timing) ROUNDS=$2; shift 2 ;;
        --iq) IQ_EXTRA=$(realpath "$2"); shift 2 ;;
        --iformat) IQ_FORMAT=$2; shift 2 ;;
        --workdir) WORK=$2; shift 2 ;;
        *) sed -n '2,26p' "$0"; exit 2 ;;
    esac
done

cd "$TOP"
if [ -z "$BASE" ]; then
    if git rev-parse -q --verify origin/master >/dev/null; then BASE=origin/master; else BASE=master; fi
fi
[ -n "$WORK" ] || WORK=$(mktemp -d)
mkdir -p "$WORK"
WORK=$(realpath "$WORK")
MAKEFLAGS_SDR="RTLSDR=no BLADERF=no HACKRF=no LIMESDR=no SOAPYSDR=no"
JOBS=$(nproc 2>/dev/null || echo 2)
export TZ=UTC

log() { echo "=== $*" >&2; }

build_into() {   # build_into SRCDIR BINDIR
    local src=$1 bin=$2
    mkdir -p "$bin"
    (cd "$src" && make -j"$JOBS" $MAKEFLAGS_SDR dump1090 >"$bin/build.log" 2>&1) || { cat "$bin/build.log" >&2; exit 1; }
    (cd "$src" && fuzz/build-replay.sh "$bin/replay" >>"$bin/build.log" 2>&1) || { cat "$bin/build.log" >&2; exit 1; }
    cp "$src/dump1090" "$bin/dump1090"
}

log "building current tree ($(git rev-parse --short HEAD))"
build_into "$TOP" "$WORK/head"

log "building base $BASE ($(git rev-parse --short "$BASE"))"
rm -rf "$WORK/base-src"
git worktree add -f --detach "$WORK/base-src" "$BASE" >/dev/null 2>&1 || {
    # no worktree support (e.g. shallow clone): fall back to an archive
    mkdir -p "$WORK/base-src"; git archive "$BASE" | tar -x -C "$WORK/base-src"; }
mkdir -p "$WORK/base-src/fuzz"
cp fuzz/replay.c fuzz/build-replay.sh "$WORK/base-src/fuzz/"
build_into "$WORK/base-src" "$WORK/base"
git worktree remove --force "$WORK/base-src" >/dev/null 2>&1 || true

log "generating synthetic traffic"
DATA=$WORK/data
mkdir -p "$DATA"
if [ $SMALL = 1 ]; then
    IQ_SECONDS=2.0000005; NOISY_SECONDS=2; BEAST_SECONDS=20; IQ_AC=150; BEAST_AC=300
else
    IQ_SECONDS=10.0000005; NOISY_SECONDS=5; BEAST_SECONDS=60; IQ_AC=300; BEAST_AC=600
fi
# (the odd IQ durations give a sample count that is not a multiple of 8,
#  so the last block also exercises the DSP tail code)
python3 tools/gen-traffic.py --aircraft $IQ_AC --seconds $IQ_SECONDS --seed 1 --iq "$DATA/clean.iq" 2>/dev/null
python3 tools/gen-traffic.py --aircraft 200 --seconds $NOISY_SECONDS --seed 3 --noise 10 --iq "$DATA/noisy.iq" 2>/dev/null
python3 tools/gen-traffic.py --aircraft $BEAST_AC --seconds $BEAST_SECONDS --seed 2 --beast "$DATA/traffic.beast" 2>/dev/null

# name, replay arguments
CONFIGS=(
    "clean_fix1|--fix 1 --lat 45 --lon 10 --iq $DATA/clean.iq"
    "clean_fix2_modeac|--fix 2 --modeac --lat 45 --lon 10 --iq $DATA/clean.iq"
    "noisy_fix0|--fix 0 --lat 45 --lon 10 --iq $DATA/noisy.iq"
    "noisy_fix1|--fix 1 --lat 45 --lon 10 --iq $DATA/noisy.iq"
    "noisy_fix2_modeac|--fix 2 --modeac --iq $DATA/noisy.iq"
    "beast_fix1|--fix 1 --lat 45 --lon 10 --beast $DATA/traffic.beast"
)
if [ -n "$IQ_EXTRA" ]; then
    CONFIGS+=("recording_fix1|--fix 1 --iformat $IQ_FORMAT --iq $IQ_EXTRA"
              "recording_fix2_modeac|--fix 2 --modeac --iformat $IQ_FORMAT --iq $IQ_EXTRA")
fi

checksums() {   # checksums BINDIR OUTFILE
    local bin=$1 out=$2 t
    : > "$out"
    for c in "${CONFIGS[@]}"; do
        local name=${c%%|*} args=${c#*|}
        t=$(mktemp -d "$WORK/run.XXXXXX")
        # shellcheck disable=SC2086
        "$bin/replay" $args "$t" >/dev/null 2>&1
        for f in $(ls "$t" | sort); do
            echo "$name/$f $(sha256sum < "$t/$f" | cut -d' ' -f1) $(wc -c < "$t/$f")" >> "$out"
        done
        # keep the decoded-message text of Mode A/C runs, to explain differences below
        case "$args" in *--modeac*) mkdir -p "$bin/out/$name"; mv "$t/messages.txt" "$bin/out/$name/" ;; esac
        rm -rf "$t"
    done
    for fix in --no-fix --fix --fix-2bit; do
        echo "dump1090_raw$fix $("$bin/dump1090" --device-type ifile --ifile "$DATA/noisy.iq" --iformat UC8 --raw $fix 2>/dev/null | sha256sum | cut -d' ' -f1)" >> "$out"
    done
}

log "running both builds"
checksums "$WORK/base" "$WORK/base.sums"
checksums "$WORK/head" "$WORK/head.sums"

# Before the commit "Reset the message for each Mode A/C reply in a sample buffer", a Mode
# A/C reply without a valid Mode C code showed the altitude of the previous
# reply. Comparing against an older base on real recordings (which contain
# Mode A/C replies) therefore differs in exactly that way: lines
# "  Baro altitude: ..." present only in the base output. Recognise that
# pattern so it is reported as expected rather than as a regression.
expected_modeac_diff() {   # expected_modeac_diff NAME (e.g. recording_fix2_modeac/messages.txt)
    local cfg=${1%/messages.txt}
    [ "$cfg" != "$1" ] || return 1
    local b="$WORK/base/out/$cfg/messages.txt" h="$WORK/head/out/$cfg/messages.txt"
    [ -f "$b" ] && [ -f "$h" ] || return 1
    local d
    d=$(diff "$b" "$h" | grep '^[<>]') || return 1
    # only removals of altitude lines, and each removed line belongs to a Mode A/C reply
    echo "$d" | grep -qv '^<   Baro altitude: ' && return 1
    python3 - "$b" "$h" <<'PYEOF'
import difflib, sys
base = open(sys.argv[1]).read().split('\n')
head = open(sys.argv[2]).read().split('\n')
ok = True
for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, base, head, autojunk=False).get_opcodes():
    if tag == 'equal':
        continue
    if tag != 'delete':
        ok = False; break
    for i in range(i1, i2):
        # walk back to the message header line
        k = i
        while k > 0 and not base[k].startswith('DF:'):
            k -= 1
        if not base[k].startswith('DF:32 Mode A/C Reply'):
            ok = False
sys.exit(0 if ok else 1)
PYEOF
}

STATUS=0
EXPECTED=0
RESULT="$WORK/result.md"
{
    echo "### Output comparison: $(git rev-parse --short HEAD) vs $BASE ($(git rev-parse --short "$BASE")) on $(uname -m)"
    echo
    echo "| output | bytes | result |"
    echo "|---|---|---|"
    while read -r name sum size; do
        other=$(grep "^$name " "$WORK/base.sums" | cut -d' ' -f2)
        if [ "$sum" = "$other" ]; then
            r=identical
        elif expected_modeac_diff "$name"; then
            r="differs as expected (Mode A/C altitude fix, $(diff "$WORK/base/out/${name%/messages.txt}/messages.txt" "$WORK/head/out/${name%/messages.txt}/messages.txt" | grep -c '^<') stale altitudes removed)"
            echo "$name" >> "$WORK/expected.list"
        else
            r="**DIFFERENT**"
        fi
        echo "| $name | ${size:--} | $r |"
    done < "$WORK/head.sums"
} > "$RESULT"
# any difference not explained above is a failure (sizes are informational)
UNEXPECTED=$(diff <(cut -d' ' -f1,2 "$WORK/base.sums") <(cut -d' ' -f1,2 "$WORK/head.sums") | grep '^>' | cut -d' ' -f2 |
             grep -vxF -f <(cat "$WORK/expected.list" 2>/dev/null; echo "-none-") || true)
[ -n "$UNEXPECTED" ] && STATUS=1
[ -s "$WORK/expected.list" ] && EXPECTED=$(wc -l < "$WORK/expected.list")
if [ $STATUS = 0 ] && [ "$EXPECTED" = 0 ]; then
    echo -e "\nAll $(wc -l < "$WORK/head.sums") outputs identical." >> "$RESULT"
elif [ $STATUS = 0 ]; then
    echo -e "\nAll $(wc -l < "$WORK/head.sums") outputs identical, apart from $EXPECTED that differ only by the intended Mode A/C altitude fix." >> "$RESULT"
else
    echo -e "\n**Outputs differ.** Inputs and both builds are in $WORK." >> "$RESULT"
fi

if [ "$ROUNDS" -gt 0 ]; then
    log "timing ($ROUNDS rounds per build, alternating)"
    TIMING_IQ=$DATA/clean.iq
    [ -n "$IQ_EXTRA" ] && TIMING_IQ=$IQ_EXTRA
    TIMING_FMT=UC8
    [ -n "$IQ_EXTRA" ] && TIMING_FMT=$IQ_FORMAT
    python3 - "$WORK" "$ROUNDS" "$TIMING_IQ" "$DATA/traffic.beast" "$TIMING_FMT" >> "$RESULT" <<'EOF'
import os, resource, shutil, statistics, subprocess, sys, tempfile
work, rounds, iq, beast, fmt = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
loads = {
    'dump1090 --ifile (IQ)': lambda b: [f'{b}/dump1090', '--device-type', 'ifile', '--ifile', iq, '--iformat', fmt, '--quiet', '--fix'],
    'Beast input, all outputs': lambda b, o: [f'{b}/replay', '--quiet', '--fix', '1', '--beast', beast, o],
}
def cpu(argv):
    r0 = resource.getrusage(resource.RUSAGE_CHILDREN)
    subprocess.run(argv, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    r1 = resource.getrusage(resource.RUSAGE_CHILDREN)
    return (r1.ru_utime - r0.ru_utime) + (r1.ru_stime - r0.ru_stime)
print('\n### Timing (CPU seconds, median of %d alternating runs each)\n' % rounds)
print('| workload | base | current | change |')
print('|---|---|---|---|')
for name, mk in loads.items():
    t = {'base': [], 'head': []}
    for i in range(rounds):
        for b in (('base', 'head') if i % 2 == 0 else ('head', 'base')):
            out = tempfile.mkdtemp(dir=work)
            argv = mk(f'{work}/{b}') if name.startswith('dump1090') else mk(f'{work}/{b}', out)
            t[b].append(cpu(argv))
            shutil.rmtree(out, ignore_errors=True)
    mb, mh = statistics.median(t['base']), statistics.median(t['head'])
    print(f'| {name} | {mb:.3f} | {mh:.3f} | {100 * (mh - mb) / mb:+.1f}% |')
EOF
fi

cat "$RESULT"
[ -n "$GITHUB_STEP_SUMMARY" ] && cat "$RESULT" >> "$GITHUB_STEP_SUMMARY"
exit $STATUS
