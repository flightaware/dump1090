def crc_syndrome(bits, nb):
    rem = 0
    for b in bits[:nb - 24]:
        top = ((rem >> 23) & 1) ^ int(b)
        rem = (rem << 1) & 0xffffff
        if top: rem ^= 0xfff409
    return rem ^ int(''.join(map(str, bits[nb - 24:nb])), 2)

def judge(bits, known):
    df = int(''.join(map(str, bits[:5])), 2)
    nb = 112 if df >= 16 else 56
    s = crc_syndrome(bits, nb)
    if df in (17, 18): return s == 0, df
    if df == 11: return (s & ~0x7f) == 0, df
    if df in (0, 4, 5, 16, 20, 21): return s in known, df
    return False, df
