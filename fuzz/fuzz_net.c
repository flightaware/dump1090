// libFuzzer harness: network input parsers and the client read loop
// (modesReadFromClient) for every input service:
//
//   selector % 4 == 0: AVR raw input (port 30001, READ_MODE_ASCII, decodeHexMessage)
//   selector % 4 == 1: Beast binary input (ports 30004/30104, READ_MODE_BEAST, decodeBinMessage)
//   selector % 4 == 2: Beast output port command channel (port 30005, handleBeastCommand)
//   selector % 4 == 3: faup1090 command input (stdin from piaware, handleFaupCommand)
//
// Input: byte 0 = selector, byte 1 = chunking seed, rest = stream bytes.
// The stream is written to a socketpair in chunks of varying size and the
// read loop is run after each chunk, so messages split across reads are
// exercised.

#include "fuzz_common.h"
#include "../dump1090.h"

#include <sys/socket.h>

static struct net_service *svc[4];

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void) argc; (void) argv;
    fuzz_init();
    svc[0] = fuzz_make_raw_input_service();
    svc[1] = makeBeastInputService();
    svc[2] = Modes.beast_cooked_service;
    svc[3] = makeFaCmdInputService();
    return 0;
}

static void drain(int fd)
{
    char junk[4096];
    while (read(fd, junk, sizeof(junk)) > 0)
        ;
}

static void free_closed_clients(void)
{
    struct client *c, **prev;
    for (prev = &Modes.clients, c = *prev; c; c = *prev) {
        if (c->fd == -1) {
            *prev = c->next;
            free(c);
        } else {
            prev = &c->next;
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2 || size > 60000)
        return 0;

    fuzz_reset();

    int which = data[0] & 3;
    unsigned seed = data[1];
    data += 2; size -= 2;

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
        abort();
    int bufsz = 262144;
    setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &bufsz, sizeof(bufsz));
    setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &bufsz, sizeof(bufsz));
    anetNonBlock(Modes.aneterr, sv[0]);

    struct client *c = createGenericClient(svc[which], sv[1]);

    size_t off = 0;
    unsigned chunkno = 0;
    while (off < size && c->service) {
        // chunk sizes vary between 1 and ~1500 bytes depending on seed
        size_t chunk = ((seed * 131u + chunkno * 977u) % 1500u) + 1;
        if (seed & 0x80)
            chunk = (chunk % 64) + 1;   // small chunks
        if (chunk > size - off)
            chunk = size - off;
        ssize_t w = write(sv[0], data + off, chunk);
        if (w <= 0)
            break;
        off += (size_t) w;
        ++chunkno;
        fuzz_read_from_client(c);
        drain(sv[0]);   // anything the handler wrote back (e.g. beast settings)
    }

    if (c->service)
        fuzz_close_client(c);    // closes sv[1]
    close(sv[0]);
    free_closed_clients();

    fuzz_run_outputs();
    return 0;
}
