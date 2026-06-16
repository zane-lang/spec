---
name: zane-spec
description: |
  Work on the zane-lang/spec repository — the canonical markdown specification
  for the Zane programming language. Use this skill when the user asks to
  "add a section to the Zane spec", "rewrite a topic doc", "validate a PR
  against the existing spec", "check terminology consistency across the
  spec", or refers to a specific .md file under `spec/` by name
  (e.g. "generics.md", "types.md", "syntax.md"). Do NOT use for
  implementing a Zane compiler, parser, or runtime (that is a separate
  codebase); do NOT use for editing the `bench/` C harness (it is not
  Zane source).
---

# Zane Spec

## Inputs to collect
- Which spec document the change targets (14 markdown files under `spec/`).
- Whether the change is a new design rule, an example, a cross-reference, or a terminology swap.
- Whether the change interacts with the unified type-parameter system (see Procedure step 3). Most spec work does.

## Procedure
1. **Read `README.md` first.** It is the table of contents. Each row is one spec document, one topic.
   Why: the spec docs are not alphabetically organized; README is the only map of "what is where."

2. **Read the target document in full before editing.** Every topic doc follows the structure in `contributing/writing-spec-docs.md` (overview, numbered sections, design rationale, summary). Skim the rationale and summary tables last — they are condensed views of the section bodies, and the section bodies are what the rationale is reasoning about.
   Why: edits to one section often break the rationale row that summarizes it; reading both keeps them aligned.

3. **Use the unified type-parameter system correctly.** A type is a templated function: it declares parameters in a `<>` header and is executed to produce a layout. Functions, methods, and constructors use the same `<>` header. Each header entry is concept-typed, distinguished by the concept and by casing:
   - **Type parameter** — `name Type` with an uppercase name (e.g. `T Type`). Ranges over types; referenced bare (`T`).
   - **Number parameter** — `name Number` with a lowercase name (e.g. `n Number`). Ranges over compile-time numbers; referenced bare (`n`) and resolves to a number value in body positions.

   `<>` is the type-expression (application) syntax — `Vector<Int>`, `Array<T, n>` — and is correct in any type position. `()` is the call syntax and **never** carries a `<>` list; a type reaches a constructor by inference from a `<>` header parameter (`Vector(Int(2))`) or as a `Type`/`Number` value parameter passed explicitly (`Vector(Int)`, `Array(Int, 10000)`). Casing is load-bearing (`spec/lexical.md`): uppercase = type, lowercase = value/number. The `'` sigil, the old `[name]` binders, and `Array3`-style root forms no longer exist.

4. **Cross-reference, don't duplicate.** If a rule belongs in `spec/generics.md` (the unified parameter system) or `spec/lexical.md` (casing rules), reference it from `spec/types.md`, `spec/functions.md`, `spec/syntax.md`, or `spec/effects.md` rather than restating it. The contributing guide §1 is strict on this.

5. **Validate your change before committing.** Run a grep for the forbidden *old* forms — the pre-redesign generics syntax that should no longer appear:
   ```
   grep -nE "Array\[|\[size\]|Array[0-9]+|Matrix10|\[rows\]|\[cols\]|'[A-Z]|inferred type generic|type-parameter symbol|root form" spec/*.md
   ```
   These were removed by the type-system redesign. The new syntax (`Vector<Int>`, `Array<T, n>`, `Type`/`Number` constructor parameters) is correct and expected. The only stray `<...>` matches that are *not* Zane are the result-type comparator references in `spec/error-handling.md` (`Result<T, E>` is Rust's type, not Zane's).

6. **If the change touches the type system** (anything in `spec/generics.md` or `spec/lexical.md`, or the `<>` type-expression / `type`/`number` parameter rules referenced elsewhere), spawn a parallel-track validation team via `mavis-team` before opening a PR. Two tracks:
   - Track A: validate the files the branch modified against their own rules.
   - Track B: validate the un-updated spec files + `bench/zane_bench.c` against the new design.
   - Synthesis: cross-reference the two and produce a single fix list.

   Why this matters: a recent PR introduced a §3.4 vs §7.1 internal contradiction that only an independent re-reader caught. Single-pass self-review is not enough on this codebase.

7. **Commit messages** follow the existing style: short lower-case prefix (`docs:`, `docs(meta):`), then one or two sentences describing the spec change. See `git log --oneline` for the established cadence.

## Output contract
- One or more spec files modified, committed on a branch, pushed to remote.
- Branch name follows the topic (`inferred-type-parameters`, `phantom-types`, etc.).
- If the change is structural, a follow-up commit with the validation team's FIX list applied before opening the PR.

## Failure handling
- **Grep finds an old form (`Array[size]`, `[name]` binders, `Array3` root forms, `'T` apostrophe generics, "type generic", "type-parameter symbol").** Stop. The change references the pre-redesign generics design. Rewrite it in the unified `<>` system (`Array<T, n>`, `Type`/`Number` parameters).
- **Cross-reference target no longer exists.** `spec/generics.md` has been renumbered by the redesign; if your `§3` is now `§4`, fix the reference in every doc that uses it, then re-grep for the old `§` numbers.
- **The change conflicts with a section in another file.** Don't paper over it with a footnote. Either fix the conflicting section or escalate the conflict as a design call to the user.
- **`bench/zane_bench.c` shows up in the diff for a spec change.** It is C, not Zane. Comments and `printf` labels in it that use old surface syntax (`Array[size]`, `List<T>`) are documentation, not parsing errors. Fix only the comment/label, never the C code.

## Examples

**Input:** "Add a section to `spec/error-handling.md` about a new `Recover` abort type."

**Output:** A new section that follows the §3 / §4 / §5 numerical pattern of nearby sections, with its own design-rationale row in §8 and a row in the §9 summary. Cross-references `spec/generics.md` if the new abort type interacts with the type-parameter system. PR description summarizes the cross-references added.

**Input:** "Validate this PR — does it keep the spec internally consistent?"

**Output:** Spawn a `mavis-team` plan with two parallel tracks (modified files vs. rest-of-spec) plus a synthesis gate. The synthesis writes `.mavis/plans/validation-report.md` with PASS/FIX/SPEC-GAP per file, a coverage matrix, and a concrete fix list. Reject and re-edit if either track reports a contradiction, a missing cross-reference, or a forbidden form.

## Repo map

The spec lives in `spec/`. Each file is one topic.

| File | Owns |
|---|---|
| `spec/syntax.md` | Canonical surface syntax reference |
| `spec/lexical.md` | Case sensitivity, identifier formation, casing-determines-kind |
| `spec/glossary.md` | Canonical names for recurring concepts |
| `spec/types.md` | Classes, structs, fields, constructors, implicit conversions, `type`/`alias` |
| `spec/functions.md` | Methods, free functions, subscripts, overload resolution, lambdas |
| `spec/generics.md` | **Unified type parameters, `<>` type expressions, constructor calls (canonical home)** |
| `spec/memory.md` | Ownership, refs, anchors, layout |
| `spec/lifetimes.md` | Scope rules, ownership moves, lifetime, deterministic destruction |
| `spec/effects.md` | Effect model, `mut`, inferred effect levels |
| `spec/concurrency.md` | Implicit parallelism, `spawn`, water-tower lifetimes |
| `spec/control-flow.md` | Conditional branching, `guard`, counted loops, 1-based ordinals |
| `spec/operators.md` | Operator set, derived operators, precedence |
| `spec/error-handling.md` | Bifurcated return paths, `?` handlers, abort/resolve |
| `spec/packages.md` | Package declarations, `$` access, instanceful package pattern |
| `spec/dependencies.md` | Package identity, manifests, fetch rules |
| `README.md` | Document index |

Plus `contributing/writing-spec-docs.md` (style guide) and `bench/` (C harness — not Zane source).
