---
name: zane-spec
description: |
  Work on the zane-lang/spec repository — the canonical markdown specification
  for the Zane programming language. Use this skill when the user asks to
  "add a section to the Zane spec", "rewrite a topic doc", "validate a PR
  against the existing spec", "check terminology consistency across the
  spec", or refers to a specific .md file under the repo root by name
  (e.g. "type_parameters.md", "oop.md", "syntax.md"). Do NOT use for
  implementing a Zane compiler, parser, or runtime (that is a separate
  codebase); do NOT use for editing the `bench/` C harness (it is not
  Zane source).
---

# Zane Spec

## Inputs to collect
- Which spec document the change targets (12 markdown files at repo root).
- Whether the change is a new design rule, an example, a cross-reference, or a terminology swap.
- Whether the change interacts with the two parameter kinds (see §"Vocabulary" below). Most spec work does.

## Procedure
1. **Read `README.md` first.** It is the table of contents. Each row is one spec document, one topic.
   Why: the 12 files are not alphabetically organized; README is the only map of "what is where."

2. **Read the target document in full before editing.** Every topic doc follows the structure in `contributing/writing-spec-docs.md` (overview, numbered sections, design rationale, summary). Skim the rationale and summary tables last — they are condensed views of the section bodies, and the section bodies are what the rationale is reasoning about.
   Why: edits to one section often break the rationale row that summarizes it; reading both keeps them aligned.

3. **Use the two parameter kinds correctly.** The spec has two:
   - **Type generics** — the `'`-prefixed name in a body type position (e.g. `value 'T`). Inferred from the body; no use-site syntax exists.
   - **Type parameters** — the `[name]` binder in a type header (e.g. `struct Buffer[n]`) and references in the body. A third kind of symbol that resolves to `Int` in body positions. Adjacent slots require a non-type-parameter delimiter.

   "Type parameter" never means the old `<'T>` slot. If you find yourself writing `<'T>` or `<Int>`, you are reverting the inferred-generic design — stop and re-read `type_parameters.md`.

4. **Cross-reference, don't duplicate.** If a rule belongs in `type_parameters.md §3.5` (phantom type generics), reference it from `oop.md` or `purity.md` rather than restating it. The contributing guide §1 is strict on this.

5. **Validate your change before committing.** Run a grep for the forbidden forms:
   ```
   grep -E "<'T|<T>|const parameter|<'A|<'B|<'C|<'D|<'E|<'K|<'V" *.md
   ```
   The only matches that should remain are deliberate negative examples in `type_parameters.md` (e.g. `// ILLEGAL: there is no <'T> form anywhere`) and the result-type comparator references in `error_handling.md` (`Result<T, E>` is Rust's type, not Zane's).

6. **If the change touches the new design** (anything in `type_parameters.md`, or the `<'T>` / `[name]` rules referenced elsewhere), spawn a parallel-track validation team via `mavis-team` before opening a PR. Two tracks:
   - Track A: validate the 6 files the branch modified against their own rules.
   - Track B: validate the 6 un-updated spec files + `bench/zane_bench.c` against the new design.
   - Synthesis: cross-reference the two and produce a single fix list.

   Why this matters: a recent PR introduced a §3.4 vs §7.1 internal contradiction that only an independent re-reader caught. Single-pass self-review is not enough on this codebase.

7. **Commit messages** follow the existing style: short lower-case prefix (`docs:`, `docs(meta):`), then one or two sentences describing the spec change. See `git log --oneline` for the established cadence.

## Output contract
- One or more spec files modified, committed on a branch, pushed to remote.
- Branch name follows the topic (`inferred-type-parameters`, `phantom-types`, etc.).
- If the change is structural, a follow-up commit with the validation team's FIX list applied before opening the PR.

## Failure handling
- **Grep finds `<T>` outside the documented exceptions.** Stop. The change reverts the inferred-generic design. Either drop the use-site syntax or update the section to be a deliberate negative example.
- **Cross-reference target no longer exists.** `type_parameters.md` has been renumbered multiple times; if your `§3` is now `§4`, fix the reference in every doc that uses it, then re-grep for the old `§` numbers.
- **The change conflicts with a section in another file.** Don't paper over it with a footnote. Either fix the conflicting section or escalate the conflict as a design call to the user.
- **`bench/zane_bench.c` shows up in the diff for a spec change.** It is C, not Zane. Comments and `printf` labels in it that use old surface syntax (`Array[size]<T>`, `List<T>`) are documentation, not parsing errors. Fix only the comment/label, never the C code.

## Examples

**Input:** "Add a section to `error_handling.md` about a new `Recover` abort type."

**Output:** A new section that follows the §3.9 / §4 / §5 numerical pattern of nearby sections, with its own design-rationale row in §8 and a row in the §9 summary. Cross-references `type_parameters.md §3.5` if the new abort type interacts with phantom generics. PR description summarizes the cross-references added.

**Input:** "Validate this PR — does it keep the spec internally consistent?"

**Output:** Spawn a `mavis-team` plan with two parallel tracks (modified files vs. rest-of-spec) plus a synthesis gate. The synthesis writes `.mavis/plans/validation-report.md` with PASS/FIX/SPEC-GAP per file, a coverage matrix, and a concrete fix list. Reject and re-edit if either track reports a contradiction, a missing cross-reference, or a forbidden form.

## Repo map (12 .md files at root)

| File | Owns |
|---|---|
| `memory_model.md` | Ownership, refs, anchors, lifetime, layout |
| `concurrency_model.md` | Implicit parallelism, `spawn`, water-tower lifetimes |
| `oop.md` | Classes, structs, constructors, methods, packages, call resolution |
| `purity.md` | Effect model, `mut`, inferred effect levels |
| `operators.md` | Operator set, derived operators, precedence |
| `type_parameters.md` | **Inferred type generics + type-parameter symbols (canonical home)** |
| `error_handling.md` | Bifurcated return paths, `?` handlers, abort/resolve |
| `dependency_management.md` | Package identity, manifests, fetch rules |
| `control_flow.md` | Conditional branching, `guard`, counted loops, 1-based ordinals |
| `terminology.md` | Canonical names for recurring concepts |
| `syntax.md` | Canonical surface syntax reference |
| `README.md` | Document index |

Plus `contributing/writing-spec-docs.md` (style guide) and `bench/` (C harness — not Zane source).
