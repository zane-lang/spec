# langbench

The same programs written in Zane, C, C++, Rust, Go, D and Zig, each built by
its language's release compiler and timed as a whole process. Where
[`memorybench/`](../memorybench/) models the memory design in C, this bench
measures what a released Zane compiler actually produces.

## Layout

| path | what it is |
| --- | --- |
| `zane.coda`, `zane-lock.coda` | the Zane project: the pinned compiler release and `core` |
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

The zane CLI must be on `PATH`, with the release `zane.coda` names installed
(`zane toolchain install`), and so must every other language's compiler.

```sh
python3 langbench/langbench.py --quick        # small sizes, one run: check everything builds and agrees
python3 langbench/langbench.py                # build, check, time, render
python3 langbench/langbench.py --save         # ... and pin the run
python3 langbench/langbench.py --from-file    # re-render the pinned run
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
  `zane inspect cgt` shows how many loops the optimized build of the Zane
  program keeps compared with the plain build, and the page reports what was
  computed while compiling as it is.

Every language builds at its level-2 optimization, keeps the safety checks it
has by default, and is written the way that language is usually written.
`primes` also has a variant that asks the compiler to evaluate the sieve, in
the languages that can ask.
