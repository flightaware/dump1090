// Deterministic, single-threaded replay of IQ samples or a Beast stream
// through the full dump1090 pipeline (sample conversion, demodulation,
// decoding, tracking, CPR, every network output format and aircraft.json),
// with a simulated clock so that the output depends only on the input.
//
// Used to check that a code change leaves decoded output byte-for-byte
// identical, and as a repeatable benchmark workload.
//
//   fuzz/build-replay.sh [outdir]
//   replay [--quiet] [--modeac] [--fix N] [--lat LAT --lon LON] (--iq FILE | --beast FILE) OUTDIR
//
// Writes to OUTDIR: messages.txt (decoded messages, as dump1090 prints
// them; empty with --quiet, which is how dump1090 normally runs), raw.txt,
// beast.bin, sbs.txt, stratux.txt, fatsv.txt and aircraft.jsonl
// (aircraft.json once per simulated second).
//
// net_io.c is included directly to reach its static functions; link with
// -Wl,--wrap=mstime,--wrap=clock_gettime so that every wall-clock read sees
// the simulated clock. Run with TZ=UTC for output that is identical across hosts.

#include "../net_io.c"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

struct _Modes Modes;

void receiverPositionChanged(float lat, float lon, float alt)
{
    (void) lat; (void) lon; (void) alt;
}

static uint64_t sim_now = 1700000000000ULL;   // simulated wall clock, ms

uint64_t __wrap_mstime(void);
uint64_t __wrap_mstime(void)
{
    return sim_now;
}

// The SBS writer reads CLOCK_REALTIME directly for its "logged" timestamps
int __real_clock_gettime(clockid_t clk, struct timespec *ts);
int __wrap_clock_gettime(clockid_t clk, struct timespec *ts);
int __wrap_clock_gettime(clockid_t clk, struct timespec *ts)
{
    if (clk == CLOCK_REALTIME) {
        ts->tv_sec = sim_now / 1000;
        ts->tv_nsec = (sim_now % 1000) * 1000000;
        return 0;
    }
    return __real_clock_gettime(clk, ts);
}

static FILE *json_out;
static uint64_t next_second;

static void open_output(const char *dir, const char *name, struct net_writer *writer, const char *descr, struct net_service **svcp)
{
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror(path);
        exit(1);
    }
    struct net_service *s = serviceInit(descr, writer, NULL, READ_MODE_IGNORE, NULL, NULL);
    createGenericClient(s, fd);
    // output files are regular files: make writes blocking again
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
    if (svcp)
        *svcp = s;
}

static void periodic(void)
{
    while (sim_now >= next_second) {
        trackPeriodicUpdate();
        icaoFilterExpire();

        int len;
        char *json = generateAircraftJson("/data/aircraft.json", &len);
        fwrite(json, 1, len, json_out);
        fputc('\n', json_out);
        free(json);

        writeFATSV();
        flushWrites(&Modes.raw_out);
        flushWrites(&Modes.beast_cooked_out);
        flushWrites(&Modes.sbs_out);
        flushWrites(&Modes.stratux_out);
        flushWrites(&Modes.fatsv_out);

        next_second += 1000;
    }
}

static void replay_iq(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror(path);
        exit(1);
    }

    unsigned overlap = Modes.trailing_samples;
    unsigned block = MODES_MAG_BUF_SAMPLES;
    uint16_t *data = calloc(block + overlap, sizeof(uint16_t));
    uint8_t *iq = malloc(block * 2);
    struct converter_state *state = NULL;
    iq_convert_fn convert = init_converter(INPUT_UC8, Modes.sample_rate, 0, &state);
    uint64_t start = sim_now;
    uint64_t samples = 0;

    for (;;) {
        size_t got = 0;
        while (got < block * 2) {
            ssize_t n = read(fd, iq + got, block * 2 - got);
            if (n <= 0)
                break;
            got += n;
        }
        unsigned nsamples = got / 2;
        if (nsamples == 0)
            break;

        struct mag_buf mag;
        memset(&mag, 0, sizeof(mag));
        mag.data = data;
        mag.totalLength = block + overlap;
        mag.overlap = overlap;
        mag.validLength = overlap + nsamples;
        mag.sampleTimestamp = samples * 12000000ULL / 2400000ULL;
        mag.sysTimestamp = start + samples * 1000 / 2400000;

        convert(iq, &data[overlap], nsamples, state, &mag.mean_level, &mag.mean_power);

        samples += nsamples;
        sim_now = start + samples * 1000 / 2400000;

        demodulate2400(&mag);
        if (Modes.mode_ac)
            demodulate2400AC(&mag);

        // keep the trailing samples as the next block's overlap, as fifo.c does
        memmove(data, &data[mag.validLength - overlap], overlap * sizeof(uint16_t));

        periodic();
    }

    free(iq);
    free(data);
    close(fd);
}

static void replay_beast(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(size);
    if (fread(buf, 1, size, f) != (size_t) size) {
        perror(path);
        exit(1);
    }
    fclose(f);

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        exit(1);
    }
    struct client *c = createGenericClient(makeBeastInputService(), sv[1]);

    uint64_t start = sim_now;
    uint64_t first_ts = 0;
    long pos = 0;
    while (pos < size && c->service) {
        // collect messages covering up to 10ms of receiver time
        long batch_start = pos, p = pos;
        uint64_t batch_ts = 0;
        while (p < size && p - batch_start < 4000) {
            if (buf[p] != 0x1a) { ++p; continue; }
            long q = p + 1;
            if (q >= size) break;
            int type = buf[q++];
            int len = (type == '1') ? 2 : (type == '2') ? 7 : (type == '3') ? 14 : -1;
            if (len < 0) { p = q; continue; }
            uint64_t ts = 0;
            int need = 6 + 1 + len, k = 0;
            while (k < need && q < size) {
                uint8_t ch = buf[q++];
                if (ch == 0x1a) ++q;
                if (k < 6) ts = (ts << 8) | ch;
                ++k;
            }
            if (k < need) { p = size; break; }
            if (!first_ts) first_ts = ts;
            if (batch_ts && ts > batch_ts + 120000)   // 10ms at 12MHz
                break;
            if (!batch_ts) batch_ts = ts;
            p = q;
        }
        if (p == batch_start)
            p = size;

        sim_now = start + (batch_ts - first_ts) / 12000;
        if (write(sv[0], buf + batch_start, p - batch_start) != p - batch_start) {
            perror("write");
            exit(1);
        }
        // read until the socket is drained
        int pending;
        do {
            modesReadFromClient(c);
            pending = 0;
            if (!c->service || ioctl(sv[1], FIONREAD, &pending) != 0)
                break;
        } while (pending > 0);
        pos = p;
        periodic();
    }
    close(sv[0]);
    free(buf);
}

int main(int argc, char **argv)
{
    const char *iq = NULL, *beast = NULL, *outdir = NULL;

    memset(&Modes, 0, sizeof(Modes));
    Modes.fix_df = 1;
    Modes.nfix_crc = 1;
    Modes.mode_ac = 0;
    Modes.interactive_display_ttl = MODES_INTERACTIVE_DISPLAY_TTL;
    Modes.json_interval = 1000;
    Modes.json_location_accuracy = 2;
    Modes.maxRange = 1852 * 300;
    Modes.net = 1;
    Modes.net_heartbeat_interval = MODES_NET_HEARTBEAT_INTERVAL;
    Modes.net_output_flush_size = 1300;
    Modes.net_output_flush_interval = 500;
    Modes.faup_rate_multiplier = 1.0;
    Modes.sample_rate = 2400000.0;
    Modes.trailing_samples = (MODES_PREAMBLE_US + MODES_LONG_MSG_BITS + 16) * 1e-6 * Modes.sample_rate;

    for (int j = 1; j < argc; ++j) {
        if (!strcmp(argv[j], "--modeac")) Modes.mode_ac = 1;
        else if (!strcmp(argv[j], "--quiet")) Modes.quiet = 1;
        else if (!strcmp(argv[j], "--fix") && j + 1 < argc) Modes.nfix_crc = atoi(argv[++j]);
        else if (!strcmp(argv[j], "--lat") && j + 1 < argc) Modes.fUserLat = atof(argv[++j]);
        else if (!strcmp(argv[j], "--lon") && j + 1 < argc) Modes.fUserLon = atof(argv[++j]);
        else if (!strcmp(argv[j], "--iq") && j + 1 < argc) iq = argv[++j];
        else if (!strcmp(argv[j], "--beast") && j + 1 < argc) beast = argv[++j];
        else if (argv[j][0] != '-' && !outdir) outdir = argv[j];
        else {
            fprintf(stderr, "usage: %s [--quiet] [--modeac] [--fix N] [--lat LAT --lon LON] (--iq FILE | --beast FILE) OUTDIR\n", argv[0]);
            return 1;
        }
    }
    if ((!iq && !beast) || !outdir) {
        fprintf(stderr, "usage: %s [--quiet] [--modeac] [--fix N] [--lat LAT --lon LON] (--iq FILE | --beast FILE) OUTDIR\n", argv[0]);
        return 1;
    }
    if (Modes.nfix_crc > MODES_MAX_BITERRORS)
        Modes.nfix_crc = MODES_MAX_BITERRORS;
    if (Modes.fUserLat != 0.0 || Modes.fUserLon != 0.0)
        Modes.bUserFlags |= MODES_USER_LATLON_VALID;

    Modes.log10lut = malloc(sizeof(uint16_t) * 65536);
    Modes.log10lut[0] = 0;
    for (int i = 1; i <= 65535; i++)
        Modes.log10lut[i] = (uint16_t) round(100.0 * log10(i));
    modesChecksumInit(Modes.nfix_crc);
    icaoFilterInit();
    modeACInit();

    char path[4096];
    snprintf(path, sizeof(path), "%s/messages.txt", outdir);
    if (!freopen(path, "w", stdout)) {
        perror(path);
        return 1;
    }
    snprintf(path, sizeof(path), "%s/aircraft.jsonl", outdir);
    json_out = fopen(path, "w");
    if (!json_out) {
        perror(path);
        return 1;
    }

    open_output(outdir, "raw.txt", &Modes.raw_out, "Raw output", NULL);
    open_output(outdir, "beast.bin", &Modes.beast_cooked_out, "Beast output", &Modes.beast_cooked_service);
    open_output(outdir, "sbs.txt", &Modes.sbs_out, "SBS output", NULL);
    open_output(outdir, "stratux.txt", &Modes.stratux_out, "Stratux output", NULL);
    open_output(outdir, "fatsv.txt", &Modes.fatsv_out, "FATSV output", NULL);

    next_second = sim_now + 1000;
    if (iq)
        replay_iq(iq);
    else
        replay_beast(beast);

    sim_now = next_second;
    periodic();
    fflush(stdout);
    fclose(json_out);
    return 0;
}
