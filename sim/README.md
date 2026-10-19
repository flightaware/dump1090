# DEMOD_2400X test tools

* `ls_coeffs.py fmax gain`: designs the 5-phase upsampler table in
  `dsp/helpers/demod2400x_coeffs.h` (least-squares fractional delay, flat to
  `fmax` MHz, every phase summing to `gain`) and prints `UPSAMPLE_ABS_SUM`.
  It exits with an error if a tap doesn't fit in int8. The current table is
  `python3 ls_coeffs.py 1.0 128`. Needs numpy.
* `gen_timing.c`: synthetic DF17 frames at known 12 MHz ticks as an SC16 file,
  for sensitivity and timestamp tests. Needs liquid-dsp:
  `gcc -O2 -o gen_timing gen_timing.c -lliquid -lm`.
* `tscheck.py`: checks the timestamps dump1090 reports for a `gen_timing` file.

Example, 2000 weak frames with a random +-100 kHz carrier offset:

```
./gen_timing weak.sc16 2000 300 100000
../dump1090 --ifile weak.sc16 --iformat SC16 --quiet --stats    # accepted frames
../dump1090 --ifile weak.sc16 --iformat SC16 --raw --mlat | python3 tscheck.py /dev/stdin
```
