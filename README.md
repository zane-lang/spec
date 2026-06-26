# The Zane Language Specification

This repository is a **prior art publication** for the Zane programming language. The techniques described in these documents were conceived and developed by the author and are dedicated to the public domain under [CC0 1.0 Universal](LICENSE). Anyone may use these ideas freely, for any purpose, without restriction or attribution. No patent claims are made by the author.

---

## Repository layout

```
spec/             ← the specification itself (start here): what the language does
rationale/        ← rationale stories: why each spec decision was made
contributing/     ← style guides for writing spec docs and rationale docs
bench/            ← reference C harness used for runtime experiments
```

## Specification documents

The specification lives in [`spec/`](spec/) and is organized by topic. Each document has a single canonical home for its rules; everything else cross-references.

### Foundations

| Document | Purpose |
|---|---|
| [`spec/foundations.md`](spec/foundations.md) | The few cross-cutting commitments the whole spec rests on: captured intent, staged compilation, casing-determines-kind, fixed layout, and strictness-as-performance — start here |

### Reference documents

| Document | Purpose |
|---|---|
| [`spec/syntax.md`](spec/syntax.md) | Canonical surface syntax for every construct |
| [`spec/lexical.md`](spec/lexical.md) | Case-sensitive parsing, identifier formation, and the casing-determines-kind rule |
| [`spec/glossary.md`](spec/glossary.md) | Canonical names for recurring concepts and coined spec terms |

### Type system and runtime model

| Document | Purpose |
|---|---|
| [`spec/types.md`](spec/types.md) | Classes, structs, fields, constructors, implicit conversions, and `type`/`alias` declarations |
| [`spec/adt.md`](spec/adt.md) | Enums, variants, the struct/variant symmetry, pattern matching, `match`, and enum maps |
| [`spec/functions.md`](spec/functions.md) | Methods, free functions, subscripts, overload resolution, function values, and lambdas |
| [`spec/generics.md`](spec/generics.md) | Unified type parameters, `<>` type expressions, constructor calls, and the `Array<T, n>` primitive |
| [`spec/memory.md`](spec/memory.md) | Ownership, refs, anchors, and heap layout |
| [`spec/lifetimes.md`](spec/lifetimes.md) | Lexical lifetime rules, ownership moves, scope rules, and deterministic destruction |
| [`spec/effects.md`](spec/effects.md) | The effect model, `mut`, inferred effect levels, and capability wiring |
| [`spec/concurrency.md`](spec/concurrency.md) | Implicit parallelism, `spawn`, water-tower lifetimes, and concurrency safety |

### Programs and program structure

| Document | Purpose |
|---|---|
| [`spec/control-flow.md`](spec/control-flow.md) | `if`/`elif`/`else`, `guard`, counted loops, and 1-based ordinal rules |
| [`spec/operators.md`](spec/operators.md) | Operator set, derived operators, precedence, and boolean keywords |
| [`spec/error-handling.md`](spec/error-handling.md) | Bifurcated return paths, `?` handlers, and abort/resolve semantics |
| [`spec/packages.md`](spec/packages.md) | Package declarations and member access |
| [`spec/dependencies.md`](spec/dependencies.md) | Package identity, manifests, version pinning, fetching, and caching |

## Design rationale

The spec states *what* the language does; the **why** lives in a parallel set of rationale docs under [`rationale/`](rationale/), one per spec document. Each reads as a set of short stories — the forks, the roads not taken, and the costs behind each decision — kept separate so the spec stays terse while the reasoning has room to breathe.

| Document | Justifies |
|---|---|
| [`rationale/foundations.md`](rationale/foundations.md) | [`spec/foundations.md`](spec/foundations.md) — the bets behind captured intent, staged compilation, casing-determines-kind, and strictness-as-performance |
| [`rationale/generics.md`](rationale/generics.md) | [`spec/generics.md`](spec/generics.md) — the parameter model, the `<>`/`()` split, size-in-the-type, and the deferred features |
| [`rationale/dependencies.md`](rationale/dependencies.md) | [`spec/dependencies.md`](spec/dependencies.md) — URL identity, the manifest/resolution split, prebuilt distribution, symbol-rewriting, and the opt-in remapping model |

## Contributing

Style and structural conventions live in two sibling guides; read the relevant one before editing or adding a document:

- [`contributing/writing-spec-docs.md`](contributing/writing-spec-docs.md) — normative spec documents in [`spec/`](spec/).
- [`contributing/writing-rationale-docs.md`](contributing/writing-rationale-docs.md) — rationale docs in [`rationale/`](rationale/).
