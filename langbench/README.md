# langbench

The same programs written in Zane, C, C++, Rust, Go, D and Zig, each built by
its language's release compiler and timed as a whole process. Where
[`memorybench/`](../memorybench/) models the memory design in C, this bench
measures what a released Zane compiler actually produces.

## Layout

| path | what it is |
| --- | --- |
| `zane.coda`, `zane-lock.coda` | the Zane project: the pinned compiler release and `core` |
| `devbox.json`, `devbox.lock` | the other languages' compilers, pinned |
| `bin/<test>/main.zn` | each test's Zane program |
| `lib/bench/` | what the Zane programs share: reading the size from the arguments |
| `c/`, `cpp/`, `rust/`, `go/`, `d/`, `zig/` | the other languages, one file per program |
| `tests.py` | each test's size, expected output and fold profile, and each language's build command |
| `langbench.py` | builds, checks, times and renders |
| `template.html` | the page skeleton |
| `explanations.txt` | notes on the pinned results, one block per test |
| `langbench_results.json` | the pinned run |
| `langbench.html` | generated; never hand-edited |

## Running it

[Devbox](https://www.jetify.com/devbox) supplies the other languages'
compilers at the versions `devbox.json` pins: clang 19, the LLVM release the
Zane compiler is built against, for C and C++, then Rust, Go, LDC for D, and
Zig 0.16. Python 3 and the zane CLI come from the machine. The CLI must
be v0.4 or later, the first release with `zane inspect`, and the compiler
release `zane.coda` names must be installed (`zane toolchain install`). Run from `langbench/`:

```sh
devbox run -- python3 langbench.py --quick        # small sizes, one run: check everything builds and agrees
devbox run -- python3 langbench.py                # build, check, time, render
devbox run -- python3 langbench.py --save         # ... and pin the run
python3 langbench.py --from-file                  # re-render the pinned run
```

A plain run renders the page from what it measured and leaves the pinned run
alone; `--save` replaces it. Pin only from a machine whose numbers the notes
in `explanations.txt` can describe, and update those notes in the same change.

## What is measured

- **Output.** Every program prints its result, and each must print exactly
  what `tests.py` expects at the test's size before it is timed.
- **Run time.** One untimed run, then five timed runs of the whole process;
  the page shows the median and the spread.
- **Build time.** Each program built once, with its language's standard
  library and its dependencies already built: `core` for Zane, and for Go and
  Zig a small program built first into a fresh cache. Zig compiles the parts
  of its standard library a program uses as part of that program, so its
  build time includes them.
- **Folding.** Each test declares a fold profile: `static` reads no input,
  `setup` has a fixed part that needs none, and `input` reads its size first.
  `zane inspect cgt` supplies raw loop counts for the plain and optimized
  CGT. Optimized builds now include imported dependency bodies that plain
  builds omit, so these whole-tree counts do not measure how much was
  computed while compiling. The page reports them without a folding score.

Every language builds at its level-2 optimization, keeps the safety checks it
has by default, and is written the way that language is usually written.
`primes` also has a variant that asks the compiler to evaluate the sieve, in
the languages that can ask. The Zig programs allocate through
`std.heap.smp_allocator`, because in ReleaseSafe the allocator `main` is
handed is the leak-checking debug allocator.

`binarytrees` uses reference-type nodes with owning child fields. Building a
parent transfers both children into it, matching the owning pointers in C++
and Rust; checking a tree borrows it. `treecopy` intentionally uses value
types, because copying the complete tree is the work that test measures.

The pinned 2026-10-08 run uses compiler v0.8 with cross-package optimization
and the reference-based binary-tree program. It was measured on an Intel
Core i7-1355U laptop under WSL2; `explanations.txt` describes that run.

## Tests that need more of the language

These tests are planned and cannot be written in idiomatic Zane with the
compiler release this project pins. Each names what it is missing.

| test | what it measures | what Zane needs first |
| --- | --- | --- |
| n-body | floating-point physics over a few bodies (Benchmarks Game) | a square root |
| spectral-norm | repeated matrix-vector products (Benchmarks Game) | a square root, for the final norm |
| alloc and release | allocating many small objects and releasing them in random order (memorybench Tests 1 and 2) | removing an element from a `List` |
| game loop | entities spawned, updated and killed every frame (memorybench Test 7) | removing an element from a `List` |
| particles | short-lived objects spawned in bursts and expired (memorybench Test 8) | removing an element from a `List` |
| concurrent scan | four shards of one array summed on four threads (memorybench Test 12) | `spawn` in the compiler |

A square root is a math function, and the language has none yet. `core`'s
`List` can be appended to, indexed and overwritten, but not shrunk.
`concurrency.md` specifies `spawn`, but the compiler does not implement it.
