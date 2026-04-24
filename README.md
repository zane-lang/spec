# The Zane Language Specification

This repository is a **prior art publication** for the Zane programming language.

The techniques described in these documents were conceived and developed by the author. This work is dedicated to the public domain under [CC0 1.0 Universal](LICENSE) — anyone may use these ideas freely, for any purpose, without restriction or attribution. No patent claims are made by the author. This publication exists to prevent any third party from patenting these techniques by establishing an unambiguous prior art record with a publicly verifiable timestamp.

---

| File                           | Description                                                                                                   |
|--------------------------------|---------------------------------------------------------------------------------------------------------------|
| `memory_model.md`              | Ownership model, refs, anchor system, heap layout, free-stack allocator, struct/class layout, design rationale, language comparisons |
| `dependency_management.md`     | Package identity, manifest, symbol placeholder, global registry, versioned symbols, design rationale          |
| `oop.md`                       | Classes, structs, constructors, methods, free functions, overloading, packages, instanceful pattern, design rationale |
| `purity.md`                    | Effect model, `mut` modifier, inferred purity levels, capability wiring, concurrency implications, design rationale |
| `error_handling.md`            | Bifurcated return paths, `?` handler block, `??` shorthand, `resolve`/`abort`/`return` semantics, design rationale, language comparisons |
| `syntax.md`                    | Canonical syntax reference for all Zane constructs (declarations, types, functions, constructors, calls, error handling) |
| `bench/zane_bench.c`           | C benchmark comparing Zane's allocator against malloc, arena, and pool                                        |
| `bench/zane_bench_results.txt` | Benchmark results (median of 20 runs per test)                                                                |
| `bench/benchmark.html`         | Bar graph showing the benchmark results                                                                       |
