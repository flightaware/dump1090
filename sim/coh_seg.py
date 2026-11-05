# Segmented estimates: instead of one frequency and channel from the first 56 bits used
# for all 112, split the frame into segments of 56, 28 or 14 bits and estimate each one
# on its own (nothing shared), so a short frame only ever uses its own bits. Also the
# block length L of the coarse search (8 now; 4 halves the frequency-edge loss and costs
# twice the bin work), and a hybrid: frequency from the first 56 bits, channel (phase)
# per segment. Larger-chip carrier samples throughout, as the kernel.
# Every variant uses the same frame selection (the kernel's rho2 gate, first 56 bits,
# L = 8), so only the slicing differs. Usage: coh_seg.py chips.bin [records]
import sys, numpy as np
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coh_samples as cs
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'coh_judge.py')).read())
WMAX = 2 * np.pi * 62.5e3 / 2e6     # rad per chip

def carrier(x, b0, b1, w=None):
    """Larger-chip carrier samples of bits b0..b1-1, and whether a was picked; with w,
    de-rotated (second chip by w for its half-bit delay, bit i by 2 w i)."""
    xa, xb = x[:, 16 + 2 * b0:16 + 2 * b1:2], x[:, 17 + 2 * b0:17 + 2 * b1:2]
    pick = np.abs(xa) >= np.abs(xb)
    if w is None:
        return np.where(pick, xa, xb)
    i = np.arange(b0, b1)
    c = np.where(pick, xa, xb * np.exp(-1j * w)[:, None])
    return c * np.exp(-1j * 2 * w[:, None] * i[None, :])

def freq(x, b0, b1, L, nbins=33):
    """Coarse search over blocks of L bits, parabola refinement: w per frame."""
    n = len(x); nb = (b1 - b0) // L
    B = carrier(x, b0, b0 + nb * L).reshape(n, nb, L).sum(2)
    tb = (b0 + np.arange(nb) * L + (L - 1) / 2) * 2
    ws = np.linspace(-WMAX, WMAX, nbins)
    P = np.abs(B @ np.exp(-1j * np.outer(tb, ws))) ** 2
    b = P.argmax(1); r = np.arange(n); step = ws[1] - ws[0]
    bm, bp = np.clip(b - 1, 0, nbins - 1), np.clip(b + 1, 0, nbins - 1)
    den = P[r, bm] - 2 * P[r, b] + P[r, bp]
    ok = (b > 0) & (b < nbins - 1) & (den < 0)
    return ws[b] + np.where(ok, 0.5 * step * (P[r, bm] - P[r, bp]) / np.where(ok, den, -1), 0)

def channel(x, b0, b1, w):
    cc = carrier(x, b0, b1, w)
    h = cc.sum(1)
    return h, np.abs(h) ** 2 / ((b1 - b0) * (np.abs(cc) ** 2).sum(1) + 1e-30)

def slice_seg(x, b0, b1, w, h, out):
    kk = np.arange(16 + 2 * b0, 16 + 2 * b1) - 16
    seg = x[:, 16 + 2 * b0:16 + 2 * b1]
    y = np.real(seg * np.exp(-1j * w[:, None] * kk[None, :]) * np.conj(h)[:, None])
    out[:, b0:b1] = y[:, 0::2] > y[:, 1::2]

def decode(x, seg, L, wseg=None):
    """seg: segment length in bits; L: block length; wseg: bits per frequency estimate
    (None = same as seg, i.e. fully independent segments)."""
    out = np.zeros((len(x), 112), np.uint8)
    wseg = wseg or seg
    for f0 in range(0, 112, wseg):
        w = freq(x, f0, f0 + wseg, L)
        for b0 in range(f0, f0 + wseg, seg):
            h, _ = channel(x, b0, b0 + seg, w)
            slice_seg(x, b0, b0 + seg, w, h, out)
    return out

VARIANTS = [   # name, segment bits, L, frequency-estimate bits
    ('now: 56 est, all 112',      None, 8, None),
    ('now, L=4',                  None, 4, None),
    ('2 x 56 independent, L=8',     56, 8, None),
    ('2 x 56 independent, L=4',     56, 4, None),
    ('4 x 28 independent, L=4',     28, 4, None),
    ('4 x 28 independent, L=2',     28, 2, None),
    ('8 x 14 independent, L=2',     14, 2, None),
    ('8 x 14 independent, L=7',     14, 7, None),
    ('freq per 56, phase per 28',   28, 8, 56),
    ('freq per 56, phase per 14',   14, 8, 56),
]

def run(x, name, seg, L, wseg):
    if seg is None:     # the kernel: estimate on bits 0..55, slice all 112 with it
        out = np.zeros((len(x), 112), np.uint8)
        w = freq(x, 0, 56, L); h, _ = channel(x, 0, 56, w)
        slice_seg(x, 0, 112, w, h, out)
        return out
    return decode(x, seg, L, wseg)

count = int(sys.argv[2]) if len(sys.argv) > 2 else 6000
gate = 0.6
d = cs.load(sys.argv[1], count=count)
acc = d['accepted']; known = set(d['addr'][acc].tolist())
xg, good = d['x'][acc].astype(np.complex128), d['good_bits'][acc]
df = good[:, :5].dot(1 << np.arange(4, -1, -1))
long_ = df >= 16
mask = np.arange(112)[None, :] < np.where(long_, 112, 56)[:, None]
xf, mbf = d['x'][~acc].astype(np.complex128), d['mag_bits'][~acc]
mag_ok = np.array([judge(b, known)[0] for b in mbf])
_, rg = channel(xg, 0, 56, freq(xg, 0, 56, 8))
_, rf = channel(xf, 0, 56, freq(xf, 0, 56, 8))
cg, cf = rg > gate, rf > gate
print(f"{acc.sum()} accepted frames ({cg.sum()} coherent), {(~acc).sum()} failed candidates "
      f"({cf.sum()} coherent, rho2 > {gate})")
print(f"{'':28s} {'rescued':>8s} {'(long)':>7s} {'no gate':>8s} | accepted, coherent: 2+ errors  in 0-55  in 56-111")
for name, seg, L, wseg in VARIANTS:
    eg = (run(xg, name, seg, L, wseg) != good) & mask
    jj = [judge(b, known) for b in run(xf, name, seg, L, wseg)]
    ok = np.array([j[0] for j in jj]) & ~mag_ok
    lg = np.array([j[1] >= 16 for j in jj])
    e1, e2 = eg[:, :56].sum(1), eg[:, 56:].sum(1)
    print(f"{name:28s} {(ok & cf).sum():8d} {(ok & cf & lg).sum():7d} {ok.sum():8d} | {(e1 + e2 >= 2)[cg].sum():25d} "
          f"{(e1 >= 2)[cg].sum():8d} {(e2 >= 2)[cg & long_].sum():9d}")
