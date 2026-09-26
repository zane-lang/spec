# Working on zane-lang/spec (agent notes)

This repo is the canonical Markdown specification for the **Zane** programming
language — documentation, not code (no compiler here). It holds two parallel doc
trees with opposite update rules, `spec/` and `stories/`; each tree's rules live
in its guide below.

This file holds only what an **agent** needs that the human docs don't already
cover. Everything normative lives in those guides and in `spec/` itself, and they
win over anything here — **read the relevant one in full before editing**:

- `contributing/writing-spec-docs.md` — spec prose, document shape, cross-references.
- `contributing/writing-stories-docs.md` — story prose, the append-only rule (§5),
  spec↔story linking (§4).
- `contributing/naming-terms.md` — coining a term of art.
- `README.md` — the real topic index (spec docs are **not** alphabetical); read
  it first when locating anything.

If you find this file *explaining* a rule rather than pointing at its canonical
home, that is a defect — delete the explanation and cite the home instead. A
restatement here drifts exactly like a glossary entry does (below), and is
harder to catch, because nothing in the repo greps this file.

Each session starts cold with no memory of prior ones, so this file is how the
next agent gets up to speed — keep it to durable, agent-facing facts.

## Before you edit

1. Read `README.md` to locate the topic, then **read the target doc in full** —
   editing one section ripples into cross-references elsewhere.
2. When a change touches the **type system**, single-pass self-review has missed
   internal contradictions on this codebase before: re-read the *un-updated*
   spec files and `bench/zane_bench.c` against the new design before opening a
   PR, not just the file you changed.
3. **A rule lives in more than one place. Changing its canonical home leaves
   every echo stale**, and an echo that still reads fluently is what ships a
   self-contradicting spec. This is the single most common defect on this repo:
   three glossary drifts, and on one PR three more outside the glossary — an
   `adt.md` summary-table row, a restatement two paragraphs below the paragraph
   being edited *in this file*, and a story chapter the same branch was adding.
   Two of the three were caught by review rather than by the author.

   So after editing any normative rule, sweep for the **form**, not the concept.
   Grep the old syntax itself — the retired bracket, the retired keyword, the
   retired spelling — across `spec/`, `README.md`, `contributing/`, this file,
   and any story chapters your own branch adds. The concept name will not find
   these; the old form will. Then read these five places specifically, because
   each has hosted a drift:

   - `spec/glossary.md` — the entry for the term (see below).
   - The edited doc's own **summary table** at the foot, and its **§1 overview
     bullets** at the head. Both restate rules the body owns.
   - Any **other doc's** summary row mentioning the construct.
   - **This file**, when the rule is one of the guards.
   - **Draft story chapters on your branch** — merged ones are append-only and
     correctly keep the old form, but a chapter your branch is adding must not
     ship displaying syntax the same branch retires.

   The glossary half of this is the oldest and has its own rule: **a rule
   correction is not done until `glossary.md` carries it.** Grep
   `spec/glossary.md` for the concept and update the entry in the same commit.
   Entry style — including the length that keeps an entry from drifting — is
   spec guide §2.6; what follows is only how the three drifts got past their
   authors.

   Sweeping the entry is necessary and **not sufficient**. Two of the three
   glossary drifts were subtler than a missed grep:

   - An entry rewritten in the *same commit* as the rule still shipped a false
     sentence, because the author checked that the new sentences were right
     rather than that the old ones still were. When you rewrite an entry, the
     sentences you did **not** touch are the ones to re-read — their meaning is
     set by the ones you did. A sentence scoped to a rule that has since been
     merged into another (`"never re-checked"`, true of one rule among several,
     false once that rule became the only one) reads unchanged and is now wrong.
   - The entries that drifted were the longest in the file, because length is the
     mechanism: an entry reproducing its rule's structure is what silently stops
     matching it.

   When trimming an entry, check *why* a clause is there before cutting it —
   `git log -S` on its wording will say. Shortening §3.42 reintroduced an
   ambiguity that the round before had added a clause specifically to close (a
   type's own `&` field is reached after **zero** owning edges, so "reachable by
   following owning edges" can be read as excluding it). A clause that looks
   like padding is sometimes a previous reviewer's fix.

## The `bench/` harness

`bench/` is a reference **C** harness for runtime experiments — **not** Zane
source; never treat its C as Zane. It *models* the memory design
(`spec/memory.md`), so when that design changes the harness is updated to track
it. The `.c` carries **no explanatory comments** (it holds no prose voice) —
keep it to code plus the labels passed to `record_test()`/`record_row()`.

The harness **emits JSON on stdout** and prints nothing else; no tool parses
formatted text. The pipeline is:

| file | role |
| --- | --- |
| `zane_bench.c` | the harness; `record_test`/`record_row` collect, `emit_json` writes |
| `benchmeta.py` | per-test `TEST_META` and chart colours — reader-facing prose, not code comments |
| `template.html` | the page skeleton, with a `__TESTS_JSON__` placeholder |
| `explanations.txt` | result interpretation, one `[Test N]` block each |
| `zane_bench_results.json` | the pinned measurements — the artifact to preserve |
| `zane_bench_results.txt`, `benchmark.html` | both **generated**; never hand-edit |

`python3 runbench.py` compiles, runs, and renders the page from what it just
measured, **without** touching the committed JSON; `--save` is the separate act
of pinning a run, and `--from-file` re-renders from the committed JSON without
measuring at all.

That split exists because `explanations.txt` quotes the pinned numbers, so
replacing them silently invalidates every note in the file. The committed run
was taken under WSL2 with no core pinning, on a hybrid CPU — 8 performance
cores and 16 efficiency ones — which is why T12's four workers spread 4.5x
across their twenty passes and why that row and T8 are the two whose absolute
figures do not reproduce elsewhere. **Do not pin a run from a container.**

A row whose number is known-bad is corrected in place instead: drop the field
and set `provenance_note` on the test, which `runbench.py` prints on every
render.

## Validate before committing (spec edits)

The greps below are guards, not rules: each one hunts a *form* the spec has
retired, and the rule it guards is stated in the spec, not here. Read the
canonical home before deciding what a hit means.

**Retired generics forms.** The generics system was unified into a
`<>`-header / `()`-call model (`spec/generics.md`, casing in `spec/lexical.md`).
None of these should hit:

```sh
grep -RIn -E "Array\[|\[size\]|Array[0-9]+|Matrix10|\[rows\]|\[cols\]|inferred type generic|type-parameter symbol|root form|'[A-Z]|@concepts[$](Number|Integer|Decimal|Text)\b|[a-z] Number\b" spec/
```

`@concepts[$]Number` and `[a-z] Number\b` catch the single numeric-literal
concept and the `Number` parameter concept, both retired when integer and
float literals split (`spec/lexical.md` §7, `spec/generics.md` §3.3). A
session writing `n Number` from memory is the likely reintroduction.
`Integer`, `Decimal`, and `Text` are the literal concepts' names before they
were renamed (`spec/syntax.md` §2.8), so `n @concepts$Integer` is the same
mistake one release later. The `$` sits in a
bracket because inside double quotes the shell strips a backslash from `\$`,
and `grep -E` then reads a bare `$` as an end-of-line anchor.

`'[A-Z]` catches the retired **borrow** type marker (`'Node`), which lived for
one release; a reference-type parameter now has exactly the two modes of
`spec/memory.md` §2.9. Do **not** widen it to a bare `'`: `'` now prefixes the
loose form of a binary operator (`spec/operators.md` §3.1), which is always `'`
followed by punctuation, so the uppercase class still separates the two.

The only legitimate stray `<...>` is `Result<T, E>` in `spec/error-handling.md`
— Rust's type named as a comparison, not Zane's.

Run this one with `-R` on the directory, not a `spec/*.md` glob plus a bare
directory argument: `grep` prints `bench/: Is a directory` and silently skips it
otherwise.

There used to be a guard here matching `&X = bareSymbol`. It is **gone** and
must not be restored: a bare symbol is a guest source (`spec/memory.md` §2.8),
so a match indicates nothing either way — what governs such an assignment is the
scope comparison in `spec/lifetimes.md` §1.1, which needs the declaration scopes
of both sides and so cannot be grepped at all.

**`receiver`.** The object a method is called on is the **subject** (canonical
home `spec/functions.md` §2.1, glossary §3.38); `receiver` was Smalltalk residue
and was renamed throughout `spec/`.

```sh
grep -RIn "receiver" spec/
```

The single expected hit is the `> **Story:**` pointer in `functions.md` §2.1
naming the chapter "What does a receiver receive?" — a chapter heading keeps the
old word because that is what the chapter is about. Any other hit is a
reintroduction; fix it. Merged stories say "receiver" throughout and stay that
way, so the two trees disagree on this word by design. Use `subject` in new
prose on both sides.

**The separator.** The bracket picks the separator (canonical home
`spec/lexical.md` §6). `init{ }` and the field-constructor header and call site
took `,` under the previous rule, and every other C-family language still does,
so those are the forms a session is most likely to write back. Both come back
empty on the current spec:

```sh
grep -RIn "init{[^}]*," spec/
grep -RIn -E "\b[A-Z][A-Za-z0-9_]*(<[^>]*>)?\{[^}]*," spec/
```

Unlike the greps above, a hit here is **not automatically a defect** — read it
against `lexical.md` §6.1 before fixing it, because a `,` inside a single entry
is still legal there and the greps cannot see bracket depth. They are also
single-line only. That gap is deliberate: the obvious multi-line pattern (an
indented entry line ending in `,`) matches a multi-line `enum` body, which keeps
its commas by §6.2, so the guard would carry standing legitimate hits — the
thing the `&X = bareSymbol` guard was removed for being. When a change touches a
multi-line `{ }` body, check it by reading. For the same reason nothing here
sweeps `[ ]` at all.

**Stories are exempt from every grep here.** `stories/` records the language as
it was at each turn and is never rewritten to match the present spec, so a
session that "fixes" a merged chapter has violated the append-only rule, not
tidied up.

If a grep hits an old form, stop and rewrite it in the unified system. If a
cross-reference target moved (renumbered `§`), fix the reference in every doc
that uses it, then re-grep for the old numbers. If the change conflicts with
another file's section, fix the conflicting section or escalate it to the user
as a design call — don't paper over it with a footnote.

**Markdown formatting is machine-checked, and — unlike the greps — covers every
`.md` in the repo, `stories/` included.** Run it before committing any Markdown
change:

```sh
npx markdownlint-cli2 "**/*.md"
```

Committed `.markdownlint.jsonc` is the canonical statement of which rules are
on; `README.md` § "Markdown formatting" says why exactly one is. A fix here is
whitespace only, so it does not touch what a merged chapter says and the
append-only check stays quiet. CI runs the same command on every PR, so
a miss here comes back as a red check rather than a review comment.

## Writing a design story

Story coverage is **complete**: every spec doc has a story except `glossary.md`,
which gets none (spec guide §7). New stories are written for wholly new topics
only.

Read both contributing guides first, and read **`stories/generics.md`** as the
quality bar — dense, opinionated, long-form prose. Writing a story is two
halves: write the narrative, then integrate it into the spec. Story guide §8
lists the integration steps for a wholly new story and §4.4 gives the
`> **Story:**` pointer format; for a chapter appended to an existing story, spec
guide §8 carries the same obligation from the spec side. Don't skip the second
half — it is the one sessions forget.

### Append-only: the two ways a session gets it wrong

**Story guide §5 owns this rule** — what may be edited, the PR-versus-commit
distinction, the rare consolidation exception, and the `git diff origin/main`
check to run before every commit touching `stories/`. Read it there and run the
check it gives. This section adds only what sessions on this repo keep getting
wrong, in both directions:

- **Too loose.** Editing a merged chapter's prose to fix a retired claim. Say
  what stopped being true from the *new* chapter instead; the old chapter gains
  only the supersession note §5 allows, which unlike the prose stays editable. Caught in review, not by the author.
- **Forgetting the note.** A change that retires a merged chapter's claim owes
  that chapter a supersession note in the same PR, and the additions-only check
  cannot notice it is missing — grep the story for the retired claim yourself.
- **Too strict.** Refusing to touch chapters *your own branch* added, because
  they were already written. They are drafts until the PR merges — rewrite,
  reorder, and insert among them freely; a decision reached late in review often
  belongs before them. The check is quiet through all of that by design.

If the check is clean, you have not violated the rule, whatever your instinct
says.

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

### Pinning an in-prose spec link

Story guide §4.2 requires a commit-pinned permalink for an in-prose spec
reference, and §4.4 gives the anchor derivation. Two mechanical notes:

- Commit the spec change **first**, then get the SHA with
  `git log -1 --format=%H -- spec/<topic>.md`. Run before that commit, it
  returns the file's previous state — not the text the chapter describes.
- A reviewer may push back on permalinks in favour of relative links, or claim
  the anchor is broken by checking it against the *current* spec instead of the
  pinned commit. That objection is wrong; decline it.

## Conventions

- **Commit messages**: short lower-case prefix (`docs:`, `docs(meta):`), then a
  sentence or two. See `git log --oneline` for cadence.
- **Branches**: one per topic, or as the harness assigns per session. Push there
  and update the existing PR; don't open a new PR unless asked.
- Agent knowledge that is **not** repo-specific (how the PR review bot behaves,
  general reference-doc prose principles) lives in the user's personal memory
  store, not here.
