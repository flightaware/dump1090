# Timestamp check for gen_timing.c output: reported 12 MHz timestamp minus the known
# frame start, by sub-sample phase (start tick mod 5). The mean includes a constant
# offset (resampler and filter delays), which doesn't matter for MLAT; it should be
# the same for every phase, with a small spread.
# Usage: dump1090 --ifile t.sc16 --iformat SC16 --raw --mlat | python3 tscheck.py /dev/stdin
import sys, numpy as np
SPACING = 3601
err = {}
for l in open(sys.argv[1]):
    l = l.strip().rstrip(';')
    if not l.startswith('@'): continue
    ts, h = int(l[1:13], 16), l[13:]
    k = int(h[18:22], 16)                        # frame number in msg[9..10]
    err[k] = ts - (k + 1) * SPACING
k = np.array(sorted(err)); e = np.array([err[i] for i in k])
print(f"  decoded {len(k)} frames; err - 768 (end of bit 56): mean {e.mean() - 768:.3f} std {e.std():.3f}")
for p in range(5):
    m = (k + 1) * SPACING % 5 == p
    v, c = np.unique(e[m] - 768, return_counts=True)
    print(f"    start phase {p}: mean {e[m].mean() - 768:+7.3f}  " + "  ".join(f"{a:+d}:{b}" for a, b in zip(v, c)))
