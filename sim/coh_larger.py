# Carrier sample of each bit: the pair sum a + b (first version) against the chip with
# the larger magnitude, a if |a|^2 >= |b|^2 else b, used as it is (complex), as the
# kernel now does. Same method otherwise: 7 blocks of 8, 33 bins, parabola, exact
# correlation, rho2 gate, fixed-rotation slicing. Usage: coh_larger.py chips.bin [records]
import sys, numpy as np
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coh_samples as cs
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'coh_judge.py')).read())
WMAX = 2 * np.pi * 62.5e3 / 2e6

def acquire(x, mode):
    n = len(x); i = np.arange(56)
    xa, xb = x[:, 16:16 + 112:2], x[:, 17:17 + 112:2]
    pick = np.abs(xa) >= np.abs(xb)
    c0 = xa + xb if mode == 'sum' else np.where(pick, xa, xb)
    B = c0.reshape(n, 7, 8).sum(2)
    tb = (np.arange(7) * 8 + 3.5) * 2
    ws = np.linspace(-WMAX, WMAX, 33)
    P = np.abs(B @ np.exp(-1j * np.outer(tb, ws))) ** 2
    b = P.argmax(1); r = np.arange(n); step = ws[1] - ws[0]
    bm, bp = np.clip(b - 1, 0, 32), np.clip(b + 1, 0, 32)
    den = P[r, bm] - 2 * P[r, b] + P[r, bp]
    ok = (b > 0) & (b < 32) & (den < 0)
    w = ws[b] + np.where(ok, 0.5 * step * (P[r, bm] - P[r, bp]) / np.where(ok, den, -1), 0)
    xbr = xb * np.exp(-1j * w)[:, None]
    c = (xa + xbr) if mode == 'sum' else np.where(pick, xa, xbr)
    cc = c * np.exp(-1j * 2 * w[:, None] * i[None, :])
    h = cc.sum(1)
    rho2 = np.abs(h) ** 2 / (56 * (np.abs(cc) ** 2).sum(1) + 1e-30)
    return w, h, rho2

def slice_fixed(x, w, h):
    kk = np.arange(cs.CHIPS) - 16
    y = np.real(x * np.exp(-1j * w[:, None] * kk[None, :]) * np.conj(h)[:, None])
    return cs.slice_bits(y)

count = int(sys.argv[2]) if len(sys.argv) > 2 else 6000
d = cs.load(sys.argv[1], count=count)
acc = d['accepted']; known = set(d['addr'][acc].tolist())
xg, good = d['x'][acc].astype(np.complex128), d['good_bits'][acc]
df = good[:, :5].dot(1 << np.arange(4, -1, -1))
mask = np.arange(112)[None, :] < np.where(df >= 16, 112, 56)[:, None]
xf, mbf = d['x'][~acc].astype(np.complex128), d['mag_bits'][~acc]
mag_ok = np.array([judge(b, known)[0] for b in mbf])
print(f"{acc.sum()} accepted frames, {(~acc).sum()} failed candidates")
for mode in ('sum', 'larger'):
    w, h, rho2 = acquire(xg, mode)
    e = ((slice_fixed(xg, w, h) != good) & mask).sum(1)
    wf, hf, rf = acquire(xf, mode)
    ok = np.array([judge(b, known)[0] for b in slice_fixed(xf, wf, hf)]) & ~mag_ok
    print(f"{mode}: rescued with no gate: {ok.sum()}")
    for g in (0.5, 0.6, 0.7, 0.8):
        c = rho2 > g
        print(f"  rho2>{g}: accepted frames coherent {c.mean():4.0%}, 2+ bit errors {(e[c] >= 2).sum():4d} | failed: tried {(rf > g).sum():6d}, rescued {(ok & (rf > g)).sum():4d}")
