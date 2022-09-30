#!/usr/bin/env python3

# For each error-correction level (0, 1, or 2-bit errors)
# generate the set of possible DF values that could possibly be
# corrected to DF11/17/18/19 messages, expressed as a bitset.

def popcount(x):
    return bin(x).count('1')

# is the Hamming distance between 'df' and a valid long-message DF no more than `max_errors`?
def correctable_long(df, max_errors):
    if popcount(df ^ 17) <= max_errors: return True
    if popcount(df ^ 18) <= max_errors: return True
    #if popcount(df ^ 19) <= max_errors: return True
    return False

# is the Hamming distance between 'df' and a valid short-message DF no more than `max_errors`?
def correctable_short(df, max_errors):
    if popcount(df ^ 11) <= max_errors: return True
    return False

# Generate a bitset value where bit N is set if predicate(N) is True
def bitset(predicate):
    result = 0
    for i in range(32):
        if predicate(i):
            result |= 1 << i
    return result

shorts = [
    bitset(lambda x: correctable_short(x,0)),
    bitset(lambda x: correctable_short(x,1)),
    bitset(lambda x: correctable_short(x,1))  # deliberately not 2
]

longs = [
    bitset(lambda x: correctable_long(x,0)),
    bitset(lambda x: correctable_long(x,1)),
    bitset(lambda x: correctable_long(x,2))
]

print('static const uint32_t df_correctable_short[MODES_MAX_BITERRORS + 1] = {')
print('    ' + ', '.join(f'0x{i:08x}' for i in shorts))
print('};')

print('static const uint32_t df_correctable_long[MODES_MAX_BITERRORS + 1] = {')
print('    ' + ', '.join(f'0x{i:08x}' for i in longs))
print('};')
