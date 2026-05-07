# Zane Terminology

This document records the canonical names used across the Zane specification for recurring language concepts and design rules. It does not add semantics; each term points back to the document that defines the rule in full.

> **See also:** [`README.md`](README.md) for the document index. [`syntax.md`](syntax.md) for canonical surface forms. Topic documents for the full semantic rules behind each term.

---

## 1. Overview

This file gives short, reusable names to concepts that appear across multiple spec documents.

- **`Canonical names`.** Each entry defines the preferred label for a recurring concept.
- **`No new semantics`.** The terminology file summarizes rules; the linked home document remains authoritative.
- **`Cross-document reuse`.** A term listed here should be reused consistently when the same concept appears elsewhere in the spec.
- **`Home document links`.** Every entry names the document section where the full rule is specified.

---

## 2. Terminology Catalog

| Term | Meaning | Canonical home |
|---|---|---|
| **Bifurcated Return Path** | The error model in which an abortable call has a statically typed primary path and a statically typed abort path. | [`error_handling.md`](error_handling.md) §1 |
| **binder/reference split** | The contextual rule that `[...]` binds a const parameter in definition positions but refers to an already bound const in type bodies and method `this` types. | [`type_parameters.md`](type_parameters.md) §3 and §4 |
| **capability wiring** | The explicit passing or storing of capability objects so external-state access stays visible in object structure and call structure. | [`purity.md`](purity.md) §6 |
| **compiler concept types** | Compiler-provided literal-facing types such as `@concepts$Number` that may appear in parameter positions but not in storage. | [`syntax.md`](syntax.md) §2.8 |
| **field constructor** | Constructor syntax that declares fields directly in the parameter header and maps them straight into `init{}`. | [`oop.md`](oop.md) §3.3 |
| **home-package operator rule** | The restriction that an operator implementation may be declared only in the home package of one of its operand types. | [`operators.md`](operators.md) §2.2 |
| **method-based privacy** | The rule that `_` fields are private to methods whose first parameter is `this` for that type, rather than to a package boundary. | [`oop.md`](oop.md) §2.3 |
| **placeholder-prefix rewriting** | The fetch-time rewrite that replaces a library's `!`-prefixed export symbols with the resolved version tag before caching and linking. | [`dependency_management.md`](dependency_management.md) §6.1 |
| **place expression** | An expression that denotes an existing, stable storage location and is therefore legal as a source for `ref` binding. | [`memory_model.md`](memory_model.md) §2.8 |
| **resolve-only shorthand** | The `??` form, which desugars to a `?` handler that only supplies a fallback `resolve`. | [`error_handling.md`](error_handling.md) §3.3 |
| **scope-exit guard** | A `guard` form that conditionally exits the current lexical scope instead of introducing another nested branch. | [`control_flow.md`](control_flow.md) §3 |
| **single-writer rule** | The concurrency rule that at most one concurrent `mut` accessor may exist for the same object. | [`concurrency_model.md`](concurrency_model.md) §4.2 |
| **struct-downstream enforcement** | The recursive rule that a struct may contain only primitives and other legal structs, never a class or `ref` field anywhere downstream. | [`memory_model.md`](memory_model.md) §2.10 |
| **structural effect model** | The effect system in which `mut`, ownership, call structure, and capabilities determine the inferred effect level of a function. | [`purity.md`](purity.md) §1 and §5 |
| **URL identity** | The rule that a package's canonical identity is its full source URL, while local aliases are only import conveniences. | [`dependency_management.md`](dependency_management.md) §1 and §2 |
| **water-tower lifetimes** | The lifetime rule that scope-owned objects remain alive until every `spawn`ed call in that scope has finished and the scope drains. | [`concurrency_model.md`](concurrency_model.md) §4.1 |
| **1-based ordinal counting** | The convention that counted loops and positional indexing start at `1`, so an ordered sequence's final valid position is its size. | [`control_flow.md`](control_flow.md) §5 |

---

## 3. Design Rationale

| Decision | Rationale |
|---|---|
| Canonical terminology index | Centralizing the names keeps related documents aligned without duplicating the full rules in each file. |
| Home-document links in every entry | Readers need one-hop access from the short term to the authoritative rule text. |
| Mix of existing and newly coined labels | Some concepts already had stable names, while others needed a short reusable label so the spec can refer to them consistently. |
| Terminology file adds no semantics | Keeping semantics in the topic documents avoids conflicts between summary wording and the canonical rule text. |
