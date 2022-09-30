
/* starch generated code. Do not edit. */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

//On Windows platform, separate functions for allocating and freeing aligned memory must be used
#ifdef _WIN32
#  include <malloc.h>
#  define util_aligned_alloc(alignment,size) (_aligned_malloc(size, alignment))
#  define util_aligned_free(ptr) (_aligned_free(ptr))
#else
#  define util_aligned_alloc(alignment,size) (aligned_alloc(alignment,size))
#  define util_aligned_free(ptr) (free(ptr))
#endif

#include "starch.h"

typedef struct timespec starch_benchmark_time;

uint64_t starch_benchmark_elapsed(starch_benchmark_time *start, starch_benchmark_time *end);
void *starch_benchmark_aligned_alloc(size_t alignment, size_t type_alignment, size_t size);
void starch_benchmark_aligned_free(void *user_ptr);
void starch_benchmark_get_time(starch_benchmark_time *t);

const unsigned starch_benchmark_warmup_loops = 10;

typedef struct {
    const char *name;
    const char *impl;
    uint64_t ns;
} starch_benchmark_result;

static starch_benchmark_result *starch_benchmark_results = NULL;
static unsigned starch_benchmark_result_size = 0;
static unsigned starch_benchmark_result_count = 0;

typedef struct benchmark_flavor_list_node {
    const char *flavor;
    struct benchmark_flavor_list_node *next;
} starch_benchmark_flavor_list;

static starch_benchmark_flavor_list *starch_benchmark_flavor_whitelist = NULL;
static starch_benchmark_flavor_list *starch_benchmark_flavor_blacklist = NULL;

static bool starch_benchmark_list_only = false;
static bool starch_benchmark_validate_only = false;
static bool starch_benchmark_validation_failed = false;
static bool starch_benchmark_top_only = false;
static unsigned starch_benchmark_iterations = 1;

typedef struct timespec starch_benchmark_time;
void starch_benchmark_get_time(starch_benchmark_time *t)
{
#ifdef CLOCK_THREAD_CPUTIME_ID
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, t);
#else
    clock_gettime(CLOCK_MONOTONIC, t);
#endif
}

uint64_t starch_benchmark_elapsed(starch_benchmark_time *start, starch_benchmark_time *end)
{
    return ((uint64_t)end->tv_sec - (uint64_t)start->tv_sec) * 1000000000U + (uint64_t)end->tv_nsec - (uint64_t)start->tv_nsec;
}

void *starch_benchmark_aligned_alloc(size_t alignment, size_t type_alignment, size_t size)
{
    size_t use_alignment = (type_alignment > alignment ? type_alignment : alignment);
    if ( (use_alignment % type_alignment) || (use_alignment % alignment) ) {
        fprintf(stderr, "STARCH_BENCHMARK_ALLOC of %zu bytes failed: conflicting alignment requirements (%zu versus %zu)\n", size, alignment, type_alignment);
        return NULL;
    }

    /* Over-allocate so we can stash our own pointer before the start, and so that we can adjust
     * the returned alignment so it is only aligned to the requested boundary, and not also
     * aligned to a larger power of two (we don't want to accidentally benchmark the performance
     * of a more restrictive larger alignment)
     */
    size_t header_size = (use_alignment < sizeof(void*) ? sizeof(void*) : use_alignment);
    char *block_ptr = util_aligned_alloc(use_alignment, header_size + size + use_alignment);
    if (!block_ptr) {
        fprintf(stderr, "STARCH_BENCHMARK_ALLOC of %zu bytes failed: %s\n", size, strerror(errno));
        return NULL;
    }

    char *user_ptr = block_ptr + header_size;
    if ( (uintptr_t)user_ptr % (use_alignment * 2) == 0 ) {
        // user_ptr is aligned to the next power of two, but we don't want that, move it on
        user_ptr += use_alignment;
    }

    void **stash = (void**)user_ptr - 1;
    *stash = block_ptr;

    return user_ptr;
}

void starch_benchmark_aligned_free(void *user_ptr)
{
    if (!user_ptr)
        return;
    void **stash = (void**)user_ptr - 1;
    util_aligned_free(*stash);
}

static bool starch_benchmark_flavor_in_list(const char *flavor, const starch_benchmark_flavor_list *list)
{
    for (; list; list = list->next) {
        if (!strcmp(flavor, list->flavor))
            return true;
    }
    return false;
}


/* prototypes for benchmark helpers provided by user code */
void starch_count_above_u16_benchmark (void);
bool starch_count_above_u16_benchmark_verify ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_count_above_u16_benchmark(void);

static void starch_benchmark_one_count_above_u16( starch_count_above_u16_regentry * _entry, const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2, arg3 );

    /* verify correctness of the output */
    if (! starch_count_above_u16_benchmark_verify ( arg0, arg1, arg2, arg3 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "count_above_u16";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_count_above_u16( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 )
{
    for (starch_count_above_u16_regentry *_entry = starch_count_above_u16_registry; _entry->name; ++_entry) {
        starch_benchmark_one_count_above_u16( _entry, arg0, arg1, arg2, arg3 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_count_above_u16_aligned_benchmark (void);
bool starch_count_above_u16_aligned_benchmark_verify ( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_count_above_u16_aligned_benchmark(void);

static void starch_benchmark_one_count_above_u16_aligned( starch_count_above_u16_aligned_regentry * _entry, const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2, arg3 );

    /* verify correctness of the output */
    if (! starch_count_above_u16_aligned_benchmark_verify ( arg0, arg1, arg2, arg3 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "count_above_u16_aligned";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_count_above_u16_aligned( const uint16_t * arg0, unsigned arg1, uint16_t arg2, unsigned * arg3 )
{
    for (starch_count_above_u16_aligned_regentry *_entry = starch_count_above_u16_aligned_registry; _entry->name; ++_entry) {
        starch_benchmark_one_count_above_u16_aligned( _entry, arg0, arg1, arg2, arg3 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_magnitude_power_uc8_benchmark (void);
bool starch_magnitude_power_uc8_benchmark_verify ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_magnitude_power_uc8_benchmark(void);

static void starch_benchmark_one_magnitude_power_uc8( starch_magnitude_power_uc8_regentry * _entry, const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2, arg3, arg4 );

    /* verify correctness of the output */
    if (! starch_magnitude_power_uc8_benchmark_verify ( arg0, arg1, arg2, arg3, arg4 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3, arg4 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3, arg4 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "magnitude_power_uc8";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_magnitude_power_uc8( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 )
{
    for (starch_magnitude_power_uc8_regentry *_entry = starch_magnitude_power_uc8_registry; _entry->name; ++_entry) {
        starch_benchmark_one_magnitude_power_uc8( _entry, arg0, arg1, arg2, arg3, arg4 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_magnitude_power_uc8_aligned_benchmark (void);
bool starch_magnitude_power_uc8_aligned_benchmark_verify ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_magnitude_power_uc8_aligned_benchmark(void);

static void starch_benchmark_one_magnitude_power_uc8_aligned( starch_magnitude_power_uc8_aligned_regentry * _entry, const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2, arg3, arg4 );

    /* verify correctness of the output */
    if (! starch_magnitude_power_uc8_aligned_benchmark_verify ( arg0, arg1, arg2, arg3, arg4 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3, arg4 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3, arg4 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "magnitude_power_uc8_aligned";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_magnitude_power_uc8_aligned( const uc8_t * arg0, uint16_t * arg1, unsigned arg2, double * arg3, double * arg4 )
{
    for (starch_magnitude_power_uc8_aligned_regentry *_entry = starch_magnitude_power_uc8_aligned_registry; _entry->name; ++_entry) {
        starch_benchmark_one_magnitude_power_uc8_aligned( _entry, arg0, arg1, arg2, arg3, arg4 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_magnitude_sc16_benchmark (void);
bool starch_magnitude_sc16_benchmark_verify ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_magnitude_sc16_benchmark(void);

static void starch_benchmark_one_magnitude_sc16( starch_magnitude_sc16_regentry * _entry, const sc16_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2 );

    /* verify correctness of the output */
    if (! starch_magnitude_sc16_benchmark_verify ( arg0, arg1, arg2 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "magnitude_sc16";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_magnitude_sc16( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    for (starch_magnitude_sc16_regentry *_entry = starch_magnitude_sc16_registry; _entry->name; ++_entry) {
        starch_benchmark_one_magnitude_sc16( _entry, arg0, arg1, arg2 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_magnitude_sc16_aligned_benchmark (void);
bool starch_magnitude_sc16_aligned_benchmark_verify ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_magnitude_sc16_aligned_benchmark(void);

static void starch_benchmark_one_magnitude_sc16_aligned( starch_magnitude_sc16_aligned_regentry * _entry, const sc16_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2 );

    /* verify correctness of the output */
    if (! starch_magnitude_sc16_aligned_benchmark_verify ( arg0, arg1, arg2 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "magnitude_sc16_aligned";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_magnitude_sc16_aligned( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    for (starch_magnitude_sc16_aligned_regentry *_entry = starch_magnitude_sc16_aligned_registry; _entry->name; ++_entry) {
        starch_benchmark_one_magnitude_sc16_aligned( _entry, arg0, arg1, arg2 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_magnitude_sc16q11_benchmark (void);
bool starch_magnitude_sc16q11_benchmark_verify ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_magnitude_sc16q11_benchmark(void);

static void starch_benchmark_one_magnitude_sc16q11( starch_magnitude_sc16q11_regentry * _entry, const sc16_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2 );

    /* verify correctness of the output */
    if (! starch_magnitude_sc16q11_benchmark_verify ( arg0, arg1, arg2 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "magnitude_sc16q11";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_magnitude_sc16q11( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    for (starch_magnitude_sc16q11_regentry *_entry = starch_magnitude_sc16q11_registry; _entry->name; ++_entry) {
        starch_benchmark_one_magnitude_sc16q11( _entry, arg0, arg1, arg2 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_magnitude_sc16q11_aligned_benchmark (void);
bool starch_magnitude_sc16q11_aligned_benchmark_verify ( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_magnitude_sc16q11_aligned_benchmark(void);

static void starch_benchmark_one_magnitude_sc16q11_aligned( starch_magnitude_sc16q11_aligned_regentry * _entry, const sc16_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2 );

    /* verify correctness of the output */
    if (! starch_magnitude_sc16q11_aligned_benchmark_verify ( arg0, arg1, arg2 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "magnitude_sc16q11_aligned";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_magnitude_sc16q11_aligned( const sc16_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    for (starch_magnitude_sc16q11_aligned_regentry *_entry = starch_magnitude_sc16q11_aligned_registry; _entry->name; ++_entry) {
        starch_benchmark_one_magnitude_sc16q11_aligned( _entry, arg0, arg1, arg2 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_magnitude_uc8_benchmark (void);
bool starch_magnitude_uc8_benchmark_verify ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_magnitude_uc8_benchmark(void);

static void starch_benchmark_one_magnitude_uc8( starch_magnitude_uc8_regentry * _entry, const uc8_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2 );

    /* verify correctness of the output */
    if (! starch_magnitude_uc8_benchmark_verify ( arg0, arg1, arg2 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "magnitude_uc8";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_magnitude_uc8( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    for (starch_magnitude_uc8_regentry *_entry = starch_magnitude_uc8_registry; _entry->name; ++_entry) {
        starch_benchmark_one_magnitude_uc8( _entry, arg0, arg1, arg2 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_magnitude_uc8_aligned_benchmark (void);
bool starch_magnitude_uc8_aligned_benchmark_verify ( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_magnitude_uc8_aligned_benchmark(void);

static void starch_benchmark_one_magnitude_uc8_aligned( starch_magnitude_uc8_aligned_regentry * _entry, const uc8_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2 );

    /* verify correctness of the output */
    if (! starch_magnitude_uc8_aligned_benchmark_verify ( arg0, arg1, arg2 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "magnitude_uc8_aligned";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_magnitude_uc8_aligned( const uc8_t * arg0, uint16_t * arg1, unsigned arg2 )
{
    for (starch_magnitude_uc8_aligned_regentry *_entry = starch_magnitude_uc8_aligned_registry; _entry->name; ++_entry) {
        starch_benchmark_one_magnitude_uc8_aligned( _entry, arg0, arg1, arg2 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_mean_power_u16_benchmark (void);
bool starch_mean_power_u16_benchmark_verify ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_mean_power_u16_benchmark(void);

static void starch_benchmark_one_mean_power_u16( starch_mean_power_u16_regentry * _entry, const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2, arg3 );

    /* verify correctness of the output */
    if (! starch_mean_power_u16_benchmark_verify ( arg0, arg1, arg2, arg3 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "mean_power_u16";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_mean_power_u16( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 )
{
    for (starch_mean_power_u16_regentry *_entry = starch_mean_power_u16_registry; _entry->name; ++_entry) {
        starch_benchmark_one_mean_power_u16( _entry, arg0, arg1, arg2, arg3 );
    }
}

/* prototypes for benchmark helpers provided by user code */
void starch_mean_power_u16_aligned_benchmark (void);
bool starch_mean_power_u16_aligned_benchmark_verify ( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 );

/* prototype the benchmarking function so that we can build with -Wmissing-declarations */
void starch_mean_power_u16_aligned_benchmark(void);

static void starch_benchmark_one_mean_power_u16_aligned( starch_mean_power_u16_aligned_regentry * _entry, const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 )
{
    fprintf(stderr, "  %-40s  ", _entry->name);

    /* test for support */
    if (_entry->flavor_supported && !(_entry->flavor_supported())) {
        fprintf(stderr, "unsupported\n");
        return;
    }

    if (starch_benchmark_flavor_whitelist && !starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_whitelist)) {
        fprintf(stderr, "skipped (not whitelisted)\n");
        return;
    }

    if (starch_benchmark_flavor_blacklist && starch_benchmark_flavor_in_list(_entry->flavor, starch_benchmark_flavor_blacklist)) {
        fprintf(stderr, "skipped (blacklisted)\n");
        return;
    }

    if (starch_benchmark_list_only) {
        fprintf(stderr, "supported\n");
        return;
    }

    /* initial warmup */
    for (unsigned _loop = 0; _loop < starch_benchmark_warmup_loops; ++_loop)
        _entry->callable ( arg0, arg1, arg2, arg3 );

    /* verify correctness of the output */
    if (! starch_mean_power_u16_aligned_benchmark_verify ( arg0, arg1, arg2, arg3 )) {
        fprintf(stderr, "skipped (verification failed)\n");
        starch_benchmark_validation_failed = true;
        return;
    }
    if (starch_benchmark_validate_only) {
        fprintf(stderr, "validation ok\n");
        return;
    }

    /* pre-benchmark, find a loop count that takes at least 100ms */
    starch_benchmark_time _start, _end;
    uint64_t _elapsed = 0;
    uint64_t _loops = 127;
    while (_elapsed < 100000000) {
        _loops *= 2;
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3 );
        starch_benchmark_get_time(&_end);
        _elapsed = starch_benchmark_elapsed(&_start, &_end);
    }

    /* real benchmark, run for approx 1 second */
    _loops = _loops * 1000000000 / _elapsed;

    _elapsed = 0;
    uint64_t _elapsed_min = UINT64_MAX;
    uint64_t _elapsed_max = 0;
    for (unsigned _iter = 0; _iter < starch_benchmark_iterations; ++_iter) {
        starch_benchmark_get_time(&_start);
        for (uint64_t _loop = 0; _loop < _loops; ++_loop)
            _entry->callable ( arg0, arg1, arg2, arg3 );
        starch_benchmark_get_time(&_end);
        uint64_t _elapsed_one = starch_benchmark_elapsed(&_start, &_end);
        if (_elapsed_one < _elapsed_min)
            _elapsed_min = _elapsed_one;
        if (_elapsed_one > _elapsed_max)
            _elapsed_max = _elapsed_one;
        _elapsed += _elapsed_one;
    }

    uint64_t _per_loop;
    if (starch_benchmark_iterations > 2)
        _per_loop = (_elapsed - _elapsed_min - _elapsed_max) / _loops / (starch_benchmark_iterations - 2);
    else
        _per_loop = _elapsed / _loops / starch_benchmark_iterations;

    fprintf(stderr, "%" PRIu64 " ns/call\n", _per_loop);

    if (starch_benchmark_result_count >= starch_benchmark_result_size) {
        if (!starch_benchmark_result_size)
            starch_benchmark_result_size = 64;
        else
            starch_benchmark_result_size *= 2;
        starch_benchmark_results = realloc(starch_benchmark_results, starch_benchmark_result_size * sizeof(*starch_benchmark_results));
        if (!starch_benchmark_results) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            exit(1);
        }
    }

    starch_benchmark_results[starch_benchmark_result_count].name = "mean_power_u16_aligned";
    starch_benchmark_results[starch_benchmark_result_count].impl = _entry->name;
    starch_benchmark_results[starch_benchmark_result_count].ns = _per_loop;
    ++starch_benchmark_result_count;
}

static void starch_benchmark_run_mean_power_u16_aligned( const uint16_t * arg0, unsigned arg1, double * arg2, double * arg3 )
{
    for (starch_mean_power_u16_aligned_regentry *_entry = starch_mean_power_u16_aligned_registry; _entry->name; ++_entry) {
        starch_benchmark_one_mean_power_u16_aligned( _entry, arg0, arg1, arg2, arg3 );
    }
}


#undef STARCH_ALIGNMENT

#define STARCH_ALIGNMENT 1
#define STARCH_ALIGNED(_ptr) (_ptr)
#define STARCH_SYMBOL(_name) starch_ ## _name ## _benchmark_sym
#define STARCH_IMPL(_function,_impl) starch_ ## _function ## _ ## _impl ## _dummy_benchmark
#define STARCH_IMPL_REQUIRES(_function,_impl,_feature) STARCH_IMPL(_function,_impl)
#define STARCH_BENCHMARK(_function) starch_ ## _function ## _benchmark
#define STARCH_BENCHMARK_VERIFY(_function) starch_ ## _function ## _benchmark_verify
#define STARCH_BENCHMARK_RUN(_function, ...) starch_benchmark_run_ ## _function ( __VA_ARGS__ )
#define STARCH_BENCHMARK_ALLOC(_count, _type) ((_type *) starch_benchmark_aligned_alloc(1, alignof(_type), (_count) * sizeof(_type)))
#define STARCH_BENCHMARK_FREE(_ptr) starch_benchmark_aligned_free(_ptr)

#include "../benchmark/count_above_u16_benchmark.c"
#include "../benchmark/magnitude_power_uc8_benchmark.c"
#include "../benchmark/magnitude_sc16_benchmark.c"
#include "../benchmark/magnitude_sc16q11_benchmark.c"
#include "../benchmark/magnitude_uc8_benchmark.c"
#include "../benchmark/mean_power_u16_benchmark.c"

#undef STARCH_ALIGNMENT
#undef STARCH_ALIGNED
#undef STARCH_SYMBOL
#undef STARCH_IMPL
#undef STARCH_IMPL_REQUIRES
#undef STARCH_BENCHMARK
#undef STARCH_BENCHMARK_VERIFY
#undef STARCH_BENCHMARK_RUN
#undef STARCH_BENCHMARK_ALLOC
#undef STARCH_BENCHMARK_FREE

#define STARCH_ALIGNMENT STARCH_MIX_ALIGNMENT
#define STARCH_ALIGNED(_ptr) (__builtin_assume_aligned((_ptr), STARCH_MIX_ALIGNMENT))
#define STARCH_SYMBOL(_name) starch_ ## _name ## _aligned_benchmark_sym
#define STARCH_IMPL(_function,_impl) starch_ ## _function ## _aligned_ ## _impl ## _dummy_benchmark
#define STARCH_IMPL_REQUIRES(_function,_impl,_feature) STARCH_IMPL(_function,_impl)
#define STARCH_BENCHMARK(_function) starch_ ## _function ## _aligned_benchmark
#define STARCH_BENCHMARK_VERIFY(_function) starch_ ## _function ## _aligned_benchmark_verify
#define STARCH_BENCHMARK_RUN(_function, ...) starch_benchmark_run_ ## _function ## _aligned ( __VA_ARGS__ )
#define STARCH_BENCHMARK_ALLOC(_count, _type) ((_type *) starch_benchmark_aligned_alloc(STARCH_MIX_ALIGNMENT, alignof(_type), (_count) * sizeof(_type)))
#define STARCH_BENCHMARK_FREE(_ptr) starch_benchmark_aligned_free(_ptr)

#include "../benchmark/count_above_u16_benchmark.c"
#include "../benchmark/magnitude_power_uc8_benchmark.c"
#include "../benchmark/magnitude_sc16_benchmark.c"
#include "../benchmark/magnitude_sc16q11_benchmark.c"
#include "../benchmark/magnitude_uc8_benchmark.c"
#include "../benchmark/mean_power_u16_benchmark.c"

static void starch_benchmark_all_count_above_u16(void)
{
    fprintf(stderr, "==== count_above_u16 ===\n");
    starch_count_above_u16_benchmark ();
}
static void starch_benchmark_all_count_above_u16_aligned(void)
{
    fprintf(stderr, "==== count_above_u16_aligned ===\n");
    starch_count_above_u16_aligned_benchmark ();
}
static void starch_benchmark_all_magnitude_power_uc8(void)
{
    fprintf(stderr, "==== magnitude_power_uc8 ===\n");
    starch_magnitude_power_uc8_benchmark ();
}
static void starch_benchmark_all_magnitude_power_uc8_aligned(void)
{
    fprintf(stderr, "==== magnitude_power_uc8_aligned ===\n");
    starch_magnitude_power_uc8_aligned_benchmark ();
}
static void starch_benchmark_all_magnitude_sc16(void)
{
    fprintf(stderr, "==== magnitude_sc16 ===\n");
    starch_magnitude_sc16_benchmark ();
}
static void starch_benchmark_all_magnitude_sc16_aligned(void)
{
    fprintf(stderr, "==== magnitude_sc16_aligned ===\n");
    starch_magnitude_sc16_aligned_benchmark ();
}
static void starch_benchmark_all_magnitude_sc16q11(void)
{
    fprintf(stderr, "==== magnitude_sc16q11 ===\n");
    starch_magnitude_sc16q11_benchmark ();
}
static void starch_benchmark_all_magnitude_sc16q11_aligned(void)
{
    fprintf(stderr, "==== magnitude_sc16q11_aligned ===\n");
    starch_magnitude_sc16q11_aligned_benchmark ();
}
static void starch_benchmark_all_magnitude_uc8(void)
{
    fprintf(stderr, "==== magnitude_uc8 ===\n");
    starch_magnitude_uc8_benchmark ();
}
static void starch_benchmark_all_magnitude_uc8_aligned(void)
{
    fprintf(stderr, "==== magnitude_uc8_aligned ===\n");
    starch_magnitude_uc8_aligned_benchmark ();
}
static void starch_benchmark_all_mean_power_u16(void)
{
    fprintf(stderr, "==== mean_power_u16 ===\n");
    starch_mean_power_u16_benchmark ();
}
static void starch_benchmark_all_mean_power_u16_aligned(void)
{
    fprintf(stderr, "==== mean_power_u16_aligned ===\n");
    starch_mean_power_u16_aligned_benchmark ();
}

static int starch_benchmark_compare_result(const void *a, const void *b)
{
    const starch_benchmark_result *left = (const starch_benchmark_result *) a;
    const starch_benchmark_result *right = (const starch_benchmark_result *) b;

    int name_cmp = strcmp(left->name, right->name);
    if (name_cmp)
        return name_cmp;

    if (left->ns < right->ns)
        return -1;
    if (left->ns > right->ns)
        return 1;
    return 0;
}

static void starch_benchmark_usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [OPTION]... [FUNCTION]...\n"
        "Benchmarks starch functions and optionally writes a sorted wisdom file.\n"
        "\n"
        "  -r FILE          Read initial wisdom from FILE\n"
        "  -o FILE          Write sorted wisdom to FILE\n"
        "  -F FLAVOR        Add FLAVOR to whitelist\n"
        "                     (default: no whitelist, run all runtime-supported flavors)\n"
        "  -N FLAVOR        Add FLAVOR to blacklist\n"
        "                     (default: no blacklist, run all runtime-supported flavors)\n"
        "  -l               List compiled-in implementations but don't benchmark them\n"
        "  -V               Run validation tests, but don't run benchmarks\n"
        "  -t               Include only the top candidate per function in wisdom output\n"
        "  -i ITERS         Run benchmark ITERS times and use the mean. If ITERS > 2, ignore\n"
        "                   the smallest and largest runs when calculating the mean.\n"
        "                     (default: 1 iteration)\n"
        "  FUNCTION         Run benchmarks for these functions only\n"
        "                     (default: benchmark all functions)\n"
        "\n"
        "Supported flavors:   "
#ifdef STARCH_FLAVOR_ARMV7A_NEON_VFPV4
          "armv7a_neon_vfpv4 "
#endif
#ifdef STARCH_FLAVOR_ARMV8_NEON_SIMD
          "armv8_neon_simd "
#endif
#ifdef STARCH_FLAVOR_GENERIC
          "generic "
#endif
#ifdef STARCH_FLAVOR_X86_AVX2
          "x86_avx2 "
#endif
          "\n"
        "Supported functions: "
          "count_above_u16 "
          "count_above_u16_aligned "
          "magnitude_power_uc8 "
          "magnitude_power_uc8_aligned "
          "magnitude_sc16 "
          "magnitude_sc16_aligned "
          "magnitude_sc16q11 "
          "magnitude_sc16q11_aligned "
          "magnitude_uc8 "
          "magnitude_uc8_aligned "
          "mean_power_u16 "
          "mean_power_u16_aligned "
          "\n", argv0);
}

static void starch_benchmark_append_flavor(const char *flavor, starch_benchmark_flavor_list **list)
{
    starch_benchmark_flavor_list *newnode = malloc(sizeof(*newnode));
    if (!newnode) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        exit(1);
    }

    newnode->flavor = flavor;
    newnode->next = *list;
    *list = newnode;
}

int main(int argc, char **argv)
{
    int specific = 0;
    const char *output_path = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "r:o:F:N:i:lhtV")) != -1) {
        switch (opt) {
        case 'r':
            if (starch_read_wisdom(optarg) < 0) {
                fprintf(stderr, "%s: cannot read %s: %s\n", argv[0], optarg, strerror(errno));
                return 1;
            }
            fprintf(stderr, "%s: loaded wisdom file %s\n", argv[0], optarg);
            break;

        case 'o':
            output_path = optarg;
            break;

        case 'F':
            if (starch_benchmark_flavor_in_list(optarg, starch_benchmark_flavor_blacklist)) {
                fprintf(stderr, "%s: conflicting -F and -N options for flavor %s\n", argv[0], optarg);
                return 2;
            }
            starch_benchmark_append_flavor(optarg, &starch_benchmark_flavor_whitelist);
            break;

        case 'N':
            if (starch_benchmark_flavor_in_list(optarg, starch_benchmark_flavor_whitelist)) {
                fprintf(stderr, "%s: conflicting -F and -N options for flavor %s\n", argv[0], optarg);
                return 2;
            }
            starch_benchmark_append_flavor(optarg, &starch_benchmark_flavor_blacklist);
            break;

        case 'l':
            starch_benchmark_list_only = true;
            break;

        case 't':
            starch_benchmark_top_only = true;
            break;

        case 'i':
            starch_benchmark_iterations = atoi(optarg);
            break;

        case 'V':
            starch_benchmark_validate_only = true;
            break;

        case 'h':
            starch_benchmark_usage(argv[0]);
            return 0;

        case '?':
        default:
            starch_benchmark_usage(argv[0]);
            return 2;
        }
    }

    if (starch_benchmark_list_only && output_path) {
        fprintf(stderr, "%s: -o and -l options cannot be specified together\n", argv[0]);
        return 2;
    }

    for (int i = optind; i < argc; ++i) {
        if (!strcmp(argv[i], "count_above_u16")) {
            specific = 1;
            starch_benchmark_all_count_above_u16();
            continue;
        }
        if (!strcmp(argv[i], "count_above_u16_aligned")) {
            specific = 1;
            starch_benchmark_all_count_above_u16_aligned();
            continue;
        }
        if (!strcmp(argv[i], "magnitude_power_uc8")) {
            specific = 1;
            starch_benchmark_all_magnitude_power_uc8();
            continue;
        }
        if (!strcmp(argv[i], "magnitude_power_uc8_aligned")) {
            specific = 1;
            starch_benchmark_all_magnitude_power_uc8_aligned();
            continue;
        }
        if (!strcmp(argv[i], "magnitude_sc16")) {
            specific = 1;
            starch_benchmark_all_magnitude_sc16();
            continue;
        }
        if (!strcmp(argv[i], "magnitude_sc16_aligned")) {
            specific = 1;
            starch_benchmark_all_magnitude_sc16_aligned();
            continue;
        }
        if (!strcmp(argv[i], "magnitude_sc16q11")) {
            specific = 1;
            starch_benchmark_all_magnitude_sc16q11();
            continue;
        }
        if (!strcmp(argv[i], "magnitude_sc16q11_aligned")) {
            specific = 1;
            starch_benchmark_all_magnitude_sc16q11_aligned();
            continue;
        }
        if (!strcmp(argv[i], "magnitude_uc8")) {
            specific = 1;
            starch_benchmark_all_magnitude_uc8();
            continue;
        }
        if (!strcmp(argv[i], "magnitude_uc8_aligned")) {
            specific = 1;
            starch_benchmark_all_magnitude_uc8_aligned();
            continue;
        }
        if (!strcmp(argv[i], "mean_power_u16")) {
            specific = 1;
            starch_benchmark_all_mean_power_u16();
            continue;
        }
        if (!strcmp(argv[i], "mean_power_u16_aligned")) {
            specific = 1;
            starch_benchmark_all_mean_power_u16_aligned();
            continue;
        }

        fprintf(stderr, "%s: unrecognized function name: %s\n", argv[0], argv[i]);
        return 2;
    }

    if (!specific) {
        starch_benchmark_all_count_above_u16();
        starch_benchmark_all_count_above_u16_aligned();
        starch_benchmark_all_magnitude_power_uc8();
        starch_benchmark_all_magnitude_power_uc8_aligned();
        starch_benchmark_all_magnitude_sc16();
        starch_benchmark_all_magnitude_sc16_aligned();
        starch_benchmark_all_magnitude_sc16q11();
        starch_benchmark_all_magnitude_sc16q11_aligned();
        starch_benchmark_all_magnitude_uc8();
        starch_benchmark_all_magnitude_uc8_aligned();
        starch_benchmark_all_mean_power_u16();
        starch_benchmark_all_mean_power_u16_aligned();
    }

    if (output_path) {
        FILE *out = fopen(output_path, "w");
        if (!out) {
            fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], output_path, strerror(errno));
            return 1;
        }

        fprintf(out, "# generated by ");
        for (int i = 0; i < argc; ++i)
            fprintf(out, "%s ", argv[i]);
        fprintf(out, "\n\n");

        qsort(starch_benchmark_results, starch_benchmark_result_count, sizeof(*starch_benchmark_results), starch_benchmark_compare_result);

        const char *last_name = NULL;
        bool first = true;
        for (unsigned i = 0; i < starch_benchmark_result_count; ++i) {
            starch_benchmark_result *result = &starch_benchmark_results[i];
            if (last_name && strcmp(last_name, result->name) != 0) {
                fprintf(out, "\n");
                first = true;
            }
            last_name = result->name;
            if (starch_benchmark_top_only && !first)
                continue;
            fprintf(out, "%-40s %-40s  # %" PRIu64 " ns/call\n", result->name, result->impl, result->ns);
            first = false;
        }

        fclose(out);
        fprintf(stderr, "%s: wrote sorted wisdom to %s\n", argv[0], output_path);
    }

    return starch_benchmark_validation_failed ? 1 : 0;
}
