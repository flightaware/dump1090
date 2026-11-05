# Decision SNR, magnitude vs coherent, on accepted frames (true bits known), first 56 bits
import sys, numpy as np
sys.argv = [sys.argv[0].replace('coh_snr.py', 'coh_acq.py')] + sys.argv[1:]
src = open(sys.argv[0]).read().split("count = int(")[0]          # reuse acquire()/slice_fixed()
exec(src)
count = int(sys.argv[2]) if len(sys.argv) > 2 else 6000
d = cs.load(sys.argv[1], count=count)
acc = d['accepted']
x, good = d['x'][acc].astype(np.complex128), d['good_bits'][acc]
n = len(x)
w, h, rho2 = acquire(x, np.full(n, 56))
xa, xb = x[:, 16:16 + 112:2], x[:, 17:17 + 112:2]
b = good[:, :56].astype(bool)
on = np.where(b, xa, xb); off = np.where(b, xb, xa)
# magnitude decision
Dm = np.abs(on) - np.abs(off)
# coherent decision: de-rotate each chip by its own time, project on the channel
i = np.arange(56)
ka = 2 * i; kb = 2 * i + 1                                        # chip index from bit 0
ra = np.exp(-1j * w[:, None] * ka[None, :]); rb_ = np.exp(-1j * w[:, None] * kb[None, :])
ya = np.real(xa * ra * np.conj(h)[:, None]); yb = np.real(xb * rb_ * np.conj(h)[:, None])
Dc = np.where(b, ya - yb, yb - ya) / (np.abs(h)[:, None] + 1e-30)
snr = lambda D: D.mean(1) ** 2 / (D.var(1) + 1e-30)
gain = 10 * np.log10(snr(Dc) / snr(Dm))
level = 10 * np.log10(np.mean(np.abs(on) ** 2, 1) / (2048.0 * 128) ** 2 + 1e-30)  # rough dBFS (filter gain 128)
c = rho2 > 0.6
print(f"{c.sum()} coherent accepted frames; decision SNR gain coherent vs magnitude (dB):")
print(f"  all: median {np.median(gain[c]):+.2f}, mean {np.mean(gain[c]):+.2f}")
for lo, hi in [(-40, -24), (-24, -18), (-18, -12), (-12, -6), (-6, 3)]:
    m = c & (level >= lo) & (level < hi)
    if m.sum() >= 10:
        print(f"  signal {lo:4d} .. {hi:3d} dBFS: {m.sum():5d} frames, median gain {np.median(gain[m]):+.2f} dB"
              f"  (magnitude decision SNR {10*np.log10(np.median(snr(Dm)[m])):5.1f} dB)")

# Marginal frames: failed candidates whose coherent bits pass the CRC (truth = coherent bits)
known = set(d['addr'][acc].tolist())
xf, mbf = d['x'][~acc].astype(np.complex128), d['mag_bits'][~acc]
nf = len(xf)
wf, hf, rf = acquire(xf, np.full(nf, 56))
cb = slice_fixed(xf, wf, hf)
ok = np.array([judge(bb, known)[0] for bb in cb]) & ~np.array([judge(bb, known)[0] for bb in mbf]) & (rf > 0.6)
sel = np.where(ok)[0]
x2, b2 = xf[sel], cb[sel, :56].astype(bool)
xa, xb = x2[:, 16:16 + 112:2], x2[:, 17:17 + 112:2]
on = np.where(b2, xa, xb); off = np.where(b2, xb, xa)
Dm = np.abs(on) - np.abs(off)
w2, h2 = wf[sel], hf[sel]
ya = np.real(xa * np.exp(-1j * w2[:, None] * (2 * i)[None, :]) * np.conj(h2)[:, None])
yb = np.real(xb * np.exp(-1j * w2[:, None] * (2 * i + 1)[None, :]) * np.conj(h2)[:, None])
Dc = np.where(b2, ya - yb, yb - ya) / (np.abs(h2)[:, None] + 1e-30)
g = 10 * np.log10(snr(Dc) / snr(Dm))
errm = (Dm < 0).sum(1); errc = (Dc < 0).sum(1)
print(f"\n{len(sel)} rescued (marginal) frames: decision SNR gain median {np.median(g):+.2f} dB, mean {np.mean(g):+.2f} dB;"
      f" magnitude decision SNR median {10*np.log10(np.median(snr(Dm))):.1f} dB")
print(f"  wrong bits in the first 56: magnitude {errm.sum()} total ({np.mean(errm):.2f}/frame), coherent {errc.sum()}")
