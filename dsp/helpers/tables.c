#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#include "dsp-types.h"
#include "dsp/helpers/tables.h"

const uint16_t * get_uc8_mag_table()
{
    static uint16_t *table = NULL;

    if (!table) {
        table = malloc(sizeof(uint16_t) * 256 * 256);
        if (!table) {
            fprintf(stderr, "can't allocate UC8 conversion lookup table\n");
            abort();
        }

        for (int i = 0; i <= 255; i++) {
            for (int q = 0; q <= 255; q++) {
                float fI, fQ, magsq;

                fI = (i - 127.4) / 128;
                fQ = (q - 127.4) / 128;
                magsq = fI * fI + fQ * fQ;

                float mag = round(sqrtf(magsq) * 65536.0f);
                if (mag > 65535)
                    mag = 65535;

                uc8_u16_t u;
                u.uc8.I = i;
                u.uc8.Q = q;
                table[u.u16] = mag;
            }
        }
    }

    return table;
}

const uint16_t * get_sc16q11_mag_11bit_table()
{
    static uint16_t *table = NULL;

    if (!table) {
        table = malloc(sizeof(uint16_t) * 2048 * 2048);
        if (!table) {
            fprintf(stderr, "can't allocate SC16Q11 conversion lookup table\n");
            abort();
        }

        for (int i = 0; i <= 2047; i++) {
            for (int q = 0; q <= 2047; q++) {
                float fI, fQ, magsq;

                fI = i / 2048.0;
                fQ = q / 2048.0;
                magsq = fI * fI + fQ * fQ;

                float mag = round(sqrtf(magsq) * 65536.0f);
                if (mag > 65535)
                    mag = 65535;

                table[(q << 11) | i] = mag;
            }
        }
    }

    return table;
}

const uint16_t * get_sc16q11_mag_12bit_table()
{
    static uint16_t *table = NULL;

    if (!table) {
        table = malloc(sizeof(uint16_t) * 4096 * 4096);
        if (!table) {
            fprintf(stderr, "can't allocate SC16Q11 conversion lookup table\n");
            abort();
        }

        for (int i = -2048; i <= 2047; i++) {
            for (int q = -2048; q <= 2047; q++) {
                float fI, fQ, magsq;

                fI = abs(i) / 2048.0;
                fQ = abs(q) / 2048.0;
                magsq = fI * fI + fQ * fQ;

                float mag = round(sqrtf(magsq) * 65536.0f);
                if (mag > 65535)
                    mag = 65535;

                unsigned index = ((i & 4095) << 12) | (q & 4095);
                table[index] = mag;
            }
        }
    }

    return table;
}

