#ifndef AIS_CHARSET_H
#define AIS_CHARSET_H

// AIS character set is just the first 64 printable ASCII characters,
// but with 0x20..0x3F after 0x40..0x5F.
static inline __attribute__((always_inline)) char ais_to_ascii(unsigned i) {
  return (i + 0x20) ^ 0b01100000;
}

#endif
