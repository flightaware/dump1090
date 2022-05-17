#ifndef CLOCK_NANOSLEEP_H
#define CLOCK_NANOSLEEP_H

int clock_nanosleep (clockid_t id, int flags, const struct timespec *ts,
                     struct timespec *ots);

#endif //CLOCK_NANOSLEEP_H
