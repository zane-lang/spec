# langbench

The same programs written in Zane, C, C++, Rust, Go, D, Zig and OCaml, each built by
its language's release compiler and timed as a whole process. Where
[`memorybench/`](../memorybench/) models the memory design in C, this bench
measures what a released Zane compiler actually produces.

## Layout

| path | what it is |
| --- | --- |
| `zane.coda`, `zane-lock.coda` | the Zane project: the pinned compiler release and `core` |
| `devbox.json`, `devbox.lock` | the other languages' compilers, pinned |
| `bin/<test>/main.zn` | each test's Zane program |
| `lib/bench/` | what the Zane programs share: reading the size from the arguments, and the random numbers |
| `c/`, `cpp/`, `rust/`, `go/`, `d/`, `zig/`, `ocaml/` | the other languages, one file per program |
| `tests.py` | each test's size, expected output and fold profile, and each language's build command |
| `langbench.py` | builds, checks, times and renders |
| `template.html` | the page skeleton |
| `explanations.txt` | notes on the pinned results, one block per test |
| `langbench_results.json` | the pinned run |
| `langbench.html` | generated; never hand-edited |

## Running it

[Devbox](https://www.jetify.com/devbox) supplies the other languages'
compilers at the versions `devbox.json` pins: clang 19, the LLVM release the
Zane compiler is built against, for C and C++, then Rust, Go, LDC for D,
Zig 0.16 and OCaml 5.5. Python 3 and the zane CLI come from the machine. The CLI must
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
OCaml's level-2 optimization needs flambda, and nixpkgs builds `ocamlopt`
without it, so OCaml builds at its one level.
`primes` also has a variant that asks the compiler to evaluate the sieve, in
the languages that can ask. The Zig programs allocate through
`std.heap.smp_allocator`, because in ReleaseSafe the allocator `main` is
handed is the leak-checking debug allocator.

`allocfree`, `gameloop` and `particles` draw their random numbers from the
same linear congruential generator in every language, so all of them make
the same choices and print the same result. `nbody` and `spectralnorm` print
floating-point results in billionths, truncated to an integer, and do the
same operations in the same order in every language, so the digits agree.

`binarytrees` uses reference-type nodes with owning child fields. Building a
parent transfers both children into it, matching the owning pointers in C++
and Rust; checking a tree borrows it. `treecopy` intentionally uses value
types, because copying the complete tree is the work that test measures.

The project builds with compiler v0.12
([`a490ae4`](https://github.com/zane-lang/compiler/commit/a490ae476163df4716eb0e94cd447b73b08fca8b))
and `core` v0.1. The pinned 2026-10-16 run used both, with every language and
test above, and was measured on an Intel Core i7-1355U laptop under WSL2 with
zane CLI v0.6; `explanations.txt` describes that run.
