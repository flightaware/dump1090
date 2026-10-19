# Least-squares fractional-delay upsampler phases, every phase summing to exactly 'gain'.
# Usage: ls_coeffs.py fmax_mhz gain   (exits with an error if any |tap| > 127)
import sys, numpy as np
fmax, G = float(sys.argv[1]), int(sys.argv[2])
k = np.arange(8)
w = 2 * np.pi * np.linspace(0, fmax, 400) / 2.4
rows = []
for p in range(5):
    t = 3.1 + 0.2 * p                                    # delay in input samples
    A = np.exp(-1j * np.outer(w, k)); d = np.exp(-1j * w * t)
    A2 = np.vstack([A.real, A.imag, 1000 * np.ones((1, 8))])   # heavy row: DC gain 1
    b2 = np.concatenate([d.real, d.imag, [1000]])
    h = np.linalg.lstsq(A2, b2, rcond=None)[0] * G
    lo, hi = -2.0, 2.0                                   # common offset so the rounded taps sum to G
    for _ in range(60):
        m = 0.5 * (lo + hi); q = np.round(h + m).astype(int)
        if q.sum() == G: break
        if q.sum() > G: hi = m
        else: lo = m
    if q.sum() != G:
        sys.exit(f"phase {p}: cannot round to sum {G}")
    if np.abs(q).max() > 127:
        sys.exit(f"phase {p}: tap {np.abs(q).max()} exceeds 127 at gain {G}")
    rows.append(q)
def resp(h, f): return np.sum(h * np.exp(-1j * 2 * np.pi * f / 2.4 * k))
def gd(h, f, df=0.01):
    a = np.angle(resp(h, f + df)) - np.angle(resp(h, f - df)); a = (a + np.pi) % (2 * np.pi) - np.pi
    return -a / (2 * np.pi * 2 * df / 2.4)
err = max(abs(gd(r, f) - (3.1 + 0.2 * p)) for p, r in enumerate(rows) for f in (0.0001, 0.4, 0.8))
ng = sum((r.astype(float) ** 2).sum() for r in rows) / 5 / G ** 2
print(f"// LS fmax={fmax} gain={G}: max group delay error {err:.3f} samples (0..0.8 MHz), noise gain {ng:.3f}", file=sys.stderr)
print(f"#define UPSAMPLE_ABS_SUM {max(int(abs(r).sum()) for r in rows)}    // largest sum of |taps| over the phases")
for p, r in enumerate(rows):
    print("    {" + ",".join("%4d" % v for v in r) + f" }}, // phase={p} sum={r.sum()}")
