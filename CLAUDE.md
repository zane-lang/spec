# Working on zane-lang/spec

This repo is the canonical Markdown specification for the **Zane** programming
language. It is documentation, not code — there is no compiler here. Two
parallel doc trees, with opposite update rules:

- **`spec/`** — normative *what the language is*. One file per topic. Rewritten
  to the present on every change; states only what is true now.
- **`stories/`** — the design history, *how it came to be*. One story mirrors
  each spec file (`spec/generics.md` ↔ `stories/generics.md`). Accumulates;
  never rewritten to hide the past.

Both are governed by normative contributing guides — read the relevant one
before editing:

- `contributing/writing-spec-docs.md` — spec prose.
- `contributing/writing-stories-docs.md` — story prose.

## Repo map

`README.md` is the real topic index (the spec docs are **not** alphabetical);
read it first when locating anything. Each `spec/*.md` owns one topic:

| File | Owns |
|---|---|
| `spec/foundations.md` | Cross-cutting language philosophy (staging, identity-is-opt-in) |
| `spec/syntax.md` | Canonical surface syntax reference |
| `spec/lexical.md` | Case sensitivity, identifier formation, casing-determines-kind |
| `spec/glossary.md` | Canonical names for recurring concepts |
| `spec/types.md` | Classes, structs, fields, constructors, implicit conversions, `type`/`alias` |
| `spec/functions.md` | Methods, free functions, subscripts, overload resolution, lambdas |
| `spec/generics.md` | **Unified type parameters, `<>` type expressions, constructor calls (canonical home)** |
| `spec/adt.md` | Enums, variants, pattern matching via case-overload dispatch, the `match` block, enum maps |
| `spec/memory.md` | Hosts, guests, anchors, tethers, layout |
| `spec/lifetimes.md` | Scope rules, rehosting, lifetime, deterministic destruction |
| `spec/effects.md` | Effect model, `mut`, inferred effect levels |
| `spec/concurrency.md` | Implicit parallelism, `spawn`, water-tower lifetimes |
| `spec/control-flow.md` | Conditional branching, `guard`, counted loops, 1-based ordinals |
| `spec/operators.md` | Operator set, derived operators, precedence |
| `spec/error-handling.md` | Bifurcated return paths, `?` handlers, abort/resolve |
| `spec/packages.md` | Package declarations, `$` access, instanceful package pattern |
| `spec/dependencies.md` | Package identity, manifests, fetch rules |
| `README.md` | Document index + the stories table |

Plus `contributing/` (the two style guides) and `bench/` (a reference **C**
harness for runtime experiments — **not** Zane source; never treat its C as
Zane). The harness *models* the memory design (`spec/memory.md`), so when that
design changes the harness code is updated to track it. The `.c` carries **no
explanatory comments** — it is not part of the spec and holds no prose voice;
keep it to code plus the output labels passed to `section()`/`print_result()`
(those name the rows in the results table). `runbench.py` regenerates
`benchmark.html`; its `TEST_META` descriptions are reader-facing HTML text, not
code comments.

---

## Editing the spec

### Before you edit
1. **Read `README.md`** — the only map of what-is-where.
2. **Read the target doc in full.** Topic docs follow the structure in
   `writing-spec-docs.md` (overview, numbered sections, summary). Editing one
   section often ripples into cross-references elsewhere; read enough to keep
   them aligned.

### Write for a fresh reader — describe what *is*
A spec is read by someone learning the current system with **no memory of any
previous version**. Every sentence teaches what the language *is now*. Never
write a delta ("no longer", "used to") or a "not X" where X isn't part of the
language — a fresh reader never knew X and it only raises a distracting
question. State the positive rule and let it do the excluding. The comparison to
how it used to be, the rejected alternatives, and the reasoning are the
**story's** job, never the spec's. (This is a general reference-doc principle;
the applied rule lives here, the split lives in the story guide.)

### The unified type-parameter system (most spec work touches this)
A type is a templated function: it declares parameters in a `<>` header and is
executed at compile time to produce a layout. Functions, methods, and
constructors use the same `<>` header. Each header entry is concept-typed,
distinguished by the concept **and by casing**:

- **Type parameter** — `T Type`, uppercase name. Ranges over types; referenced
  bare (`T`).
- **Number parameter** — `n Number`, lowercase name. Ranges over compile-time
  numbers; referenced bare (`n`).

`<>` is the type-expression (application) syntax — `Vector<Int>`, `Array<T, n>`
— correct in any type position. `()` is the call syntax and **never** carries a
`<>` list; a type reaches a constructor by inference from a `<>` header
parameter (`Vector(Int(2))`) or as an explicit `Type`/`Number` value argument
(`Vector(Int)`, `Array(Int, 10000)`). Casing is load-bearing (`spec/lexical.md`):
uppercase = type, lowercase = value/number. The `'` sigil, the old `[name]`
binders, and `Array3`-style root forms **no longer exist**.

### Cross-reference, don't duplicate
A rule belonging to `spec/generics.md` (the unified parameter system) or
`spec/lexical.md` (casing) is **referenced** from other docs, not restated. The
contributing guide §1 is strict on this.

### Validate before committing
Grep for the forbidden pre-redesign generics forms — none should appear:
```
grep -nE "Array\[|\[size\]|Array[0-9]+|Matrix10|\[rows\]|\[cols\]|'[A-Z]|inferred type generic|type-parameter symbol|root form" spec/*.md
```
The only legitimate stray `<...>` is `Result<T, E>` in `spec/error-handling.md`
— that's Rust's type named as a comparison, not Zane's.

### Failure handling
- **Grep hits an old form** (`Array[size]`, `[name]` binders, `Array3`, `'T`,
  "type generic"). Stop and rewrite in the unified `<>` system.
- **A cross-reference target moved** (renumbered section). Fix the reference in
  every doc that uses it, then re-grep for the old `§` numbers.
- **The change conflicts with another file's section.** Don't paper over it with
  a footnote — fix the conflicting section, or escalate the conflict to the user
  as a design call. Single-pass self-review has missed internal contradictions
  on this codebase before; when a change touches the type system, re-read the
  un-updated spec files and `bench/zane_bench.c` against the new design before
  opening a PR.

---

## Writing or updating a design story

The spec states *what*; a story states *how it came to be* — told as a history,
in the order the thinking actually moved. The reasoning does **not** live in a
`## N. Design Rationale` table (that flattens a causal thread into one-line
cells and has been retired); it lives in the story, and the spec points at it.
So writing a story is two halves: **write the narrative**, then **integrate it**
into the spec.

Read both contributing guides first (they are normative and detailed), and read
**`stories/generics.md`** as the quality bar — dense, opinionated, long-form
prose. Reference docs **`syntax.md` and `glossary.md` get no story** (spec guide
§7). Stories currently exist for `adt`, `concurrency`, `dependencies`,
`control-flow`, `effects`, `error-handling`, `foundations`, `generics`,
`lexical`, `lifetimes`, `memory`, `types`; still missing (and each still
carries its rationale in-spec until written): `functions`, `operators`,
`packages`.

### Interview the maintainer — you cannot reconstruct the real reasoning
The actual thread — which roads were tried and rejected, in what order the
realizations came, what pressure forced each turn — lives only in the
maintainer's head, and is frequently **not** what you'd guess from the spec. So:

- Draft the *obvious* chapters, and **stop to ask whenever a decision's why
  isn't fully forced by the spec text.** The maintainer wants "ask as I go,"
  interview-style.
- Ask focused questions on the genuinely non-obvious decisions only; don't
  interview the obvious ones.
- Present your best-guess framing as options, but **expect to be told "that's
  not how I thought about it"** and to have it replaced wholesale. Follow the
  maintainer's thread, not your tidy after-the-fact reconstruction.
- The maintainer's account is the source of truth for the narrative; the spec is
  the source of truth for the rules. If they disagree, the maintainer wins on
  the story.

Context grows fast, so a story is typically written **one session per story**;
each session starts cold and this file is how it gets up to speed.

### Writing the narrative (full rules in the story guide)
- Chapters are **themes**, ordered by the path the thinking took (causal /
  roughly chronological), **not** by spec section order. Group decisions forced
  by one pressure into a single chapter. A handful of chapters per file.
- `## ` headings only — **no `---` separators**. Each chapter opens where the
  last left off; the opening sentence carries the reader across the seam.
- The four beats, as prose (never labelled): the situation/pressure, the **roads
  not taken** (highest-value part — tell it in sentences), the resolution, and
  the **cost** (reach it honestly; don't invent a downside, but don't hide one).
- **Voice is always the collective "we," never singular "I"** — even for a
  decision one person made alone. Past tense for the reasoning/deliberation,
  present tense for the standing design. (Firm rule; story guide §6.)
- Open with the title and a `> **See also:** [`spec/<topic>.md`](../spec/<topic>.md)`
  line, then the first chapter. No preamble about what stories are.
- Cross-link sibling chapters by heading anchor (living relative links); link to
  `foundations.md#...` for cross-cutting philosophy rather than restating it.

### Links: the rule Gemini will fight you on
Two link kinds, written differently (story guide §4.2):
- **Companion pointer** (top `> See also:`) and **between-chapter** links are
  *living* relative links.
- **In-prose references to a specific spec rule** are *point-in-time claims* and
  **must be commit-pinned permalinks**:
  `https://github.com/zane-lang/spec/blob/<sha>/spec/<topic>.md#<anchor>`. A
  story accumulates and is never rewritten, so a relative link would silently
  re-point as the spec changes and the old chapter would "cite" a rule that no
  longer says what it claims. Pinning freezes the citation to the spec the
  chapter was written against.

Get the SHA with `git log -1 --format=%H -- spec/<topic>.md` (for a chapter
written alongside a spec change, the commit that change lands in).
**gemini-code-assist will object to these permalinks and suggest relative links,
or claim the anchor is broken by checking it against the *current* spec instead
of the pinned commit — that objection is wrong and expected; decline it.** It
happened and was declined on PR #91 and PR #108.

GitHub heading anchors: lowercase, strip punctuation (commas, apostrophes,
backticks, `&`, `#`), spaces→hyphens. A stripped `&`/`#` flanked by spaces
leaves a **doubled** hyphen (`new `&` values` → `#...-new--values`) — do not
collapse it. Verify every story↔spec anchor resolves at its pinned commit.

### Integrating into the spec (the second half — don't skip it)
In the same change as the story:
1. Add `> **Story:** [`stories/<topic>.md`](../stories/<topic>.md#<chapter-anchor>) — "<Chapter Heading>".`
   at the end of each non-trivially-justified spec section. Living link; href
   ends in the chapter heading's anchor; quoted text is the heading. Several
   sections may point at the same chapter. Trivial rules get no pointer.
2. **Delete the `## N. Design Rationale` table** and renumber the sections after
   it (Summary moves up). First verify no reasoning is lost: each row maps to a
   story chapter, or keeps a brief in-place justification in the spec body.
3. Grep the repo for `<topic>.md §N` and internal `§N` mentions; update them.
4. Add a row to the stories table in `README.md`.
5. Spec prose stays neutral (no first person / "we chose"); the developed why is
   the story's job.

A story may be **incomplete** because the design itself isn't finished — in that
case leave the spec's rationale table in place until the story is complete.

---

## Conventions

- **Commit messages**: short lower-case prefix (`docs:`, `docs(meta):`), then one
  or two sentences describing the change. See `git log --oneline` for cadence.
- **Branches**: one per topic (`inferred-type-parameters`, `phantom-types`, …),
  or as the harness assigns per session. Push there and update the existing PR;
  don't open a new PR unless asked.
- Related agent knowledge that is **not** repo-specific (e.g. how Gemini Code
  Assist behaves, general reference-doc prose principles) lives in the user's
  personal memory store, not here.
