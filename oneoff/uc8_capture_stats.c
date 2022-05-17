/* measures min, max, mean I and Q values in a UC8-format capture */

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <limits.h>

#include "dsp-types.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "need a capture filename\n");
        return 1;
    }

    const unsigned len = 1<<24;
    uc8_t *buffer = malloc(len * sizeof(uc8_t));

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    unsigned all_samples = 0;
    double all_I = 0, all_Q = 0;

    for (;;) {
        ssize_t count = read(fd, buffer, len * sizeof(uc8_t));
        if (count < 0) {
            perror("read");
            close(fd);
            return 1;
        }

        if (count <= 0)
            break;

        unsigned actual_len = count / sizeof(uc8_t);

        int min_I = INT_MAX, max_I = INT_MIN;
        unsigned min_I_count = 0, max_I_count = 0;
        int min_Q = INT_MAX, max_Q = INT_MIN;
        unsigned min_Q_count = 0, max_Q_count = 0;
        double sum_I = 0, sum_Q = 0;

        for (unsigned i = 0; i < actual_len; ++i) {
            int I = buffer[i].I;
            int Q = buffer[i].Q;

            if (I < min_I) {
                min_I = I;
                min_I_count = 0;
            }
            if (I == min_I) {
                ++min_I_count;
            }

            if (Q < min_Q) {
                min_Q = Q;
                min_Q_count = 0;
            }
            if (Q == min_Q) {
                ++min_Q_count;
            }

            if (I > max_I) {
                max_I = I;
                max_I_count = 0;
            }
            if (I == max_I) {
                ++max_I_count;
            }

            if (Q > max_Q) {
                max_Q = Q;
                max_Q_count = 0;
            }
            if (Q == max_Q) {
                ++max_Q_count;
            }

            sum_I += I;
            sum_Q += Q;
        }

        all_I += sum_I;
        all_Q += sum_Q;
        all_samples += actual_len;

        fprintf(stderr,
                "%u samples; I: min %4d (%5u); max %4d (%5u); mean %7.2f; overall mean %7.2f; Q: min %4d (%5u); max %4d (%5u); mean %7.2f; overall mean %7.2f\n",
                actual_len,
                min_I, min_I_count, max_I, max_I_count, sum_I / actual_len, all_I / all_samples,
                min_Q, min_Q_count, max_Q, max_Q_count, sum_Q / actual_len, all_Q / all_samples);
    }

    close(fd);
    return 0;
}
