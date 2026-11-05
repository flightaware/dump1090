# Constant-frequency coherent decode (crystal transmitters): a correlator bank over
# frequency on the pair sums gives the frequency and the complex channel h; no tracking.
# This is the model of the first version; the kernel now takes the larger chip of each
# bit instead of the pair sum (see coh_larger.py).
# Window: 56 bits, or 112 when the magnitude quality2 says the frame is long.
# Usage: coh_acq.py chips.bin [records]
import sys, numpy as np
import os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coh_samples as cs
exec(open(sys.argv[0].replace('coh_acq.py', 'coh_judge.py')).read())

WMAX = 2 * np.pi * 62.5e3 / 2e6                       # radians per chip

def acquire(x, nbits, bins=65):
    """Correlator bank: C(w) = sum_i (x_a + x_b e^{-jw}) e^{-j2wi} over nbits; best bin + parabola."""
    n = len(x)
    xa, xb = x[:, 16::2], x[:, 17::2]
    i = np.arange(112)
    win = (i[None, :] < nbits[:, None])
    ws = np.linspace(-WMAX, WMAX, bins)
    P = np.empty((n, bins)); C = np.empty((n, bins), dtype=np.complex128)
    for j, w in enumerate(ws):
        c = (xa + xb * np.exp(-1j * w)) * np.exp(-1j * 2 * w * i)[None, :]
        C[:, j] = (c * win).sum(1)
        P[:, j] = np.abs(C[:, j]) ** 2
    b = P.argmax(1); r = np.arange(n); step = ws[1] - ws[0]
    bm, bp = np.clip(b - 1, 0, bins - 1), np.clip(b + 1, 0, bins - 1)
    den = P[r, bm] - 2 * P[r, b] + P[r, bp]
    ok = (b > 0) & (b < bins - 1) & (den < 0)
    w = ws[b] + np.where(ok, 0.5 * step * (P[r, bm] - P[r, bp]) / np.where(ok, den, -1), 0)
    c = (xa + xb * np.exp(-1j * w)[:, None]) * np.exp(-1j * 2 * w[:, None] * i[None, :]) * win
    h = c.sum(1)                                       # channel at bit 0 (complex gain)
    rho2 = np.abs(h) ** 2 / (nbits * (np.abs(c) ** 2).sum(1) + 1e-30)
    return w, h, rho2

def slice_fixed(x, w, h):
    kk = np.arange(cs.CHIPS) - 16
    y = np.real(x * np.exp(-1j * w[:, None] * kk[None, :]) * np.conj(h)[:, None])
    return cs.slice_bits(y)

count = int(sys.argv[2]) if len(sys.argv) > 2 else 6000
d = cs.load(sys.argv[1], count=count)
acc = d['accepted']; known = set(d['addr'][acc].tolist())
xg, good, q2g = d['x'][acc].astype(np.complex128), d['good_bits'][acc], d['quality2'][acc]
df = good[:, :5].dot(1 << np.arange(4, -1, -1))
mask = np.arange(112)[None, :] < np.where(df >= 16, 112, 56)[:, None]
em = ((cs.slice_bits(np.abs(xg)) != good) & mask).sum(1)
xf, mbf, q2f = d['x'][~acc].astype(np.complex128), d['mag_bits'][~acc], d['quality2'][~acc]
mag_ok = np.array([judge(b, known)[0] for b in mbf])

for label, q2_gate in [('56 bits', 999), ('112 if quality2>=52', 52), ('112 if quality2>=80', 80)]:
    nbg = np.where(q2g >= q2_gate, 112, 56); nbf = np.where(q2f >= q2_gate, 112, 56)
    w, h, rho2 = acquire(xg, nbg)
    e = ((slice_fixed(xg, w, h) != good) & mask).sum(1)
    wf, hf, rf = acquire(xf, nbf)
    ok = np.array([judge(b, known)[0] for b in slice_fixed(xf, wf, hf)])
    for g in (0.5, 0.6):
        c = rho2 > g
        print(f"{label:20s} rho2>{g}: good frames ({c.mean():.0%}) coherent 0/1/2+ {(e[c]==0).sum()}/{(e[c]==1).sum()}/{(e[c]>=2).sum()}"
              f" (magnitude {(em[c]==0).sum()}/{(em[c]==1).sum()}/{(em[c]>=2).sum()})"
              f" | failed: {(rf > g).sum()} tried, {(ok & ~mag_ok & (rf > g)).sum()} rescued only by coherent")

# FFT version: plain pair sums (second chip not rotated), zero-padded FFT, bins within +-62.5 kHz
def acquire_fft(x, nfft=256):
    n = len(x); i = np.arange(56)
    xa, xb = x[:, 16:16 + 112:2], x[:, 17:17 + 112:2]
    F = np.fft.fft(xa + xb, nfft, axis=1)                 # pair sums at 1 MHz
    fb = np.fft.fftfreq(nfft, d=1e-6)                     # Hz
    keep = np.where(np.abs(fb) <= 62.5e3)[0]
    keep = keep[np.argsort(fb[keep])]
    P = np.abs(F[:, keep]) ** 2
    b = P.argmax(1); r = np.arange(n); nb = len(keep)
    bm, bp = np.clip(b - 1, 0, nb - 1), np.clip(b + 1, 0, nb - 1)
    den = P[r, bm] - 2 * P[r, b] + P[r, bp]
    ok = (b > 0) & (b < nb - 1) & (den < 0)
    f = fb[keep][b] + np.where(ok, 0.5 * (1e6 / nfft) * (P[r, bm] - P[r, bp]) / np.where(ok, den, -1), 0)
    w = 2 * np.pi * f / 2e6                               # radians per chip
    c = (xa + xb * np.exp(-1j * w)[:, None]) * np.exp(-1j * 2 * w[:, None] * i[None, :])
    h = c.sum(1)
    rho2 = np.abs(h) ** 2 / (56 * (np.abs(c) ** 2).sum(1) + 1e-30)
    return w, h, rho2

w, h, rho2 = acquire_fft(xg)
e = ((slice_fixed(xg, w, h) != good) & mask).sum(1)
wf, hf, rf = acquire_fft(xf)
ok = np.array([judge(b, known)[0] for b in slice_fixed(xf, wf, hf)])
for g in (0.5, 0.6):
    c = rho2 > g
    print(f"{'FFT 256, 56 bits':20s} rho2>{g}: good frames ({c.mean():.0%}) coherent 0/1/2+ {(e[c]==0).sum()}/{(e[c]==1).sum()}/{(e[c]>=2).sum()}"
          f" (magnitude {(em[c]==0).sum()}/{(em[c]==1).sum()}/{(em[c]>=2).sum()})"
          f" | failed: {(rf > g).sum()} tried, {(ok & ~mag_ok & (rf > g)).sum()} rescued only by coherent")
