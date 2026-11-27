# The Zane Language Specification

What every construct of the Zane language means, what programs are legal, and how memory, effects and packages work.

The specification is a reference, not a tutorial: to learn the language, start with the [Learn pages](https://github.com/zane-lang/docs/blob/main/docs/README.md). The [design stories](../stories/README.md) record the history of each design decision.

Each rule is specified in exactly one document, the rule's canonical home; every other document that mentions the rule links to that home.

## Foundations

| Document | Purpose |
|---|---|
| [`foundations.md`](foundations.md) | The few cross-cutting commitments the whole spec rests on: captured intent, staged compilation, fixed layout, opt-in identity, and strictness-as-performance — start here |

## Reference documents

| Document | Purpose |
|---|---|
| [`syntax.md`](syntax.md) | Canonical surface syntax for every construct |
| [`lexical.md`](lexical.md) | Case-sensitive parsing, identifier formation, the casing-determines-kind rule, and numeric and string literal forms |
| [`glossary.md`](glossary.md) | Canonical names for recurring concepts and coined spec terms |

## Type system and runtime model

| Document | Purpose |
|---|---|
| [`types.md`](types.md) | Fundamental types, value/reference types, fields, constructors, implicit conversions, `type`/`alias` declarations, and the string primitive's numbers, bytes, and byte sets |
| [`adt.md`](adt.md) | Enums, variants, the struct/variant symmetry, pattern matching, `match`, and enum maps |
| [`functions.md`](functions.md) | Methods, free functions, subscripts, overload resolution, function values, and lambdas |
| [`generics.md`](generics.md) | Unified type parameters, `<>` type expressions, constructor calls, and the `Array`, `ArrayRef`, `List`, and `ListRef` container primitives and their operations |
| [`memory.md`](memory.md) | Ownership, settled and roaming owners, references, and arena layout |
| [`lifetimes.md`](lifetimes.md) | Lexical lifetime rules, moves, scope rules, and deterministic destruction |
| [`effects.md`](effects.md) | The effect model, `mut`, read-only bindings, what each kind of verb may write, capability wiring, and compile-time evaluation, implicit and marked with `comptime` |
| [`concurrency.md`](concurrency.md) | Implicit parallelism, `spawn`, worker pools, water-tower lifetimes, concurrency safety, and a host's calls into exported lambdas |

## Programs and program structure

| Document | Purpose |
|---|---|
| [`control-flow.md`](control-flow.md) | Block arguments, the control-flow intrinsics, the `core` branching, repetition, and workers built on them, and 1-based ordinal rules |
| [`operators.md`](operators.md) | Operator set, derived operators, intersection and union, operands passed as written, and precedence |
| [`math.md`](math.md) | The `@math$` intrinsics — bit operations, square root, rounding, trigonometry — exact and approximate functions, and the `math` package that declares them over `core`'s `Int` and `Float` |
| [`error-handling.md`](error-handling.md) | Bifurcated return paths, `?` handlers, and abort/resolve semantics |
| [`packages.md`](packages.md) | The `lib/`, `bin/` and `test/` project layout, library packages and their subpackages, file-scoped imports, explicit member access, package visibility, package-scope state, the program and test packages that hold `main`, and the plugin packages and exported lambdas a host loads |
| [`dependencies.md`](dependencies.md) | Package identity, manifests, test dependencies, package renames, version pinning, fetching, and caching |
| [`ffi.md`](ffi.md) | Calling C: a package's C code and the toolchain that builds it, foreign verbs and the types that cross into C, the context a callback gets back, foreign handles, foreign capabilities and the foreign variables that give them, and what foreign code is taken to read and write |

## Reading order

Read [`foundations.md`](foundations.md) first: it states the few commitments the rest of the language rests on. Then, roughly in this order:

1. [`syntax.md`](syntax.md), for what each construct looks like;
2. [`types.md`](types.md) and [`functions.md`](functions.md), for types, constructors and methods;
3. [`memory.md`](memory.md) and [`lifetimes.md`](lifetimes.md), for ownership and references, where Zane differs most from other languages;
4. [`effects.md`](effects.md), for what a call is allowed to change;
5. [`packages.md`](packages.md), for how a project is laid out.

Look up unfamiliar terms in [`glossary.md`](glossary.md). When a rule seems strange, its design story usually explains the reason for the rule: each spec section with a story behind it links to the story's chapter.

The spec describes the language; the [compiler](https://github.com/zane-lang/compiler) implements the language, and lists where the compiler differs from the spec in its [`docs/spec-divergences.md`](https://github.com/zane-lang/compiler/blob/main/docs/spec-divergences.md), checked against the spec commit named there.
