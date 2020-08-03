#include <stdio.h>

#include "../dump1090.h"
#include "../comm_b.h"

char last_callsign[8];
double last_callsign_ts = 0;
double last_track = -1;
double last_track_ts = 0;
double last_magnetic = -1;
double last_magnetic_ts = 0;
double last_gs = -1;
double last_gs_ts = 0;
double last_ias = -1;
double last_ias_ts = 0;
double last_tas = -1;
double last_tas_ts = 0;
double last_mach = -1;
double last_mach_ts = 0;

double angle_difference(double h1, double h2)
{
    float delta = fabs(h1 - h2);
    if (delta > 180.0)
        delta = 360.0 - delta;
    return delta;
}

void process(double timestamp, const char *line, struct modesMessage *mm)
{
    decodeCommB(mm);

    printf("line\t%s\tformat\t", line);

    switch (mm->commb_format) {
#define EMIT(x) case COMMB_ ## x: printf("%s", #x); break
        EMIT(UNKNOWN);
        EMIT(AMBIGUOUS);
        EMIT(EMPTY_RESPONSE);
        EMIT(DATALINK_CAPS);
        EMIT(GICB_CAPS);
        EMIT(AIRCRAFT_IDENT);
        EMIT(ACAS_RA);
        EMIT(VERTICAL_INTENT);
        EMIT(TRACK_TURN);
        EMIT(HEADING_SPEED);
#undef EMIT
    default:
        printf("%s", "UNHANDLED"); break;
    }

    int suspicious = 0;
    
    if (mm->callsign_valid) {
        printf("\tcallsign\t%s", mm->callsign);
        if ((timestamp - last_callsign_ts) < 30.0 && strcmp(last_callsign, mm->callsign)) {
            suspicious = 1;
        }
        memcpy(last_callsign, mm->callsign, sizeof(last_callsign));
        last_callsign_ts = timestamp;
    }
    if (mm->heading_valid && mm->heading_type == HEADING_GROUND_TRACK) {
        printf("\ttrack\t%.1f", mm->heading);
        if ((timestamp - last_track_ts) < 10.0 && angle_difference(last_track, mm->heading) > 45) {
            suspicious = 1;
        }
        if ((timestamp - last_magnetic_ts) < 10.0 && angle_difference(last_magnetic, mm->heading) > 45) {
            suspicious = 1;
        }
        last_track = mm->heading;
        last_track_ts = timestamp;
    } 
    if (mm->heading_valid && mm->heading_type == HEADING_MAGNETIC) {
        printf("\tmagnetic\t%.1f", mm->heading);
        if ((timestamp - last_magnetic_ts) < 10.0 && angle_difference(last_magnetic, mm->heading) > 45) {
            suspicious = 1;
        }
        if ((timestamp - last_track_ts) < 10.0 && angle_difference(last_track, mm->heading) > 45) {
            suspicious = 1;
        }
        last_magnetic = mm->heading;
        last_magnetic_ts = timestamp;
    } 
    if (mm->track_rate_valid) {
        printf("\ttrack_rate\t%.2f", mm->track_rate);
    }
    if (mm->roll_valid) {
        printf("\troll\t%.1f", mm->roll);
    }
    if (mm->gs_valid) {
        printf("\tgs\t%.1f", mm->gs.selected);
        if ((timestamp - last_gs_ts) < 10.0 && fabs(last_gs - mm->gs.selected) > 50) {
            suspicious = 1;
        }
        last_gs = mm->gs.selected;
        last_gs_ts = timestamp;
    }
    if (mm->ias_valid) {
        printf("\tias\t%d", mm->ias);
        if ((timestamp - last_ias_ts) < 10.0 && abs(last_ias - mm->ias) > 50) {
            suspicious = 1;
        }
        last_ias = mm->ias;
        last_ias_ts = timestamp;
    }
    if (mm->tas_valid) {
        printf("\ttas\t%d", mm->tas);
        if ((timestamp - last_tas_ts) < 10.0 && abs(last_tas - mm->tas) > 50) {
            suspicious = 1;
        }
        last_tas = mm->tas;
        last_tas_ts = timestamp;
    }
    if (mm->mach_valid) {
        printf("\tmach\t%.3f", mm->mach);
        if ((timestamp - last_mach_ts) < 10.0 && abs(last_mach - mm->mach) > 0.1) {
            suspicious = 1;
        }
        last_mach = mm->mach;
        last_mach_ts = timestamp;
    }

    if (suspicious) {
        printf("\tsuspicious\tyes!");
    }

    printf("\n");
}

int main(int argc, char **argv)
{
    /* unused */ (void)argc;
    /* unused */ (void)argv;
    
    char line[1024];

    while (fgets(line, sizeof(line), stdin)) {
        if (line[strlen(line)-1] == '\n') {
            line[strlen(line)-1] = '\0';
        }

        double timestamp = 0;
        int index = 0;
        if (sscanf(line, "%lf %n", &timestamp, &index) < 1) {
            goto bad;
        }

        char *hex = line + index;

        static struct modesMessage mmZero;
        struct modesMessage mm = mmZero;
        for (unsigned i = 0; i < sizeof(mm.MB); ++i) {
            if (!isxdigit(hex[i*2]) || !isxdigit(hex[i*2 + 1])) {
                goto bad;
            }

            unsigned xvalue = 0;
            if (sscanf(hex + i*2, "%2x", &xvalue) < 1) {
                goto bad;
            }

            mm.MB[i] = xvalue;
        }

        process(timestamp, line, &mm);
        continue;

    bad:
        fprintf(stderr, "failed to scan line: %s", line);
        continue;
    }
}
