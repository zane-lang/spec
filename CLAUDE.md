# Working on zane-lang/spec (agent notes)

Canonical Markdown specification for the **Zane** language — documentation, not
code. Two doc trees with opposite update rules: `spec/` (rewritten to the
present) and `stories/` (append-only).

This file holds only what an **agent** needs that the human docs don't cover.
The guides and `spec/` win over anything here; **read the relevant one in full
before editing**:

- `contributing/writing-spec-docs.md` — spec prose, document shape, cross-references.
- `contributing/writing-stories-docs.md` — story prose, append-only rule (§5), spec↔story linking (§4).
- `contributing/naming-terms.md` — coining a term of art.
- `README.md` — the topic index (spec docs are **not** alphabetical); read it first to locate anything.

If this file *explains* a rule instead of pointing at its home, that is a
defect: delete the explanation and cite the home.

## Before you edit

1. Read `README.md`, then **the target doc in full**.
2. For **type-system** changes, re-read the *un-updated* spec files and
   `memorybench/zane_bench.c` against the new design before the PR; single-pass
   self-review has missed contradictions here.
3. **Changing a rule's home leaves every echo stale** — the most common defect
   on this repo, and reviewers caught most of them, not authors. After editing a
   normative rule, grep the **old form** (retired bracket, keyword, spelling)
   across `spec/`, `README.md`, `contributing/`, this file, and story chapters
   *your branch adds* — the concept name won't find it. Then read these five
   places, each of which has hosted a drift:
   - `spec/glossary.md` — the term's entry. A rule correction is not done until
     it carries it, in the same commit (entry style: spec guide §2.6).
   - the edited doc's **summary table** and **§1 overview bullets**.
   - any **other doc's** summary row mentioning the construct.
   - **this file**, when the rule is one of the guards.
   - **draft story chapters on your branch** (merged ones keep the old form).

   When rewriting a glossary entry, re-read the sentences you did **not**
   touch: a sentence scoped to a rule since merged into another reads unchanged
   and is now false. Before trimming a clause, `git log -S '<wording>'` it — it
   may be a reviewer's fix (a type's own `&` field is reached after **zero**
   owning edges; cutting that clause reintroduced the ambiguity it closed).

## The `memorybench/` harness

A reference **C** harness modelling `spec/memory.md` — **not** Zane source.
Update it when that design changes. Layout and flags: `memorybench/runbench.py`
docstring. Agent-only rules:

- The `.c` carries **no explanatory comments** — only code and the labels passed
  to `record_test()`/`record_row()`. It prints JSON on stdout and nothing else.
- `benchmeta.py` and `explanations.txt` hold the reader-facing prose.
  `zane_bench_results.txt` and `benchmark.html` are **generated**; never hand-edit.
- **Never `--save` from a container.** Pins come from the maintainer's machines,
  which differ (unchanged rows 1.9–5x apart), so compare rows within one run,
  never absolute figures across pins. `explanations.txt` quotes the pinned
  numbers, so re-pinning invalidates its notes. T8 and T12 (threaded) are noisiest.
- A known-bad row is corrected in place: drop the field, set `provenance_note`.

## The `langbench/` harness

Rules live in `langbench/README.md`. Same pinning rule as above (never `--save`
from a container); `langbench.html` is generated. Its Zane programs are
idiomatic `core` Zane, never written against intrinsics to flatter a time; a
test the compiler does not fold is reported as not folded.

## Validate before committing (spec edits)

Each grep hunts a *form* the spec has retired; the rule it guards lives in the
spec, not here — read the home before deciding what a hit means. Stories are
**exempt from every grep** (append-only; "fixing" a merged chapter violates it).

**Retired generics forms** (`spec/generics.md`, casing `spec/lexical.md`); no hits:

```sh
grep -RIn -E "Array\[|\[size\]|Array[0-9]+|Matrix10|\[rows\]|\[cols\]|inferred type generic|type-parameter symbol|root form|'[A-Z]|@concepts[$](Number|Integer|Decimal|Text)\b|[a-z] Number\b|'[*/+<>=~-]|@operators[$](and|or)\b" spec/
```

- `Number` / `Integer` / `Decimal` / `Text` as concepts: retired at the literal
  split (`spec/lexical.md` §7, `spec/generics.md` §3.3; names `spec/syntax.md`
  §2.8). A session writing `n Number` from memory is the likely reintroduction.
- `'[A-Z]` is the retired **borrow** marker (`spec/memory.md` §2.9);
  `'[*/+<>=~-]` the loose operators (`spec/operators.md` §2.4);
  `@operators[$](and|or)` the machine operations renamed when those words
  became reserved (`spec/lexical.md` §4.4). Never widen `'` to a bare `'`
  (English apostrophe).
- The `$` sits in a bracket because inside double quotes the shell strips `\$`
  and `grep -E` reads a bare `$` as an anchor.
- Only legitimate stray `<...>`: `Result<T, E>` in `spec/error-handling.md`.
- Run with `-R` on the directory, not a `spec/*.md` glob plus a directory.
- A guard for `&X = bareSymbol` was removed and must not return: it needs both
  sides' declaration scopes (`spec/lifetimes.md` §1.1) and cannot be grepped.

**`receiver`** (now **subject**: `spec/functions.md` §2.1, glossary §3.38):

```sh
grep -RIn "receiver" spec/
```

One expected hit: the `> **Story:**` pointer in `functions.md` §2.1 naming the
chapter "Renaming the receiver to the subject". Any other is a reintroduction.
Use `subject` in new prose on both sides; merged stories keep `receiver`.

**`guest`, `host`, lifetime `owner`** (glossary §3.32, §3.33, §3.43; spec guide
§6.6); each comes back empty:

```sh
grep -RIn -i -E "guest|\bhost" spec/ | grep -vF '> **Story:**' | grep -v "^spec/dependencies.md"
grep -RIn -E "(block|scope)s? owns?\b|owned by (the |its |that |this |an? )?(declaring |enclosing |body )?(block|scope)|owner comparison" spec/
grep -RIn -i -E "\breference (field|element|parameter|member|slot)s?\b" spec/
```

The first skips `> **Story:**` pointers (quoted old headings) and
`dependencies.md` (a host is a download server).

**The separator** (`spec/lexical.md` §6; `,` is what every other C-family
language does, so it is the likely slip). Both come back empty today:

```sh
grep -RIn "init{[^}]*," spec/
grep -RIn -E "\b[A-Z][A-Za-z0-9_]*(<[^>]*>)?\{[^}]*," spec/
```

A hit is **not automatically a defect** — a `,` inside a single entry is legal
(`lexical.md` §6.1) and the greps cannot see bracket depth. They are
single-line on purpose: a multi-line pattern would hit multi-line `enum` bodies
(§6.2) permanently. Check multi-line `{ }` bodies by reading; nothing sweeps `[ ]` brackets.

If a grep hits an old form, rewrite it in the unified system. If a `§` target
moved, fix every referrer and re-grep for the old numbers. If the change
conflicts with another file's section, fix that section or escalate it as a
design call — no footnote papering.

**Markdown formatting** is machine-checked over every `.md`, `stories/`
included (whitespace-only fixes don't touch merged claims). Run before
committing any Markdown change; CI runs the same:

```sh
devbox run lint
```

Intel macOS fallback and the rule itself: `README.md` § Markdown formatting.

## Writing a design story

Coverage is **complete**: every spec doc has a story except `glossary.md`
(spec guide §7). Write new ones for wholly new topics only. Read both guides
and **`stories/generics.md`** as the quality bar. A story is two halves —
narrative, then integration into the spec (story guide §8; pointer format §4.4;
for a chapter appended to an existing story, spec guide §8). The second half is
the one sessions forget. One session per story.

**Append-only** — story guide §5 owns the rule and the
`git diff --word-diff origin/main -- stories/<topic>.md` review to run before
every `stories/` commit. How sessions get it wrong:

- **Too loose:** editing a merged chapter's prose to fix a retired claim. Say
  what stopped being true in the *new* chapter; the old one gains only §5's
  supersession note.
- **Forgetting the note:** a change that retires a merged chapter's claim owes
  it a note in the same PR; the diff can't show a missing one — grep the story
  for the retired claim.
- **Too strict:** refusing to touch chapters *your own branch* added. They are
  drafts until merge — rewrite, reorder, insert among them freely.
- **Adjusting in passing:** rewording a merged chapter in a PR that also
  changes the language. §5 allows it only in a PR whose sole purpose is that.

### Interview the maintainer

The real thread — roads rejected, order of realization — lives only in the
maintainer's head and is often not what the spec suggests. Draft the obvious
chapters; **stop and ask whenever a decision's why isn't forced by the spec
text**. Offer your best-guess framing as options and expect to be told "that's
not how I thought about it" and have it replaced wholesale. On the narrative the
maintainer wins; on the rules the spec wins.

### Pinning an in-prose spec link

Rule and anchor derivation: story guide §4.2, §4.4. Mechanics:

- Commit the spec change **first**, then
  `git log -1 --format=%H -- spec/<topic>.md`. Run earlier, it returns the
  file's previous state.
- A reviewer who prefers relative links, or checks the anchor against the
  *current* spec instead of the pinned commit, is wrong; decline.

## Conventions

- **Commits**: short lower-case prefix (`docs:`, `docs(meta):`), then a sentence
  or two; see `git log --oneline`.
- **Branches**: one per topic, or as the harness assigns. Push there and update
  the existing PR; don't open a new one unless asked.
- Agent knowledge that is **not** repo-specific lives in the user's personal
  memory store, not here.
