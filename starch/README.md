# starch - a framework for selecting architecture-specific code at runtime

`starch` helps generates glue code to *s*elec*t* *arch*itecture-specific
versions of code depending on the hardware detected at runtime.

It arranges for code to be built multiple times with different compiler
options. At runtime, user code calls a dispatcher entry point which
selects the best compiled version of the versions that can safely run
on the hardware used at runtime.

It tries to be agnostic about the details of the code being generated
and the details of the hardware.

## Caution caution work in progress

This documentation isn't very complete. You'll need to look at the example
and the code itself.

## Design notes

 * Architecture-independent generated output; the generated outputs can
   be generated during development and committed as part of the main
   source code, and at build time starch does not need to be re-run.

 * Doesn't care about the details of the functions you call; they can
   have any signature.

 * Can automatically generate benchmarking code given a benchmarking
   helper that sets up inputs to the function.

 * Does not do any hardware detection itself, and does not care about
   the hardware details; for each combination of compiler flags, the user
   code provides a test function to be called at runtime to determine if
   it is safe to run code compiled with those flags.

 * Allows the same generic code to be compiled multiple times with different
   compile flags to take advantage of compile auto-vectorization that
   requires additional instruction set features (AVX, NEON, ..) being enabled.

 * Emits makefile fragments to be included into a larger makefile structure

## License

The generator script and templates are licensed under a BSD 2-clause license,
see the LICENSE file.

No copyright claim is made on generated code.

## Prerequisites

At generation time (results can be committed to version control):

 * Python 3
 * [Mako](https://www.makotemplates.org/)

At build time:

 * a C compiler
 * make

## Quickstart

Look in example/ for a full example.

## Concepts

A *function* is the user-visible API to starch-generated code. It just looks
like a C function pointer. Initially, this pointer points to a dispatcher
routine which will select an appropriate implementation at runtime and call
it. For subsequent calls, the dispatcher updates the function pointer to
point directly to the selected implementation.

A *function impl* is one particular way of implementing a function. All
impls should produce the same results given the same inputs to avoid confusing
user code. There may be different impls with different performance
characteristics - for example, different degrees of manual loop unrolling, or
an impl that takes advantage of a particular instruction set (NEON, AVX, etc).
Each impl has a unique-within-the-function "variant" name that identifies it.

Function impls may be conditionally compiled depending on build features
(see below). This is useful for impls that cannot always be compiled e.g.
they depend on the availability of a particular instruction set.

A *build flavor* is a particular way of building the function impl. It
consists of a set of compiler flags to use, plus an associated test function
that determines at runtime if it is safe to run the code. For example,
a flavor may enable use of specific instructions that may or may not be
available at runtime via `-mavx`, `-march=...`, and similar flags. Each
flavor declares that it provides zero or more *features*.

A *feature* is a characteristic of the build flavor compiler flags that
allows certain impls to be compiled. For example, an impl that uses NEON
intrinsics can only be compiled if the compiler is building for an ARM
instruction set that supports NEON. Features are defined in the build flavor,
and are advertised at compile time by the presence of a `STARCH_FEATURE_x`
macro; implementations may conditionally compile on this macro and should use
`STARCH_IMPL_REQUIRES` to indicate they will only be emitted when a given
feature is present.

A *build mix* is a combination of build flavors that can coexist in the same
binary. For example, an "x86" mix might include build flavors that build
for generic x86, x86-with-AVX, and x86-with-AVX2; but it would not include
a build flavor for ARM, because ARM and x86 object code can't be linked
together into a single binary.

## Alignment

A function can optionally include an aligned version; this is a version of the
function with an independent call point and wisdom, which assumes that
data passed to the function is already aligned. Each flavor has an associated
alignment in bytes, but otherwise it is up to the implementations to decide
what exactly is aligned. Implementations for an aligned function on a flavor
that specifies an alignment (>1 byte) will be compiled twice, once with an
alignment of 1 and once with the flavor's alignment, to generate two different
compiled versions.

starch provides macros to help with alignment:

 * `STARCH_ALIGNMENT`, in implementations, is the alignment (in bytes) that
   implementations can assume.
 * `STARCH_MIX_ALIGNMENT`, defined in the generated header file, is the required
   alignment (in bytes) for callers of the _aligned version of a function.
   It is the largest alignment of all flavors in the mix.
 * `STARCH_ALIGNED(ptr)` in implementations evaluates to `ptr` while hinting to
   the compiler that the data is aligned according to STARCH_ALIGNMENT. This
   maps to gcc's `__builtin_assume_aligned` builtin.

## Benchmarks

Functions can optionally provide a benchmark helper by defining a
(no args, void return typer) function using the STARCH_BENCHMARK macro. This
macro is only present when benchmark code is being compiled.

The benchmark helper should set up function inputs for benchmarking and then
use the `STARCH_BENCHMARK_RUN` macro. This macro expands to code that will
benchmark each possible impl in turn with the provided arguments.

If the benchmark needs to allocated possibly-aligned buffers,
two macros `STARCH_BENCHMARK_ALLOC` and `STARCH_BENCHMARK_FREE`
will allocate suitably aligned buffers for the current `STARCH_ALIGNMENT`
value. `STARCH_BENCHMARK_ALLOC(count,type)` will allocate `count` elements of
type `type`, aligned to either `STARCH_ALIGNMENT` or the required alignment
for `type`, whichever is larger. `STARCH_BENCHMARK_FREE(ptr)` will free a
buffer previously allocated by `STARCH_BENCHMARK_ALLOC`.

See `example/benchmark/subtract_n_benchmark.c` for examples.

## Gotchas

Files added by `scan_file` are `#include`-d into surrounding support files.
Multiple files may be included into the same compilation unit. You should
ensure that you don't pollute the global namespace (macros, static functions
names, etc) for subsequent files that will follow.

Files added by `scan_file` will be compiled multiple times. You should ensure
that any symbols other than those handled by STARCH_IMPL / STARCH_IMPL_REQUIRES
are either static or use the STARCH_SYMBOL macro to get a unique name for
this compilation pass.

You probably want to separate out benchmark-support code into separate files
to avoid an extra version of any impls present in the same file from being
emitted.

## Wisdom

There is partial support for a wisdom implementation. Wisdom is a priori
information about the preferred code to use for a given function, for example
as the result of benchmarking to find the fastest version. It is simply the
order in which compiled impls are tried until one that is supported is found.

To set wisdom, there are two options:

1) Provide a wisdom ordering for the function when defining a build mix. This
controls the order in which the compiled impls are included in the generated
registry that is searched at runtime.

2) Call `starch_<function>_set_wisdom` at runtime. This accepts an array of
function variants, terminated by NULL. When called, the registry is re-sorted
to prefer the listed variants in the order provided (and the function pointer
is reset to the dispatcher so the chosen code will be re-selected on the next
call). This could be used to load install-specific wisdom during program
startup.
