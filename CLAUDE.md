# Working on zane-lang/spec (agent notes)

This repo is the canonical Markdown specification for the **Zane** programming
language — documentation, not code (no compiler here). Two parallel doc trees
with opposite update rules: `spec/` states *what the language is now* (rewritten
to the present on every change) and `stories/` records *how it came to be*
(accumulates; never rewritten to hide the past). One story mirrors each spec
file (`spec/generics.md` ↔ `stories/generics.md`).

This file holds only what an **agent** needs that the human docs don't already
cover. The actual rules live in the normative guides — **read the relevant one
in full before editing**, they are detailed and win over anything summarized
here:

- `contributing/writing-spec-docs.md` — spec prose.
- `contributing/writing-stories-docs.md` — story prose.
- `contributing/naming-terms.md` — coining a term of art.
- `README.md` — the real topic index (spec docs are **not** alphabetical); read
  it first when locating anything.

Each session starts cold with no memory of prior ones, so this file is how the
next agent gets up to speed — keep it to durable, agent-facing facts.

## Before you edit
1. Read `README.md` to locate the topic, then **read the target doc in full** —
   editing one section ripples into cross-references elsewhere.
2. When a change touches the **type system**, single-pass self-review has missed
   internal contradictions on this codebase before: re-read the *un-updated*
   spec files and `bench/zane_bench.c` against the new design before opening a
   PR, not just the file you changed.

## The `bench/` harness
`bench/` is a reference **C** harness for runtime experiments — **not** Zane
source; never treat its C as Zane. It *models* the memory design
(`spec/memory.md`), so when that design changes the harness is updated to track
it. The `.c` carries **no explanatory comments** (it holds no prose voice) —
keep it to code plus the labels passed to `section()`/`print_result()`.
`runbench.py` regenerates `benchmark.html`; its `TEST_META` strings are
reader-facing HTML, not code comments.

## Validate before committing (spec edits)
The generics system was unified into a `<>`-header / `()`-call model (canonical
home `spec/generics.md`, casing rules `spec/lexical.md`). Several pre-redesign
forms are now illegal and must never reappear. Grep for them — none should hit:

```
grep -nE "Array\[|\[size\]|Array[0-9]+|Matrix10|\[rows\]|\[cols\]|'[A-Z]|inferred type generic|type-parameter symbol|root form" spec/*.md
```

The only legitimate stray `<...>` is `Result<T, E>` in `spec/error-handling.md`
— Rust's type named as a comparison, not Zane's.

If the grep hits an old form, stop and rewrite it in the unified system. If a
cross-reference target moved (renumbered `§`), fix the reference in every doc
that uses it, then re-grep for the old numbers. If the change conflicts with
another file's section, fix the conflicting section or escalate it to the user
as a design call — don't paper over it with a footnote.

## Writing a design story
Story coverage is **complete**: every topic spec has a story; the two reference
docs `syntax.md` and `glossary.md` get none (spec guide §7). New stories are
written for wholly new topics only.

Read both contributing guides first, and read **`stories/generics.md`** as the
quality bar — dense, opinionated, long-form prose. Writing a story is two
halves: write the narrative, then integrate it into the spec. Don't skip the
second half.

### Interview the maintainer — you cannot reconstruct the real reasoning
The actual thread — which roads were tried and rejected, in what order the
realizations came, what pressure forced each turn — lives only in the
maintainer's head, and is frequently **not** what you'd guess from the spec. So:

- Draft the *obvious* chapters, and **stop to ask whenever a decision's why
  isn't fully forced by the spec text** — the maintainer wants "ask as I go,"
  interview-style, focused questions on the genuinely non-obvious decisions only.
- Present your best-guess framing as options, but **expect to be told "that's
  not how I thought about it"** and to have it replaced wholesale. Follow the
  maintainer's thread, not your tidy after-the-fact reconstruction.
- The maintainer's account is the source of truth for the narrative; the spec is
  the source of truth for the rules. On the story, the maintainer wins.

Because context grows fast, a story is typically written one session per story.

### Integrating into the spec (the half that's easy to forget)
In the same change as the story:
1. Add a `> **Story:**` pointer at the end of each non-trivially-justified spec
   section (living link; href ends in the chapter-heading anchor; quoted text is
   the heading). Several sections may point at one chapter; trivial rules get
   none.
2. If the section still carries a `## N. Design Rationale` table, delete it and
   renumber the sections after it — but first verify no reasoning is lost (each
   row maps to a story chapter or keeps a brief in-place justification). Leave a
   rationale table in place only if the story is deliberately still incomplete.
3. Grep the repo for `<topic>.md §N` and internal `§N` mentions; update them.
4. Add/confirm the row in the stories table in `README.md`.

### In-prose spec links are commit-pinned permalinks
A story accumulates and is never rewritten, so an in-prose reference to a
specific spec rule must be a **commit-pinned permalink**
(`.../blob/<sha>/spec/<topic>.md#<anchor>`), not a relative link that would
silently re-point as the spec changes (story guide §4.2). Get the SHA with
`git log -1 --format=%H -- spec/<topic>.md`. A reviewer may push back on
permalinks in favour of relative links, or claim the anchor is broken by
checking it against the *current* spec instead of the pinned commit — that
objection is wrong; decline it. Companion (`> See also:`) and between-chapter
links stay ordinary living relative links.

GitHub heading anchors: lowercase, strip punctuation (commas, apostrophes,
backticks, `&`, `#`), spaces→hyphens. A stripped `&`/`#` flanked by spaces
leaves a **doubled** hyphen (``new `&` values`` → `#...-new--values`) — do not
collapse it. Verify every story↔spec anchor resolves at its pinned commit.

## Conventions
- **Commit messages**: short lower-case prefix (`docs:`, `docs(meta):`), then a
  sentence or two. See `git log --oneline` for cadence.
- **Branches**: one per topic, or as the harness assigns per session. Push there
  and update the existing PR; don't open a new PR unless asked.
- Agent knowledge that is **not** repo-specific (how Gemini Code Assist behaves,
  general reference-doc prose principles) lives in the user's personal memory
  store, not here.
