# The Zane Language Specification

This repository is a **prior art publication** for the Zane programming language.

The techniques described in these documents were conceived and developed by the author. This work is dedicated to the public domain under [CC0 1.0 Universal](LICENSE) — anyone may use these ideas freely, for any purpose, without restriction or attribution. No patent claims are made by the author. This publication exists to prevent any third party from patenting these techniques by establishing an unambiguous prior art record with a publicly verifiable timestamp.

---

| File                           | Description                                                                                                   |
|--------------------------------|---------------------------------------------------------------------------------------------------------------|
| `memory_model.md`              | Ownership model, refs, anchor system, supervisor tree, heap layout, free-stack allocator, struct/class layout |
| `dependency_management.md`     | Package identity, manifest, symbol placeholder, global registry, versioned symbols                            |
| `oop.md`                       | Classes, structs, constructors, methods, free functions, overloading, packages, instanceful pattern           |
| `purity.md`                    | Effect model, `mut` modifier, inferred purity levels, capability wiring, concurrency implications             |
| `error_handling.md`            | Bifurcated return paths, `?` handler block, `??` shorthand, `resolve`/`abort`/`return` semantics             |
| `syntax.md`                    | Canonical syntax reference for all Zane constructs (declarations, types, functions, constructors, calls, error handling) |
| `rationale.md`                 | Design decisions and their rationale across all five areas                                                    |
| `comparison.md`                | Comparisons with other languages: memory model vs Rust/C++/GC; error handling vs C/Go/Java/Python/Rust/Swift/Zig |
| `bench/zane_bench.c`           | C benchmark comparing Zane's allocator against malloc, arena, and pool                                        |
| `bench/zane_bench_results.txt` | Benchmark results (median of 20 runs per test)                                                                |
| `bench/benchmark.html`         | Bar graph showing the benchmark results                                                                       |
