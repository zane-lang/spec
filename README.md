# The Zane Language Specification

This repository is a **prior art publication** for the Zane programming language. The techniques described in these documents were conceived and developed by the author and are dedicated to the public domain under [CC0 1.0 Universal](LICENSE). Anyone may use these ideas freely, for any purpose, without restriction or attribution. No patent claims are made by the author.

---

## Repository layout

```
spec/             ← the specification itself (start here)
contributing/     ← style guide for writing and editing spec docs
bench/            ← reference C harness used for runtime experiments
```

## Specification documents

The specification lives in [`spec/`](spec/) and is organized by topic. Each document has a single canonical home for its rules; everything else cross-references.

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
| [`spec/functions.md`](spec/functions.md) | Methods, free functions, subscripts, overload resolution, function values, and lambdas |
| [`spec/generics.md`](spec/generics.md) | Unified type parameters, `<>` type expressions, constructor calls, and the `Array<'T, n>` primitive |
| [`spec/memory.md`](spec/memory.md) | Ownership, refs, anchors, lifetimes, and heap layout |
| [`spec/effects.md`](spec/effects.md) | The effect model, `mut`, inferred effect levels, and capability wiring |
| [`spec/concurrency.md`](spec/concurrency.md) | Implicit parallelism, `spawn`, water-tower lifetimes, and concurrency safety |

### Programs and program structure

| Document | Purpose |
|---|---|
| [`spec/control-flow.md`](spec/control-flow.md) | `if`/`elif`/`else`, `guard`, counted loops, and 1-based ordinal rules |
| [`spec/operators.md`](spec/operators.md) | Operator set, derived operators, precedence, and boolean keywords |
| [`spec/error-handling.md`](spec/error-handling.md) | Bifurcated return paths, `?` handlers, and abort/resolve semantics |
| [`spec/packages.md`](spec/packages.md) | Package declarations, member access, and the instanceful package pattern |
| [`spec/dependencies.md`](spec/dependencies.md) | Package identity, manifests, version pinning, fetching, and caching |

## Contributing

Style and structural conventions for spec documents live in [`contributing/writing-spec-docs.md`](contributing/writing-spec-docs.md). Read that file before editing or adding a topic doc.
