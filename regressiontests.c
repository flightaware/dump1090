// Part of dump1090, a Mode S message decoder for RTLSDR devices.
//
// regressiontests.c - regression tests for input-handling fixes
//
// This file is free software: you may copy, redistribute and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 2 of the License, or (at your
// option) any later version.
//
// This file is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// net_io.c is included directly so that its static input handlers can be
// tested; link this WITHOUT net_io.o.
#include "net_io.c"

#include <time.h>

struct _Modes Modes;

void receiverPositionChanged(float lat, float lon, float alt)
{
    (void) lat; (void) lon; (void) alt;
}

static int failures;

#define CHECK(cond, ...) do {                                   \
        if (cond) {                                             \
            fprintf(stderr, "PASS: " __VA_ARGS__);              \
        } else {                                                \
            fprintf(stderr, "FAIL: " __VA_ARGS__);              \
            ++failures;                                         \
        }                                                       \
        fprintf(stderr, "\n");                                  \
    } while (0)

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static unsigned count_aircraft(void)
{
    unsigned n = 0;
    for (struct aircraft *a = Modes.aircrafts; a; a = a->next)
        ++n;
    return n;
}

static struct aircraft *find_aircraft(uint32_t addr)
{
    for (struct aircraft *a = Modes.aircrafts; a; a = a->next)
        if (a->addr == addr)
            return a;
    return NULL;
}

// A flood of distinct addresses must not make the ICAO filter fail to
// register new addresses, nor make lookups scan the whole table.
static void test_icao_filter_flood(void)
{
    icaoFilterInit();
    for (uint32_t i = 0; i < 50000; ++i)
        icaoFilterAdd(0x100000 + i * 7);

    icaoFilterAdd(0xABCDEF);
    CHECK(icaoFilterTest(0xABCDEF), "icao filter: new address is registered after a flood of 50000 addresses");

    double start = now_seconds();
    int found = 0;
    for (uint32_t i = 0; i < 200000; ++i)
        found += icaoFilterTest(0xF00000 + i);   // never added
    double per_lookup = (now_seconds() - start) / 200000;
    CHECK(per_lookup < 1e-6, "icao filter: lookup miss in a flooded filter takes %.0fns (limit 1000ns)", per_lookup * 1e9);
    (void) found;

    icaoFilterInit();
}

static void fake_df17(struct modesMessage *mm, uint32_t addr, uint64_t now)
{
    memset(mm, 0, sizeof(*mm));
    mm->msgtype = 17;
    mm->msgbits = 112;
    mm->addr = addr;
    mm->addrtype = ADDR_ADSB_ICAO;
    mm->source = SOURCE_ADSB;
    mm->reliable = 1;
    mm->sysTimestampMsg = now;
    mm->signalLevel = 0.1;
}

// A flood of distinct (spoofed) addresses must not grow the aircraft table
// without bound, and an aircraft that keeps transmitting must survive it.
static void test_aircraft_table_bound(void)
{
    struct modesMessage mm;
    uint64_t now = mstime();
    const uint32_t real = 0x7C1234;

    trackRemoveAllAircraft();

    fake_df17(&mm, real, now);
    struct aircraft *real_a = trackUpdateFromMessage(&mm);

    unsigned flood = TRACK_MAX_AIRCRAFT * 3;
    double start = now_seconds();
    for (unsigned i = 0; i < flood; ++i) {
        now += 1;   // 1000 new addresses per (simulated) second
        fake_df17(&mm, 0x800000 + i, now);
        trackUpdateFromMessage(&mm);
        if (i % 100 == 0) {
            // the real aircraft transmits several times per second
            fake_df17(&mm, real, now);
            real_a = trackUpdateFromMessage(&mm);
        }
    }
    double elapsed = now_seconds() - start;

    unsigned n = count_aircraft();
    CHECK(n <= (unsigned) TRACK_MAX_AIRCRAFT, "aircraft table: %u aircraft after %u distinct addresses (limit %u)", n, flood, (unsigned) TRACK_MAX_AIRCRAFT);
    CHECK(real_a && find_aircraft(real) == real_a, "aircraft table: a regularly transmitting aircraft survives the flood");
    CHECK(elapsed < 5.0, "aircraft table: %u updates took %.2fs (limit 5s)", flood, elapsed);

    // lookups go to the same record
    fake_df17(&mm, real, now + 1);
    CHECK(trackUpdateFromMessage(&mm) == real_a, "aircraft table: lookup returns the existing record");

    trackRemoveAllAircraft();
    CHECK(Modes.aircrafts == NULL && count_aircraft() == 0, "aircraft table: trackRemoveAllAircraft empties the table");
}

// A 7-byte frame whose DF denotes a 112-bit message must be rejected rather
// than decoded using bytes that were never received.
static void test_short_frame_long_df(void)
{
    char line[] = "*A0000000000000;";   // DF20 (long) in a short frame
    unsigned before = Modes.stats_current.remote_rejected_bad;

    trackRemoveAllAircraft();
    decodeHexMessage(NULL, line);
    CHECK(Modes.stats_current.remote_rejected_bad == before + 1, "AVR input: short frame with a long DF is rejected");

    // Beast: <type '2'> <6 bytes timestamp> <signal> <7 bytes>, DF20
    char beast[] = { '2', 0, 0, 0, 0, 0, 1, 0x40, (char) 0xA0, 0, 0, 0, 0, 0, 0 };
    before = Modes.stats_current.remote_rejected_bad;
    decodeBinMessage(NULL, beast);
    CHECK(Modes.stats_current.remote_rejected_bad == before + 1, "Beast input: short frame with a long DF is rejected");

    // a well-formed short frame (DF11 all-call reply with valid parity) is still accepted
    char good[] = "*5D4840D6F8740F;";
    unsigned accepted = Modes.stats_current.remote_accepted[0];
    decodeHexMessage(NULL, good);
    CHECK(Modes.stats_current.remote_accepted[0] == accepted + 1, "AVR input: well-formed DF11 frame is accepted");
    trackRemoveAllAircraft();
}

// faup1090 command input: a keyword with no value must not crash.
static void test_faup_command_missing_value(void)
{
    char c1[] = "upload_rate_multiplier";
    char c2[] = "upload_unknown_commb\t";
    char c3[] = "upload_rate_multiplier\t2.5";
    Modes.faup_rate_multiplier = 1.0;
    handleFaupCommand(NULL, c1);
    handleFaupCommand(NULL, c2);
    CHECK(Modes.faup_rate_multiplier == 1.0, "faup command: keywords without a value are ignored");
    handleFaupCommand(NULL, c3);
    CHECK(Modes.faup_rate_multiplier == 2.5, "faup command: a well-formed command still works");
}

static void put_le_float(char *p, float f)
{
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    p[0] = bits & 0xFF;
    p[1] = (bits >> 8) & 0xFF;
    p[2] = (bits >> 16) & 0xFF;
    p[3] = (bits >> 24) & 0xFF;
}

// A Radarcape position message from a Beast input client must not set the
// receiver location unless explicitly allowed.
static void test_radarcape_position(void)
{
    char frame[22] = { '5' };   // type byte + 21 payload bytes
    put_le_float(frame + 1 + 4, 51.5f);
    put_le_float(frame + 1 + 8, -0.25f);
    put_le_float(frame + 1 + 12, 100.0f);

    Modes.bUserFlags &= ~MODES_USER_LATLON_VALID;
    Modes.fUserLat = Modes.fUserLon = 0;
    Modes.net_accept_position = 0;
    decodeBinMessage(NULL, frame);
    CHECK(!(Modes.bUserFlags & MODES_USER_LATLON_VALID), "Beast input: Radarcape position is ignored by default");

    Modes.net_accept_position = 1;
    decodeBinMessage(NULL, frame);
    CHECK((Modes.bUserFlags & MODES_USER_LATLON_VALID) && Modes.fUserLat == 51.5 && Modes.fUserLon == -0.25,
          "Beast input: Radarcape position is accepted with --net-accept-position");

    Modes.bUserFlags &= ~MODES_USER_LATLON_VALID;
    Modes.fUserLat = Modes.fUserLon = 0;
    Modes.net_accept_position = 0;
}

int main(void)
{
    memset(&Modes, 0, sizeof(Modes));
    Modes.fix_df = 1;
    Modes.nfix_crc = 1;
    Modes.maxRange = 1852 * 300;
    Modes.quiet = 1;
    modesChecksumInit(Modes.nfix_crc);
    icaoFilterInit();
    modeACInit();

    test_icao_filter_flood();
    test_aircraft_table_bound();
    test_short_frame_long_df();
    test_faup_command_missing_value();
    test_radarcape_position();

    if (failures) {
        fprintf(stderr, "%d regression test(s) FAILED\n", failures);
        return 1;
    }
    fprintf(stderr, "all regression tests passed\n");
    return 0;
}
