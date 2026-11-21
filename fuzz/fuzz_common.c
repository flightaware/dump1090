// Shared setup for the libFuzzer harnesses. See fuzz_common.h.

#include "../net_io.c"
#include "fuzz_common.h"

#include <fcntl.h>

struct _Modes Modes;
uint64_t fuzz_now;

void receiverPositionChanged(float lat, float lon, float alt)
{
    (void) lat; (void) lon; (void) alt;
}

static struct net_service *fuzz_service(const char *descr, struct net_writer *writer, heartbeat_fn hb)
{
    struct net_service *s = serviceInit(descr, writer, hb, READ_MODE_IGNORE, NULL, NULL);
    int fd = open("/dev/null", O_WRONLY);
    if (fd < 0)
        abort();
    createGenericClient(s, fd);
    return s;
}

void fuzz_init(void)
{
    static int initialized = 0;
    if (initialized)
        return;
    initialized = 1;

    memset(&Modes, 0, sizeof(Modes));

    // Mostly the dump1090 defaults (see modesInitConfig), with everything
    // that widens the decode/output surface switched on.
    Modes.fix_df = 1;
    Modes.nfix_crc = 2;
    Modes.mode_ac = 1;
    Modes.mode_ac_auto = 0;
    Modes.enable_df24 = 1;
    Modes.interactive_display_ttl = MODES_INTERACTIVE_DISPLAY_TTL;
    Modes.json_interval = 1000;
    Modes.json_stats_interval = 60000;
    Modes.json_location_accuracy = 2;
    Modes.maxRange = 1852 * 300;
    Modes.net = 1;
    Modes.net_heartbeat_interval = MODES_NET_HEARTBEAT_INTERVAL;
    Modes.net_output_flush_size = 1300;
    Modes.net_output_flush_interval = 500;
    Modes.forward_mlat = 1;
    Modes.quiet = 1;
    Modes.faup_rate_multiplier = 1.0;
    Modes.faup_upload_unknown_commb = 1;
    Modes.sample_rate = 2400000.0;
    Modes.trailing_samples = (MODES_PREAMBLE_US + MODES_LONG_MSG_BITS + 16) * 1e-6 * Modes.sample_rate;

    // A receiver location, so relative CPR / range checks / distance output run
    Modes.fUserLat = 45.0;
    Modes.fUserLon = 10.0;
    Modes.bUserFlags |= MODES_USER_LATLON_VALID;

    Modes.log10lut = malloc(sizeof(uint16_t) * 65536);
    Modes.log10lut[0] = 0;
    for (int i = 1; i <= 65535; i++)
        Modes.log10lut[i] = (uint16_t) round(100.0 * log10(i));

    modesChecksumInit(Modes.nfix_crc);
    icaoFilterInit();
    modeACInit();

    // Output services, each with one client writing to /dev/null so that
    // prepareWrite() does not short-circuit.
    fuzz_service("Raw TCP output", &Modes.raw_out, send_raw_heartbeat);
    Modes.beast_cooked_service = fuzz_service("Beast cooked", &Modes.beast_cooked_out, send_beast_heartbeat);
    Modes.beast_verbatim_service = fuzz_service("Beast verbatim", &Modes.beast_verbatim_out, send_beast_heartbeat);
    Modes.beast_verbatim_local_service = fuzz_service("Beast verbatim local", &Modes.beast_verbatim_local_out, send_beast_heartbeat);
    fuzz_service("SBS", &Modes.sbs_out, send_sbs_heartbeat);
    fuzz_service("Stratux", &Modes.stratux_out, send_stratux_heartbeat);
    fuzz_service("FATSV", &Modes.fatsv_out, NULL);
}

void fuzz_reset(void)
{
    struct aircraft *a = Modes.aircrafts;
    while (a) {
        struct aircraft *next = a->next;
        free(a);
        a = next;
    }
    Modes.aircrafts = NULL;
    icaoFilterInit();
    Modes.json_aircraft_history_next = 0;
    for (int i = 0; i < HISTORY_SIZE; ++i) {
        free(Modes.json_aircraft_history[i].content);
        Modes.json_aircraft_history[i].content = NULL;
        Modes.json_aircraft_history[i].clen = 0;
    }
    fuzz_now = mstime();
}

void fuzz_use_message(struct modesMessage *mm)
{
    struct aircraft *a = trackUpdateFromMessage(mm);
    modesQueueOutput(mm, a);
}

//
// Minimal strict JSON syntax checker (RFC 8259), used as a test oracle.
//

struct jcur { const char *p, *end; int depth; };

static void jws(struct jcur *c)
{
    while (c->p < c->end && (*c->p == ' ' || *c->p == '\t' || *c->p == '\n' || *c->p == '\r'))
        c->p++;
}

static int jvalue(struct jcur *c);

static int jstring(struct jcur *c)
{
    if (c->p >= c->end || *c->p != '"') return 0;
    c->p++;
    while (c->p < c->end) {
        unsigned char ch = (unsigned char) *c->p++;
        if (ch == '"') return 1;
        if (ch < 0x20) return 0;
        if (ch == '\\') {
            if (c->p >= c->end) return 0;
            ch = (unsigned char) *c->p++;
            if (ch == 'u') {
                for (int i = 0; i < 4; ++i) {
                    if (c->p >= c->end || !isxdigit((unsigned char) *c->p)) return 0;
                    c->p++;
                }
            } else if (!strchr("\"\\/bfnrt", ch)) {
                return 0;
            }
        }
    }
    return 0;
}

static int jnumber(struct jcur *c)
{
    const char *s = c->p;
    if (c->p < c->end && *c->p == '-') c->p++;
    if (c->p >= c->end) return 0;
    if (*c->p == '0') c->p++;
    else if (*c->p >= '1' && *c->p <= '9') { while (c->p < c->end && isdigit((unsigned char) *c->p)) c->p++; }
    else return 0;
    if (c->p < c->end && *c->p == '.') {
        c->p++;
        if (c->p >= c->end || !isdigit((unsigned char) *c->p)) return 0;
        while (c->p < c->end && isdigit((unsigned char) *c->p)) c->p++;
    }
    if (c->p < c->end && (*c->p == 'e' || *c->p == 'E')) {
        c->p++;
        if (c->p < c->end && (*c->p == '+' || *c->p == '-')) c->p++;
        if (c->p >= c->end || !isdigit((unsigned char) *c->p)) return 0;
        while (c->p < c->end && isdigit((unsigned char) *c->p)) c->p++;
    }
    return c->p > s;
}

static int jlit(struct jcur *c, const char *lit)
{
    size_t n = strlen(lit);
    if ((size_t)(c->end - c->p) < n || memcmp(c->p, lit, n)) return 0;
    c->p += n;
    return 1;
}

static int jvalue(struct jcur *c)
{
    jws(c);
    if (c->p >= c->end) return 0;
    if (++c->depth > 64) return 0;
    int ok;
    switch (*c->p) {
    case '{':
        c->p++; jws(c);
        if (c->p < c->end && *c->p == '}') { c->p++; ok = 1; break; }
        for (;;) {
            jws(c);
            if (!jstring(c)) { ok = 0; break; }
            jws(c);
            if (c->p >= c->end || *c->p != ':') { ok = 0; break; }
            c->p++;
            if (!jvalue(c)) { ok = 0; break; }
            jws(c);
            if (c->p < c->end && *c->p == ',') { c->p++; continue; }
            if (c->p < c->end && *c->p == '}') { c->p++; ok = 1; break; }
            ok = 0; break;
        }
        break;
    case '[':
        c->p++; jws(c);
        if (c->p < c->end && *c->p == ']') { c->p++; ok = 1; break; }
        for (;;) {
            if (!jvalue(c)) { ok = 0; break; }
            jws(c);
            if (c->p < c->end && *c->p == ',') { c->p++; continue; }
            if (c->p < c->end && *c->p == ']') { c->p++; ok = 1; break; }
            ok = 0; break;
        }
        break;
    case '"': ok = jstring(c); break;
    case 't': ok = jlit(c, "true"); break;
    case 'f': ok = jlit(c, "false"); break;
    case 'n': ok = jlit(c, "null"); break;
    default: ok = jnumber(c); break;
    }
    c->depth--;
    return ok;
}

void fuzz_check_json(const char *what, const char *buf, int len)
{
    struct jcur c = { buf, buf + len, 0 };
    if (!buf || len <= 0) {
        fprintf(stderr, "fuzz: %s: empty JSON\n", what);
        abort();
    }
    if (!jvalue(&c)) {
        fprintf(stderr, "fuzz: %s: invalid JSON near offset %d: %.*s\n", what, (int)(c.p - buf), (int) (c.end - c.p > 80 ? 80 : c.end - c.p), c.p);
        abort();
    }
    jws(&c);
    if (c.p != c.end) {
        fprintf(stderr, "fuzz: %s: trailing garbage in JSON at offset %d\n", what, (int)(c.p - buf));
        abort();
    }
}

void fuzz_run_outputs(void)
{
    int len;
    char *json;

    json = generateAircraftJson("/data/aircraft.json", &len);
    fuzz_check_json("aircraft.json", json, len);

    // also keep a history entry, as dump1090 does
    int h = Modes.json_aircraft_history_next;
    free(Modes.json_aircraft_history[h].content);
    Modes.json_aircraft_history[h].content = json;
    Modes.json_aircraft_history[h].clen = len;
    Modes.json_aircraft_history_next = (h + 1) % HISTORY_SIZE;

    json = generateHistoryJson("/data/history_0.json", &len);
    fuzz_check_json("history_0.json", json, len);
    free(json);

    json = generateReceiverJson("/data/receiver.json", &len);
    fuzz_check_json("receiver.json", json, len);
    free(json);

    json = generateStatsJson("/data/stats.json", &len);
    fuzz_check_json("stats.json", json, len);
    free(json);

    writeFATSV();

    flushWrites(&Modes.raw_out);
    flushWrites(&Modes.beast_cooked_out);
    flushWrites(&Modes.beast_verbatim_out);
    flushWrites(&Modes.beast_verbatim_local_out);
    flushWrites(&Modes.sbs_out);
    flushWrites(&Modes.stratux_out);
    flushWrites(&Modes.fatsv_out);
}

void fuzz_fix_crc(unsigned char *msg, uint32_t residual)
{
    int bits = modesMessageLenByType(msg[0] >> 3);
    int bytes = bits / 8;
    uint32_t crc = modesChecksum(msg, bits);  // includes the existing parity bits
    uint32_t parity = ((uint32_t) msg[bytes - 3] << 16) | ((uint32_t) msg[bytes - 2] << 8) | msg[bytes - 1];
    // modesChecksum returns (computed crc) ^ (parity field); we want
    // new parity = computed crc ^ residual
    parity = crc ^ parity ^ residual;
    msg[bytes - 3] = (parity >> 16) & 0xFF;
    msg[bytes - 2] = (parity >> 8) & 0xFF;
    msg[bytes - 1] = parity & 0xFF;
}

// Accessors for net_io.c statics used by fuzz_net.c
struct net_service *fuzz_make_raw_input_service(void)
{
    return serviceInit("Raw TCP input", NULL, NULL, READ_MODE_ASCII, "\n", decodeHexMessage);
}

void fuzz_read_from_client(struct client *c)
{
    modesReadFromClient(c);
}

void fuzz_close_client(struct client *c)
{
    modesCloseClient(c);
}
