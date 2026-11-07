<!-- markdownlint-disable-next-line MD041 -- the logo floats before the title -->
<img height="96" align="right" src="https://raw.githubusercontent.com/zane-lang/logos/main/zane/zane.svg" alt="Zane logo" />

# The Zane Language Specification

This repository defines the [Zane](https://github.com/zane-lang/zane) programming language: what every construct means, what programs are legal, and how memory, effects and packages work. Next to the specification sit the **design stories**, which tell how and why each part came to be.

It is written as a reference, not a tutorial. To try the language first, install [`zane`](https://github.com/zane-lang/zane) and look at its first-project steps.

This repository is also a **prior art publication**. The techniques described in these documents were conceived and developed by the author and are dedicated to the public domain under [CC0 1.0 Universal](LICENSE). Anyone may use these ideas freely, for any purpose, without restriction or attribution. No patent claims are made by the author.

## Where to start

Read [`spec/foundations.md`](spec/foundations.md) first: it states the few commitments the rest of the language rests on. Then, roughly in this order:

1. [`spec/syntax.md`](spec/syntax.md), for what each construct looks like;
2. [`spec/types.md`](spec/types.md) and [`spec/functions.md`](spec/functions.md), for types, constructors and methods;
3. [`spec/memory.md`](spec/memory.md) and [`spec/lifetimes.md`](spec/lifetimes.md), for ownership and references, where Zane differs most from other languages;
4. [`spec/effects.md`](spec/effects.md), for what a call is allowed to change;
5. [`spec/packages.md`](spec/packages.md), for how a project is laid out.

Look up unfamiliar terms in [`spec/glossary.md`](spec/glossary.md). When a rule seems strange, its story usually explains it: spec sections with a story behind them link to the chapter.

The spec describes the language; the [compiler](https://github.com/zane-lang/compiler) implements it, and lists where it differs in its [`docs/spec-divergences.md`](https://github.com/zane-lang/compiler/blob/main/docs/spec-divergences.md), checked against the spec commit named there.

## Repository layout

```text
spec/             the specification itself: what the language does
stories/          design stories: how each part of the spec came to be
contributing/     style guides for writing spec and stories documents
memorybench/      a C harness that models the memory design
langbench/        compiled Zane programs timed against the same programs in other languages
site/             the page that publishes both benchmarks
```

The benchmarks' pinned results are published at <https://zane-lang.github.io/spec/>. [`memorybench/`](memorybench/) measures a C model of the memory design ([`spec/memory.md`](spec/memory.md)); [`langbench/`](langbench/README.md) measures what a released compiler builds.

## Specification documents

Each rule has one canonical home; every other mention cross-references it.

### Foundations

| Document | Purpose |
|---|---|
| [`spec/foundations.md`](spec/foundations.md) | The few cross-cutting commitments the whole spec rests on: captured intent, staged compilation, fixed layout, opt-in identity, and strictness-as-performance — start here |

### Reference documents

| Document | Purpose |
|---|---|
| [`spec/syntax.md`](spec/syntax.md) | Canonical surface syntax for every construct |
| [`spec/lexical.md`](spec/lexical.md) | Case-sensitive parsing, identifier formation, the casing-determines-kind rule, and numeric and string literal forms |
| [`spec/glossary.md`](spec/glossary.md) | Canonical names for recurring concepts and coined spec terms |

### Type system and runtime model

| Document | Purpose |
|---|---|
| [`spec/types.md`](spec/types.md) | Fundamental types, value/reference types, fields, constructors, implicit conversions, and `type`/`alias` declarations |
| [`spec/adt.md`](spec/adt.md) | Enums, variants, the struct/variant symmetry, pattern matching, `match`, and enum maps |
| [`spec/functions.md`](spec/functions.md) | Methods, free functions, subscripts, overload resolution, function values, and lambdas |
| [`spec/generics.md`](spec/generics.md) | Unified type parameters, `<>` type expressions, constructor calls, and the `Array`, `ArrayRef`, `List`, and `ListRef` container primitives |
| [`spec/memory.md`](spec/memory.md) | Ownership, settled and roaming owners, references, and arena layout |
| [`spec/lifetimes.md`](spec/lifetimes.md) | Lexical lifetime rules, moves, scope rules, and deterministic destruction |
| [`spec/effects.md`](spec/effects.md) | The effect model, `mut`, read-only bindings, what each kind of verb may write, and capability wiring |
| [`spec/concurrency.md`](spec/concurrency.md) | Implicit parallelism, `spawn`, water-tower lifetimes, and concurrency safety |

### Programs and program structure

| Document | Purpose |
|---|---|
| [`spec/control-flow.md`](spec/control-flow.md) | Block arguments, the three control-flow intrinsics, the `core` branching and repetition built on them, and 1-based ordinal rules |
| [`spec/operators.md`](spec/operators.md) | Operator set, derived operators, intersection and union, operands passed as written, and precedence |
| [`spec/math.md`](spec/math.md) | The `@math$` intrinsics — bit operations, square root, rounding, trigonometry — exact and approximate functions, and the `math` package that declares them over `core`'s `Int` and `Float` |
| [`spec/error-handling.md`](spec/error-handling.md) | Bifurcated return paths, `?` handlers, and abort/resolve semantics |
| [`spec/packages.md`](spec/packages.md) | The `lib/`, `bin/` and `test/` project layout, library packages and their subpackages, file-scoped imports, explicit member access, package visibility, package-scope state, and the program and test packages that hold `main` |
| [`spec/dependencies.md`](spec/dependencies.md) | Package identity, manifests, test dependencies, version pinning, fetching, and caching |
| [`spec/ffi.md`](spec/ffi.md) | Calling C: a package's C code and the toolchain that builds it, foreign verbs and the types that cross into C, foreign handles, foreign capabilities and the foreign variables that give them, and what foreign code is taken to read and write |

## Design stories

The spec states *what* the language does. The stories in [`stories/`](stories/), one per spec document, tell *why*: the situation that forced each choice, the roads not taken, and the costs accepted, in the order the thinking moved. They are appended to as the design changes, so each is the full record of how its part of the language came to be.

| Story | Behind |
|---|---|
| [`stories/foundations.md`](stories/foundations.md) | [`spec/foundations.md`](spec/foundations.md): the bets behind captured intent, staged compilation and strictness as performance |
| [`stories/syntax.md`](stories/syntax.md) | [`spec/syntax.md`](spec/syntax.md): why a declaration writes its name before its type, and how one shape came to cover constructors, constants and function values |
| [`stories/lexical.md`](stories/lexical.md) | [`spec/lexical.md`](spec/lexical.md): casing as a hard signal for what a name is, and letting the bracket choose the separator |
| [`stories/types.md`](stories/types.md) | [`spec/types.md`](spec/types.md): the value/reference model, why `#` marks a distinct type, and how constructors and literals reach storage |
| [`stories/adt.md`](stories/adt.md) | [`spec/adt.md`](spec/adt.md): splitting `enum` from `variant`, the central `match` block, and how a variant can contain itself |
| [`stories/generics.md`](stories/generics.md) | [`spec/generics.md`](spec/generics.md): one parameter model for types and sizes, the `<>`/`()` split, and why a second container primitive was needed |
| [`stories/dependencies.md`](stories/dependencies.md) | [`spec/dependencies.md`](spec/dependencies.md): packages identified by URL, the manifest/lock split, prebuilt releases, and `core` as an ordinary package |
| [`stories/memory.md`](stories/memory.md) | [`spec/memory.md`](spec/memory.md): memory safety with no garbage collector and no lifetime annotations: owners, references and arenas |
| [`stories/lifetimes.md`](stories/lifetimes.md) | [`spec/lifetimes.md`](spec/lifetimes.md): lexical scope in place of a borrow checker, and the single owner comparison made at every store |
| [`stories/effects.md`](stories/effects.md) | [`spec/effects.md`](spec/effects.md): inferring effects instead of annotating them, and capabilities in place of ambient I/O |
| [`stories/concurrency.md`](stories/concurrency.md) | [`spec/concurrency.md`](spec/concurrency.md): implicit parallelism without `async` coloring, and why `spawn` marks only a call |
| [`stories/error-handling.md`](stories/error-handling.md) | [`spec/error-handling.md`](spec/error-handling.md): failure as a second return path rather than a `Result` value |
| [`stories/control-flow.md`](stories/control-flow.md) | [`spec/control-flow.md`](spec/control-flow.md): how branching and repetition left the language and became ordinary `core` code |
| [`stories/functions.md`](stories/functions.md) | [`spec/functions.md`](spec/functions.md): methods pulled out of the type body, mutation made visible with `:` and `!`, and overloading by shape |
| [`stories/operators.md`](stories/operators.md) | [`spec/operators.md`](spec/operators.md): a fixed set of overloadable operators, laws enforced through derived operators, and no implicit coercion |
| [`stories/math.md`](stories/math.md) | [`spec/math.md`](spec/math.md): math as compiler intrinsics with a `math` package over them |
| [`stories/packages.md`](stories/packages.md) | [`spec/packages.md`](spec/packages.md): the directory as the package, explicit `$` access, and the `lib/`, `bin/` and `test/` layout |
| [`stories/ffi.md`](stories/ffi.md) | [`spec/ffi.md`](spec/ffi.md): calling C, whose calls can fold at compile time, and which never writes Zane memory |

## Contributing

Read the guide for the kind of document you are editing first:

- [`contributing/writing-spec-docs.md`](contributing/writing-spec-docs.md): normative spec documents in [`spec/`](spec/).
- [`contributing/writing-stories-docs.md`](contributing/writing-stories-docs.md): stories in [`stories/`](stories/).
- [`contributing/naming-terms.md`](contributing/naming-terms.md): how the coined terms in [`glossary.md`](spec/glossary.md) are chosen.
- [`contributing/markdown-formatting.md`](contributing/markdown-formatting.md): the one Markdown rule CI checks, and how to run the check.
