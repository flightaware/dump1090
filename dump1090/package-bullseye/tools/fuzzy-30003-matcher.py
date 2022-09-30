#!/usr/bin/env python3

#
# Regression testing helper: takes a 3.0.5 port-30003 output file
# and a 3.1.0 port-30003 output file and generates a diff, after
# dealing with the known formatting / data differences

import csv
from contextlib import closing

horizon=5

def fuzzy_match_details(l1, l2):
    _, _, type1, _, _, addr1, _, _, _, _, _, cs1, alt1, gs1, hdg1, lat1, lon1, vr1, sq1, change1, emerg1, spi1, aog1 = l1
    _, _, type2, _, _, addr2, _, _, _, _, _, cs2, alt2, gs2, hdg2, lat2, lon2, vr2, sq2, change2, emerg2, spi2, aog2 = l2

    if addr1 != addr2:
        return (False, 'adr')

    if type1 != type2:
        # 3.0.5: reports DF17 surface/airborne with no position as type 7
        # 3.1.0: reports DF17 surface/airborne with no position as type 2/3
        if type1 != '7':
            return (False, 'typ')
        if type2 != '2' and type2 != '3':
            return (False, 'typ')
        if lat1 != '' or lon1 != '':
            return (False, 'typ')

    if alt1 != alt2:
        # 3.0.5: omits altitude in DF17 if no position was decoded
        # 3.1.0: includes it
        if type1 != '7' or alt1 != '' or alt2 == '':
            return (False, 'alt')

    if gs1 != gs2:
        # 3.0.5: truncates computed GS
        # 3.1.0: rounds computed GS
        if gs1 == '' or gs2 == '' or abs(int(gs1) - int(gs2)) > 1:
            return (False, 'gs ')
    if hdg1 != hdg2:
        # 3.0.5: truncates computed heading
        # 3.1.0: rounds computed heading
        if hdg1 == '' or hdg2 == '':
            return (False, 'hdg')
        delta = abs(int(hdg1) - int(hdg2))
        if delta > 180:
            delta = 360 - delta
        if delta > 1:
            return False

    if lat1 != lat2:
        return (False, 'lat')
    if lon1 != lon2:
        return (False, 'lon')
    if vr1 != vr2:
        return (False, 'vr ')

    if sq1 != sq2:
        # 3.0.5: strips leading zeros
        # 3.1.0: preserves leading zeros
        if ('0' + sq1) != sq2 and ('00' + sq1) != sq2 and ('000' + sq1) != sq2:
            return (False, 'sqk')

    # 3.1.0: only reports these when available
    if change1 != change2:
        if change1 != '0' or change2 != '':
            return (False, 'chg')
    if emerg1 != emerg2:
        if emerg1 != '0' or emerg2 != '':
            return (False, 'emg')
    if spi1 != spi2:
        if spi1 != '0' or spi2 != '':
            return (False, 'spi')

    if aog1 != aog2:
        # 3.1.0: different rules for when AOG is reported
        if aog1 != '' and aog2 != '':
            return (False, 'aog')

    return (True, None)

def fuzzy_match(l1, l2):
    return fuzzy_match_details(l1, l2)[0]

def fuzzy_match_reason(l1, l2):
    return fuzzy_match_details(l1, l2)[1]

def next_line(reader, queue):
    if queue:
        return queue.pop()
    line = next(reader, None)
    if line is None or len(line) == 0:
        return None
    else:
        return [reader.line_num] + line

def unpush_line(queue, line):
    queue.insert(0, line)

def csv_diff(path1, path2):
    diffs = []
    q1 = []
    q2 = []

    with closing(open(path1, 'r')) as f1, closing(open(path2, 'r')) as f2:
        r1 = csv.reader(f1)
        r2 = csv.reader(f2)

        l1 = next_line(r1, q1)
        l2 = next_line(r2, q2)

        while (l1 is not None) or (l2 is not None):
            if l1 is None:
                yield ('+', None, l2)
                l2 = next_line(r2, q2)
                continue

            if l2 is None:
                yield ('-', l1, None)
                l1 = next_line(r1, q1)
                continue

            if fuzzy_match(l1, l2):
                yield (' ', l1, l2)
                l1 = next_line(r1, q1)
                l2 = next_line(r2, q2)
                continue

            #print('mismatch:', l1, l2)

            save_1 = []
            save_2 = []

            found = False
            for i in range(horizon):
                next_l2 = next_line(r2, q2)
                if next_l2 is not None:
                    if fuzzy_match(l1, next_l2):
                        # skip l2 and any lines in save_2
                        # continue with l1 and next_l2
                        yield('+', None, l2)
                        for l in save_2:
                            yield('+', None, l)
                        l2 = next_l2
                        q1.extend(reversed(save_1))
                        found = True
                        break
                    else:
                        save_2.append(next_l2)

                next_l1 = next_line(r1, q1)
                if next_l1 is not None:
                    if fuzzy_match(next_l1, l2):
                        # skip l1 and any lines in save_1
                        # continue with next_l1 and l2
                        yield('-', l1, None)
                        for l in save_1:
                            yield('-', l, None)
                        l1 = next_l1
                        q2.extend(reversed(save_2))
                        found = True
                        break
                    else:
                        save_1.append(next_l1)

            if found:
                #print('new l1:', l1)
                #print('new l2:', l2)
                #print('new q1:')
                #for q in q1: print(q)
                #print('new q2:')
                #for q in q2: print(q)
                continue

            #print('lookahead: nothing likely')

            q1.extend(reversed(save_1))
            q2.extend(reversed(save_2))
            yield ('*', l1, l2)
            l1 = next_line(r1, q1)
            l2 = next_line(r1, q2)

def format_line(line):
    line_num = line[0]
    subrow = line[1:3] + line[5:6] + line[11:]
    return str(line_num) + ': ' + ','.join(subrow)

if __name__ == '__main__':
    import sys
    for action, old, new in csv_diff(sys.argv[1], sys.argv[2]):
        if action == ' ':
            if False: print ('      ' + format_line(new))
        elif action == '*':
            reason = fuzzy_match_reason(old, new)
            print ('< ' + reason + ' ' + format_line(old))
            print ('> ' + reason + ' ' + format_line(new))
        elif action == '-':
            # 3.0.5: emits lines for all-zero messages
            # 3.1.0: doesn't
            if old[5] != '000000':
                print ('-     ' + format_line(old))
        elif action == '+':
            print ('+     ' + format_line(new))
