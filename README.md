# The Zane Language Specification

This repository is a **prior art publication** for the Zane programming language. The techniques described in these documents were conceived and developed by the author and are dedicated to the public domain under [CC0 1.0 Universal](LICENSE). Anyone may use these ideas freely, for any purpose, without restriction or attribution. No patent claims are made by the author.

---

## Repository layout

```
spec/             ← the specification itself (start here): what the language does
stories/          ← design stories: how each part of the spec came to be
contributing/     ← style guides for writing spec docs and stories docs
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

## Design stories

The spec states *what* the language does; the **why** lives in a parallel set of stories docs under [`stories/`](stories/), one per spec document. Each reads as a history — a handful of thematic chapters that recount, in the order the thinking moved, the situation that forced a choice, the roads not taken, and the costs accepted — kept separate so the spec stays terse while the reasoning has room to breathe. They are appended to as the design evolves, so each is the record of how its part of the spec came to be, not just why it is the way it is now.

| Document | Tells the story behind |
|---|---|
| [`stories/foundations.md`](stories/foundations.md) | [`spec/foundations.md`](spec/foundations.md) — the bets behind captured intent, staged compilation, casing-determines-kind, strictness-as-performance, and the collapse of value/reference into one `#` axis |
| [`stories/types.md`](stories/types.md) | [`spec/types.md`](spec/types.md) — the two-axis value/reference model, why `#` marks a distinct type rather than a box, constructing a type by what it is, confining `#` to the body forms, and why every type must be named |
| [`stories/adt.md`](stories/adt.md) | [`spec/adt.md`](spec/adt.md) — splitting `enum` from `variant` against the hype, the shared struct body, escaping the matcher machine with case overloads and the turn to a central `match` block, matching variants rather than patterns, keeping enum data outside the members, and reducing a match group to sugar for one arm per case |
| [`stories/generics.md`](stories/generics.md) | [`spec/generics.md`](spec/generics.md) — the parameter model, the `<>`/`()` split, size-in-the-type, and the deferred features |
| [`stories/dependencies.md`](stories/dependencies.md) | [`spec/dependencies.md`](spec/dependencies.md) — URL identity, the manifest/resolution split, prebuilt distribution, symbol-rewriting, the browsable global cache, the package-graph acyclicity rule, and the opt-in remapping model |
| [`stories/memory.md`](stories/memory.md) | [`spec/memory.md`](spec/memory.md) — the no-GC-no-lifetimes goal, the move problem and the anchor, lazy backpointer creation, the indexed heap table, the rooted-ref rules, and the collapse to one value/reference axis with a borrowed receiver |
| [`stories/lifetimes.md`](stories/lifetimes.md) | [`spec/lifetimes.md`](spec/lifetimes.md) — lexical scope in place of a borrow checker, what may be moved, the declaration-block rule that kills flow analysis, downgrade instead of use-after-move, and parameter-rooted returned refs |
| [`stories/effects.md`](stories/effects.md) | [`spec/effects.md`](spec/effects.md) — inferring effects instead of annotating them, receiver-scoped `mut`, capabilities in place of ambient I/O, the four-level ladder and the Total-Pure/Pure split, what deliberately is not an effect, and mutation through a borrowed receiver |
| [`stories/concurrency.md`](stories/concurrency.md) | [`spec/concurrency.md`](spec/concurrency.md) — the parallelism/concurrency split and the refusal of `async` coloring, why `spawn` marks only a call, water-tower lifetimes, signature-based safety without locks, and value-typed mutation closing the aliased-write gap |

## Contributing

Style and structural conventions live in two sibling guides; read the relevant one before editing or adding a document:

- [`contributing/writing-spec-docs.md`](contributing/writing-spec-docs.md) — normative spec documents in [`spec/`](spec/).
- [`contributing/writing-stories-docs.md`](contributing/writing-stories-docs.md) — stories docs in [`stories/`](stories/).
